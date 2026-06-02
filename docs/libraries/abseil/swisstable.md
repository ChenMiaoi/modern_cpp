---
title: Abseil SwissTable
topic: libraries
feature: swisstable
standard: N/A
status_checked_at: 2026-06-02
implementation:
  abseil:
    path: references/impl/abseil-cpp/absl/container/internal/raw_hash_set.h
    symbols:
      - raw_hash_set
      - ctrl_t
      - GroupSse2Impl
      - probe_seq
      - flat_hash_map
      - node_hash_map
exercises: []
solutions: []
---
# Abseil SwissTable：SIMD 加速的开放寻址哈希表

> 源码路径：`references/impl/abseil-cpp/absl/container/internal/raw_hash_set.h`

由 Matt Kulukundis 在 CppCon 2017 演讲 "Designing a Fast, Efficient, Cache-friendly Hash Table" 后为业界所知。SwissTable 是 Abseil `flat_hash_map`/`flat_hash_set` 的底层实现，其设计思想影响了众多高性能哈希表实现。

## 哈希值分割：H1/H2

对任意 key `x`，计算 `hash(x)` 后拆分为两部分：

- **H1**（低位）：直接使用完整哈希值，后续通过 `hash & (capacity - 1)` 取低位定位初始探测位置。因为 capacity 始终是 2 的幂，等价于取低 `log2(capacity)` 位。
- **H2**（高 7 位）：取 `hash >> (sizeof(size_t) * 8 - 7)`，即哈希值的最高 7 位，得到一个 7-bit 的标签值（`h2_t = uint8_t`，范围 `0x00`~`0x7F`），写入该 slot 对应的 control byte。H2 的作用是**预过滤**：在对 slot 执行真正的 `operator==` 之前，先用 1 字节的 H2 快速排除不可能匹配的 slot。7-bit H2 的假阳性概率为 `1/128`，实测中每次 `find` 的误判比较次数 < 1/8。

```cpp
// 源码：raw_hash_set.h
// H1 直接保留完整哈希值，后续 mask 操作取低位
inline size_t H1(size_t hash) { return hash; }
// H2 取最高 7 位作为标签
inline h2_t H2(size_t hash) {
    return static_cast<h2_t>(hash >> (sizeof(size_t) * 8 - 7));
}
```

## Control Byte 布局

SwissTable 将每个 slot 的元信息压缩为一个 `ctrl_t`（`enum class ctrl_t : int8_t`），与 slot 数据**分离存储**为平行数组：

```cpp
enum class ctrl_t : int8_t {
  kEmpty    = -128,  // 0b10000000  (0x80) — 未使用
  kDeleted  = -2,    // 0b11111110  (0xFE) — 墓碑（tombstone）
  kSentinel = -1,    // 0b11111111  (0xFF) — 迭代终止标记
};
// occupied slot: 0b0xxxxxxx — 最高位为 0，低 7 位是 H2
inline bool IsFull(ctrl_t c) {
  return static_cast<int8_t>(c) >= 0;  // MSB=0 即占用
}
inline bool IsEmpty(ctrl_t c) { return c == ctrl_t::kEmpty; }
inline bool IsDeleted(ctrl_t c) { return c == ctrl_t::kDeleted; }
inline bool IsEmptyOrDeleted(ctrl_t c) { return c < ctrl_t::kSentinel; }
```

这些值的选择有严格的约束（源码中通过 `static_assert` 强制）：

- **MSB=1** 的 `kEmpty`、`kDeleted`、`kSentinel` 让 SIMD 可以一次性通过 `_mm_cmpgt_epi8` 区分出"占用"（正数）和"特殊值"（负数）。
- `kSentinel = -1` 使得 SSE2 中可以用 `pcmpeqd xmm, xmm`（全 1 向量）零成本生成，无需从内存加载。
- `kEmpty = -128` 使得饱和减法 `_mm_subs_epi8` 可用于批量将 `kEmpty` 转换为 `kDeleted`（用于 rehash 时的 `ConvertSpecialToEmptyAndFullToDeleted`）。

## 内存布局全景

```
  hash(x) ──┬── H1 (高位, 模 capacity → bucket 索引)
             └── H2 (低 7-bit, 存入 ctrl byte 做快速标签)

  ┌─── BackingArray: 单次 malloc 分配 ──────────────────────────────────────────────┐
  │                                                                                  │
  │  ┌─── ctrl[] 平行数组 (1 byte/slot) ──────────────────────────────────────────┐  │
  │  │                                                                            │  │
  │  │  [0]    [1]    [2]    [3]    [4]    [5]         [C-2]  [C-1]  sentinel     │  │
  │  │  ┌────┬──────┬──────┬──────┬──────┬──────┬─ ─ ─┬──────┬──────┬────┐       │  │
  │  │  │0x3A│ 0x80 │ 0x7F │ 0xFE │ 0x12 │ 0x80 │     │ 0x44 │ 0x80 │0xFF│       │  │
  │  │  │ H2 │empty │ full │ tomb │ H2   │empty │     │ H2   │empty │sent│       │  │
  │  │  └────┴──────┴──────┴──────┴──────┴──────┴─ ─ ─┴──────┴──────┴────┘       │  │
  │  │                                                                            │  │
  │  │  后接: sentinel(1B) + clones[kWidth-1](15B) — 防止末尾 SSE2 越界读         │  │
  │  └────────────────────────────────────────────────────────────────────────────┘  │
  │                                                                                  │
  │  ┌─── slots[] 平行数组 (sizeof(T) bytes/slot) ────────────────────────────────┐  │
  │  │                                                                            │  │
  │  │  [0]         [1]         [2]              [C-2]       [C-1]                │  │
  │  │  ┌──────────┬──────────┬──────────┬─ ─ ─┬──────────┬──────────┐          │  │
  │  │  │ key+val  │ key+val  │ key+val  │     │ key+val  │ key+val  │          │  │
  │  │  │ "a"=95   │ "b"=87   │ "c"=72   │     │ "x"=61   │ "y"=44   │          │  │
  │  │  └──────────┴──────────┴──────────┴─ ─ ─┴──────────┴──────────┘          │  │
  │  │    ↑ ctrl[0]↔slot[0] 一一映射, 无需 next 指针                              │  │
  │  └────────────────────────────────────────────────────────────────────────────┘  │
  └──────────────────────────────────────────────────────────────────────────────────┘
```

**为什么 ctrl 和 slots 分离？** 16 个 control bytes 恰好装进一个 128-bit SSE 寄存器，一次 SIMD 操作即可完成探测。如果与 slot 交错，一个 64 字节 cache line 只能容纳少量 slot 的元数据。分离后，探测时只需加载 control bytes 到 SIMD 寄存器，绝大多数探测在第一个 `_mm_cmpeq_epi8` + `_mm_movemask_epi8` 后即可确定是否命中或需要继续。

`clones[kWidth - 1]` 是 `ctrl` 数组前 `kWidth - 1` 个字节的拷贝。当探测位置靠近 `ctrl` 数组末尾时，GroupSse2Impl 的 16 字节加载会"绕回"到数组开头——`clones` 数组提供了这段环绕数据的合法副本，避免越界读。

## SSE2 探测核心（GroupSse2Impl）

```cpp
struct GroupSse2Impl {
  static constexpr size_t kWidth = 16;
  using MaskInt = uint32_t;
  using BitMaskType = BitMask<MaskInt, kWidth>;

  __m128i ctrl;

  explicit GroupSse2Impl(const ctrl_t* pos) {
    ctrl = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pos));
  }

  // 匹配 H2：广播 h2 到 16 个 lane，逐字节比较，提取匹配位图
  BitMaskType Match(h2_t hash) const {
    auto match = _mm_set1_epi8(static_cast<char>(hash));
    return BitMaskType(MoveMask(_mm_cmpeq_epi8(match, ctrl)));
  }

  // 检测空 slot
  NonIterableBitMaskType MaskEmpty() const {
    return NonIterableBitMaskType(
        MoveMask(_mm_cmpeq_epi8(ctrl, _mm_set1_epi8(
            static_cast<char>(ctrl_t::kEmpty)))));
  }

  // 检测 empty 或 deleted（MSB=1 的特殊值）
  NonIterableBitMaskType MaskEmptyOrDeleted() const {
    auto special = _mm_cmpgt_epi8_fixed(
        _mm_setzero_si128(), ctrl);  // 0 > ctrl 即 ctrl < 0
    return NonIterableBitMaskType(MoveMask(special));
  }
};
```

**`Match()` 完整流程**：

```
  待查找 key 的 H2 = 0x3A
  _mm_set1_epi8(0x3A)  广播到 16 个 lane
       │
       ▼
  ┌────────────────────────────────────────────────────────────────────┐
  │  match 寄存器:                                                     │
  │  [3A][3A][3A][3A][3A][3A][3A][3A][3A][3A][3A][3A][3A][3A][3A][3A]│
  └────────────────────────────────────────────────────────────────────┘
                          ⊕  逐字节比较 (_mm_cmpeq_epi8)
  ┌────────────────────────────────────────────────────────────────────┐
  │  ctrl[pos..pos+15]:                                                │
  │  [3A][80][7F][FE][12][80][05][80][3A][80][44][FE][80][80][80][FF]│
  └────────────────────────────────────────────────────────────────────┘
                          │
                          ▼
  ┌────────────────────────────────────────────────────────────────────┐
  │  比较结果: [FF][00][00][00][00][00][00][00][FF][00][00][00][00]..│
  │            匹配   ≠    ≠    ≠    ≠    ≠    ≠    ≠  匹配          │
  └────────────────────────────────────────────────────────────────────┘
                          │
                          ▼
           _mm_movemask_epi8() 提取 MSB → 16-bit bitmask
  ┌─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┐
  │1│0│0│0│0│0│0│0│1│0│0│0│0│0│0│0│  = 0x0104
  └─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┘
   ↑                             ↑
   slot 0: H2 匹配               slot 8: H2 匹配
   → 检查 operator==              → 检查 operator==

  BitMask 遍历: while (mask) { bit = blsr(mask); slot_idx = ctz(bit); ... }
```

## 探测序列（probe_seq）

```cpp
// 源码：raw_hash_set.h
class probe_seq {
  size_t mask_;    // capacity - 1
  size_t offset_;  // 当前探测偏移（模 capacity）
  size_t index_;   // 累积步长（0, kWidth, 2*kWidth, 3*kWidth, ...）

 public:
  probe_seq(size_t hash, size_t mask)
      : mask_(mask), offset_(hash & mask), index_(0) {}

  size_t offset() const { return offset_; }
  size_t offset(size_t i) const { return (offset_ + i) & mask_; }

  void next() {
    index_ += kWidth;                    // 步长递增一个 Group 宽度
    offset_ = (offset_ + index_) & mask_; // 累积偏移
  }
};
```

探测序列为三角数增量：第 N 步偏移 `N * kWidth`（SSE2 下 16）。例如 capacity=64 时，探测序列为 0, 16, 48, 96%64=32, ...。步长始终是 Group 宽度的倍数，保证每个 Group 都被探测到，且不会陷入固定步长的循环。

## 加载因子与 rehash

- **大表**（capacity > kWidth=16）：最大加载因子 **7/8**（87.5%）。
- **小表**（capacity <= kWidth）：允许加载因子达到 1.0（所有 slot 填满），因为整个表只有一个 Group，SSE2 一次性完成探测。
- `growth_left` 字段跟踪"当前容量下还能插入多少元素"。

Rehash 时只搬移 occupied 的 slot（`IsFull(ctrl[i])`），EMPTY 和 DELETED 都被丢弃——新表的 control bytes 全部初始化为 `kEmpty`，墓碑被自然清理。

## 墓碑（Tombstone）处理与擦除优化

`erase` 不简单地将 control byte 设为 `kEmpty`。源码中的关键优化：

```
如果被擦除 slot 所在 Group 内存在其他 EMPTY slot，
则可以安全地将该 slot 标记为 EMPTY（而非 DELETED）。
```

理由：如果一个 Group 内有 EMPTY 位，说明该 Group 从未被完全填满过。`find` 的探测序列遇到有 EMPTY 的 Group 就会停止，因此被擦除的 slot 不可能出现在其他 key 的探测路径上，标记为 EMPTY 可以让后续探测更早终止。

如果 Group 内没有 EMPTY，则必须标记为 `kDeleted`（墓碑），因为可能有 key 的探测链跨越了该 slot。

## 小对象优化（SOO）

现代 Abseil 还实现了 Small Object Optimization：当 value_type 的大小/alignment 足够小且 capacity 也很小时，整个哈希表内联存储在 `raw_hash_set` 对象本身中，完全避免堆分配。

## flat_hash_map vs node_hash_map

```cpp
#include "absl/container/flat_hash_map.h"

absl::flat_hash_map<std::string, int> scores;
scores["alice"] = 95;

// 当 key/value 较大或需要指针稳定性时，用 node_hash_map
absl::node_hash_map<std::string, std::vector<int>> big_values;
```

| 特性 | `flat_hash_map` | `node_hash_map` |
|------|----------------|----------------|
| 存储方式 | 内联（key+value 直接在 slot 中） | 独立节点分配 |
| 缓存友好 | ✓（连续内存） | ✗（指针追逐） |
| 迭代器稳定性 | ✗（插入/删除后失效） | ✓ |
| 引用稳定性 | ✗ | ✓ |
| 适用场景 | 大多数场景 | 需要引用稳定性 |

## 与 libstdc++ `_Hashtable` 的差异

libstdc++ 的 `std::unordered_map` 使用节点式 `_Hashtable`（bucket + `_Hash_node` 链表），而非开放寻址。以下是 Abseil SwissTable 与 libstdc++ `_Hashtable` 的关键差异：

| 维度 | Abseil SwissTable | libstdc++ `_Hashtable` |
|------|-------------------|----------------------|
| 内存布局 | 开放寻址：ctrl 平行数组 + 连续 slot 数组 | 链式：bucket 数组 + `_Hash_node` 链表 |
| 缓存行为 | 连续内存，SIMD 探测 | 指针追逐，缓存不友好 |
| 负载因子 | 7/8（87.5%） | 1.0（默认） |
| 哈希分割 | H1/H2 分离，7-bit 标签预过滤 | 无预过滤 |
| 迭代器稳定性 | ✗（rehash 后失效） | ✓（节点独立分配） |
| 引用稳定性 | ✗ | ✓ |
| 适用场景 | 高性能查询密集场景 | 需要迭代器/引用稳定性的场景 |

## 用户 API

本文对应的用户侧容器主要是 `absl::flat_hash_map` / `absl::flat_hash_set`；现有正文已经直接下钻到它们共享的底层 `raw_hash_set`。

## 标准语义

Abseil SwissTable 容器（`flat_hash_map`、`flat_hash_set`、`node_hash_map`、`node_hash_set`）**不是** C++ 标准容器——它们不出现在任何 C++ 标准版本中，也不遵循标准的 iterator 稳定性/异常安全契约。Abseil 将它们定位为 `std::unordered_*` 的**高性能 drop-in 替代**，API 大体兼容但有明确差异：

**与标准 `unordered_*` 的重合面**：

- 相同的核心接口：`find`、`insert`、`emplace`、`erase`、`operator[]`、`count`、`contains`（C++20 才进入标准）、`equal_range`。
- 相同的类型别名：`key_type`、`mapped_type`、`value_type`、`size_type`、`allocator_type`。
- 支持 `Allocator` 模板参数，遵循 `propagate_on_container_*` trait。
- 支持从 `std::initializer_list` 和迭代器范围构造。

**与标准的关键差异**：

| 维度 | 标准 `unordered_map` | Abseil `flat_hash_map` |
|------|---------------------|----------------------|
| `erase(iterator)` 返回值 | `iterator`（指向下一元素） | `void`（需用 `erase(it++)` 获取下一迭代器） |
| `bucket_count()` / `bucket_size()` | 有，暴露 bucket 结构 | 无 bucket 概念，提供 `capacity()` |
| `max_load_factor()` 读写 | 可读可写 | 不暴露（固定 7/8） |
| 迭代器稳定性 | 插入不使迭代器失效 | **所有插入均可能使迭代器失效** |
| 异常安全 | 基本异常安全保证 | **不保证异常安全**（源码头注释明确声明） |
| 异构查找 | C++20 起支持透明 Hash/Eq | 从一开始就支持（`is_transparent`） |
| `reserve()` 语义 | 保留 n 个 bucket | 保留 n 个元素容量（无桶概念） |

**Abseil 扩展能力**：

- **异构查找**：当 `Hash` 和 `Eq` 均定义 `is_transparent` 时，`find`、`erase`、`count`、`operator[]` 均接受非 `key_type` 参数，避免临时构造 key 对象。这是 Abseil 的核心卖点之一，标准直到 C++20 才部分支持。
- **`capacity()`**：暴露表的内部容量（slot 总数），可用于预估内存占用。
- **`erase(begin(), end())` 特殊语义**：等价于 `clear()` 但保留已 `reserve` 的容量，不缩减分配。
- **`ABSL_ATTRIBUTE_OWNER`**：标记为 owner 类型，帮助编译器 lifetime 分析检测 use-after-move。

## 对象布局

上文已经覆盖 ctrl bytes、slots、sentinel 与 clones 的布局；后续补一张按字节偏移展开的 `BackingArray` 视图。
## 核心源码路径

核心实现文件的源码路径（均相对于 `references/impl/abseil-cpp/`）：

**用户侧头文件**（薄封装，直接 `#include` 下方内部头文件）：

```
absl/container/flat_hash_map.h    → absl::flat_hash_map
absl/container/flat_hash_set.h    → absl::flat_hash_set
absl/container/node_hash_map.h    → absl::node_hash_map
absl/container/node_hash_set.h    → absl::node_hash_set
```

**内部实现**（`absl/container/internal/`）：

```
raw_hash_set.h              — 核心：raw_hash_set<Policy, ...> 模板类、probe_seq、
                              find_or_prepare_insert、emplace_at、erase_meta_only
raw_hash_set.cc             — 类型擦除函数：PrepareInsertLarge、EraseMetaOnlyLarge、
                              WasNeverFull、ResetCtrl、FindNewPositionsAndTransferSlots
raw_hash_map.h              — raw_hash_map<Policy, ...>，在 raw_hash_set 上叠加
                              operator[]/at/try_emplace 等 map 接口
hashtable_control_bytes.h   — ctrl_t 枚举、GroupSse2Impl/GroupPortableImpl、
                              BitMask、NonIterableBitMask、IsFull/IsEmpty/IsDeleted
node_slot_policy.h          — node_slot_policy：node_hash_* 的 slot 策略，
                              存储 T* 而非 T 本身
hash_policy_traits.h        — hash_policy_traits<Policy>：slot construct/destroy/transfer 的 trait 层
container_memory.h          — 分配器辅助、CompressedPair
layout.h                    — BackingArray 的字节偏移计算（RawHashSetLayout）
hashtablez_sampler.h/cc     — 运行时采样（HashtablezInfo），提供 alloc_count、probe_length 等指标
raw_hash_set_resize_impl.h  — 重分配的具体实现（GrowToNextCapacity 等）
```

**调用链**（以 `flat_hash_map::find` 为例）：

```
flat_hash_map<K,V>::find(key)
  → raw_hash_map<FlatHashMapPolicy<K,V>, ...>::find(key)
    → raw_hash_set<...>::find(key)
      → find_large(key, hash_of(key))           // 大表路径
        → probe(common(), hash)                  // 构造 probe_seq
        → Group(ctrl + offset).Match(H2(hash))   // SIMD 探测
        → equal_to(key, slot)                     // 逐候选比较
```


## 核心类 / 函数

```cpp
// ─── 核心数据结构 ───────────────────────────────────────────────────────────

// 控制字节枚举（hashtable_control_bytes.h）
enum class ctrl_t : int8_t {
  kEmpty    = -128,  // 0x80 — 未使用
  kDeleted  = -2,    // 0xFE — 墓碑
  kSentinel = -1,    // 0xFF — 迭代终止标记
};

// 状态判断函数（hashtable_control_bytes.h）
inline bool IsFull(ctrl_t c);           // MSB=0 → 占用（H2 值 0x00~0x7F）
inline bool IsEmpty(ctrl_t c);          // c == kEmpty
inline bool IsDeleted(ctrl_t c);        // c == kDeleted
inline bool IsEmptyOrDeleted(ctrl_t c); // c < kSentinel（即 MSB=1 且非 sentinel）

// ─── SIMD 探测组 ───────────────────────────────────────────────────────────

// SSE2 实现（hashtable_control_bytes.h）
struct GroupSse2Impl {
  static constexpr size_t kWidth = 16;         // 每组 16 个 slot
  using MaskInt = uint32_t;                    // 16-bit bitmask 用 uint32_t 以便 blsr
  using BitMaskType = BitMask<MaskInt, kWidth>;

  __m128i ctrl;
  explicit GroupSse2Impl(const ctrl_t* pos);   // _mm_loadu_si128 加载 16 字节
  BitMaskType Match(h2_t hash) const;          // 广播 H2 → _mm_cmpeq_epi8 → bitmask
  NonIterableBitMaskType MaskEmpty() const;    // 匹配 kEmpty
  NonIterableBitMaskType MaskEmptyOrDeleted() const; // ctrl < 0（_mm_cmpgt_epi8_fixed(0, ctrl)）
};

// NEON / Portable 实现已省略，结构相同

// ─── 探测序列 ─────────────────────────────────────────────────────────────

template <size_t Width>
class probe_seq {
  HashtableCapacity capacity_;
  size_t offset_;   // 当前探测偏移
  size_t index_ = 0;
public:
  probe_seq(HashtableCapacity capacity, size_t hash);
  size_t offset() const;              // 当前 Group 起始偏移
  size_t offset(size_t i) const;      // Group 内第 i 个 slot 的绝对偏移
  void next();                        // index_ += Width; offset_ += index_; mask
  size_t index() const;               // 累积步长（0, 16, 48, 96, ...）
};
```

```cpp
// ─── raw_hash_set：瑞士表的完整实现 ─────────────────────────────────────────
// 源码：raw_hash_set.h
template <class Policy, class... Params>
class raw_hash_set {
  // 类型别名
  using PolicyTraits = hash_policy_traits<Policy>;
  using slot_type    = typename PolicyTraits::slot_type;
  using iterator     = ...;   // 包含 ctrl_t* + slot_type* + generation_ptr

  // ─── 查找 ───────────────────────────────────────────────────────────────
  iterator find(const key_arg<K>& key);                // 入口：分发 small/large
  iterator find_small(const key_arg<K>& key);          // 小表：线性扫描 single_slot
  iterator find_large(const key_arg<K>& key, size_t hash); // 大表：SIMD 探测循环

  // ─── 插入 ───────────────────────────────────────────────────────────────
  std::pair<iterator, bool> emplace(Args&&... args);   // 入口
  std::pair<iterator, bool> find_or_prepare_insert(const K& key);
    // 返回 {迭代器, 是否需要插入}
    // → find_or_prepare_insert_small  // 小表
    // → find_or_prepare_insert_soo    // SOO 路径
    // → find_or_prepare_insert_large  // 大表：SIMD 探测 + 负载检查
  void emplace_at(iterator iter, Args&&... args);      // 在 prepare 的位置就地构造

  // ─── 擦除 ───────────────────────────────────────────────────────────────
  void erase_meta_only(const_iterator it);             // 仅更新 ctrl byte
  void erase_meta_only_large(const_iterator it);       // → EraseMetaOnlyLarge()
    // 内部调用 WasNeverFull() 判断是否可以标记 kEmpty 而非 kDeleted

  // ─── 重分配 ─────────────────────────────────────────────────────────────
  void rehash(size_t n);                               // → Rehash() 类型擦除版本
  void reserve(size_t n);                              // → ReserveTableToFitNewSize()
  // 具体实现：FindNewPositionsAndTransferSlots() 遍历全表，逐 slot 重新探测定位
};
```

```cpp
// ─── 策略层：flat vs node ─────────────────────────────────────────────────

// FlatHashMapPolicy（raw_hash_map.h / container_memory.h 内定义）
//   slot_type = std::pair<const K, V>  — 值直接内联在 slot 中
//   element(slot) → T&
//   construct(alloc, slot, args...) → placement new

// NodeHashMapPolicy（node_slot_policy.h）
//   slot_type = T*  — slot 存储堆分配节点的指针
//   element(slot) → **slot（解引用指针）
//   construct(alloc, slot, args...) → Policy::new_element(alloc, args...)
//   transfer 用 memcpy（指针复制），节点本身不搬移

// hash_policy_traits<Policy>（hash_policy_traits.h）
//   统一 trait 层，提供 construct / destroy / transfer / element / value 等静态方法
//   Policy::transfer_uses_memcpy() — flat=true（trivially relocatable 时），node=true（复制指针）
```

## 关键算法

### 查找（`find`）算法路径

```
find(key):
  1. 计算 hash = hash_of(key)，拆分 H1(hash) 和 H2(hash)
  2. 预取 ctrl 数组所在的堆块（prefetch_heap_block）
  3. 用 probe_seq 从 H1(hash) & mask 开始探测
  4. 每步加载 16 字节 ctrl → Group(g)
  5. g.Match(h2) → 对每个 H2 匹配位：
       if (equal_to(key, slot[offset(i)])) → 返回迭代器（命中）
  6. g.MaskEmpty() → 非空？→ 说明此 key 不存在，返回 end()
  7. 遇到墓碑（kDeleted）→ 不停止，继续 seq.next() 到下一 Group
  8. 重复 4-7 直到命中或遇到 EMPTY
```

**关键性质**：查找遇到 kDeleted 不停止——只有 kEmpty 才终止探测。这是因为被删除的 slot 可能处于某个存在 key 的探测链中间。

### 插入（`find_or_prepare_insert`）算法路径

```
emplace(args...):
  1. decompose args → key
  2. find_or_prepare_insert(key) → {iter, need_insert}
     a. 先走一遍 find 逻辑（SIMD 探测）
     b. 若找到 → return {找到的迭代器, false}
     c. 若未找到：
        - 检查 growth_left > 0？
          是 → PrepareInsertLarge：
            在第一个有 EMPTY slot 的 Group 中找一个空位（优先复用 kDeleted 位）
            → 更新 ctrl byte 为 H2，递减 growth_left
          否 → 触发 rehash/grow：
            GrowToNextCapacity → 分配 2x 数组 → 逐 slot 重探测搬移
            → 在新表中 PrepareInsert
     return {新位置迭代器, true}
  3. emplace_at(iter, args...) → 就地构造 slot
```

**插入复用墓碑**：`PrepareInsertLarge` 在找到 EMPTY 的 Group 后，会在该 Group 内搜索第一个非 Full 位（EMPTY 或 DELETED 都可以），优先复用墓碑位置，减少不必要的 rehash。

### 擦除（`erase`）算法路径

```
erase(key):
  1. it = find(key) → 未找到则返回 0
  2. destroy(slot) → 析构 key+value
  3. erase_meta_only(it) → 更新 ctrl byte：
     a. WasNeverFull(ctrl, index)?
        检查被删 slot 前后各一个 Group 是否有 EMPTY：
        - 若前后 Group 都有 EMPTY 且连续非空区间 < kWidth
          → 说明该 Group 从未满过，find 必定在此停下
          → 标记为 kEmpty（不是 kDeleted），节省墓碑
        - 否则 → 标记为 kDeleted（墓碑）
     b. 更新 growth_info：OverwriteFullAsEmpty 或 OverwriteFullAsDeleted
  4. 返回 1
```

### Rehash 算法路径

```
rehash(n) / reserve(n):
  1. 计算目标容量 new_cap = NormalizeCapacity(n)
  2. 若 new_cap <= current_capacity → 可能仍需 rehash 清除墓碑
  3. 分配新 BackingArray（2x 或按需）
  4. 初始化新 ctrl[] 全为 kEmpty + sentinel + clones
  5. FindNewPositionsAndTransferSlots：
     遍历旧表所有 IsFull(ctrl[i]) 的 slot：
       hash = hash_slot(slot)
       TryFindNewIndexWithoutProbing → 尝试快速定位（无需完整探测）
       若失败 → find_first_non_full(hash) 完整探测新表
       transfer(new_slot, old_slot)  // flat: memcpy/move, node: 复制指针
       SetCtrl(new_ctrl, new_idx, h2)
  6. 释放旧 BackingArray
  7. 重算 growth_left
```

**Rehash 清除墓碑**：rehash 时只搬移 `IsFull` 的 slot，kEmpty 和 kDeleted 全部丢弃。新表 ctrl 全部初始化为 kEmpty，墓碑被自然清理。这是处理大量 erase 后性能退化的唯一方式（`erase` 不会触发 rehash）。

## ABI 约束

Abseil 容器**没有稳定的 ABI**——这与 libstdc++/libc++ 对 `std::unordered_map` 提供的跨版本稳定符号保证截然不同。

**Abseil 的 ABI 策略**：

- **头文件内联优先**：`raw_hash_set`、`GroupSse2Impl`、`probe_seq` 等核心类型均为模板，全部在头文件中定义。绝大部分逻辑（`find`、`emplace`、`erase`）被编译器内联到调用点，不生成跨 TU 的符号。
- **类型擦除减少膨胀**：为了控制代码膨胀，部分不依赖模板参数的操作被提取为类型擦除的函数（定义在 `raw_hash_set.cc`）：`PrepareInsertLarge`、`EraseMetaOnlyLarge`、`Rehash`、`Copy`、`ClearBackingArray` 等。这些函数通过 `PolicyFunctions` 结构体（包含函数指针和类型元数据）获取类型信息。
- **无跨库稳定 ABI 承诺**：Abseil 的版本策略是**源码兼容**而非二进制兼容。升级 Abseil 版本后需要全量重新编译。源码注释明确指出：*"Using `flat_hash_map` at interface boundaries in dynamically loaded libraries (e.g. .dll, .so) is unsupported due to the way `absl::Hash` values may be randomized across dynamically loaded libraries."*
- **`absl::Hash` 随机化**：`absl::Hash` 的结果在不同进程（甚至不同 .so）间可能不同（per-process seed），这意味着跨动态库传递 SwissTable 实例不仅 ABI 不稳定，语义也不正确。
- **模板实例化控制**：`raw_hash_set` 的模板参数组合（`Policy, Hash, Eq, Alloc`）决定最终实例化的类型。`ABSL_ATTRIBUTE_OWNER` 等属性不影响 ABI，但影响编译器的静态分析。

**与标准库对比**：

| 维度 | libstdc++ `std::unordered_map` | Abseil `flat_hash_map` |
|------|-------------------------------|----------------------|
| ABI 稳定性 | 有（libstdc++.so 中有符号） | 无（头文件模板，无稳定 ABI） |
| 跨 .so 使用 | 可以 | **不支持** |
| 版本升级 | 可二进制兼容 | 必须全量重编译 |
| 符号导出 | `_Hashtable` 相关符号 | 类型擦除函数符号（内部，非公开 ABI） |

## 异常安全

Abseil SwissTable 容器**不提供异常安全保证**——源码头注释明确声明：*"flat_hash_map is not exception-safe"*、*"node_hash_map is not exception-safe"*。这是 Abseil 的设计选择而非缺陷：Google 内部代码禁用 C++ 异常，Abseil 代码从未设计为与异常共存。

**具体场景分析**：

| 场景 | 行为 |
|------|------|
| **slot 构造抛异常**（如 `pair<string,int>` 的 string 构造） | `emplace_at` 通过 placement new 构造 slot。若构造函数抛异常，ctrl byte 已被更新为 H2，但 slot 未初始化——表处于不一致状态。Abseil 不回滚 ctrl byte。 |
| **rehash 期间搬移抛异常** | `transfer` 通常为 `memcpy`/`std::move`。对于 trivially relocatable 类型（大多数 POD）为 memcpy，不可能抛异常。对于非 trivial 类型，move 构造可能抛异常，此时新旧表均处于不一致状态。 |
| **allocator 抛异常** | `Allocate()` 调用 `std::allocator::allocate()` 可能抛 `std::bad_alloc`。在 rehash 路径中，新数组分配失败后旧表可能已部分搬移。 |
| **`find` / `count` / `contains`** | 不修改表，不触发分配，理论上不抛异常（前提是 `hash` 和 `eq` 不抛异常）。 |

**实际影响**：如果你的代码编译时启用异常（`-fexceptions`），SwissTable 的操作在异常发生时可能破坏表的内部一致性的。Abseil 的推荐做法是：

1. 确保 `Hash` 和 `Eq` 函数不抛异常。
2. 对于 value_type 的构造可能抛异常的场景，考虑在插入前构造好值，再用 `insert(value_type&&)` 传入——至少在构造失败时表未被修改。
3. 如果需要异常安全，应使用 `std::unordered_map` 或在 Abseil 容器外层包装异常处理逻辑。

## iterator / reference invalidation

**`flat_hash_map` / `flat_hash_set`（内联存储）**：

| 操作 | iterator 失效 | reference/pointer 失效 |
|------|--------------|----------------------|
| `insert` / `emplace` | **全部失效**（可能触发 rehash，搬移所有 slot） | **全部失效** |
| `erase` | **全部失效**（与标准容器不同，erase 也使所有迭代器失效） | **全部失效** |
| `rehash` / `reserve` | **全部失效** | **全部失效** |
| `operator[]` / `try_emplace` | 同 `insert` | 同 `insert` |
| `find` / `count` / `contains` | 不失效 | 不失效 |
| `clear` | **全部失效** | **全部失效** |
| 移动构造/赋值 | 源对象处于 valid-but-unspecified 状态 | — |

**`node_hash_map` / `node_hash_set`（节点指针存储）**：

| 操作 | iterator 失效 | reference/pointer 失效 |
|------|--------------|----------------------|
| `insert` / `emplace` | **全部失效** | **不失效**（节点独立堆分配，指针稳定） |
| `erase` | **全部失效** | 被删元素失效，其余不失效 |
| `rehash` / `reserve` | **全部失效** | **不失效**（只搬移指针，节点不搬移） |

**关键理解**：SwissTable 的 iterator 内部持有 `ctrl_t*` + `slot_type*` + `generation_ptr`，任何导致 ctrl 数组或 slot 数组地址变化的操作都会使迭代器失效。对于 `flat_hash_*`，rehash 时 slot 被 memcpy/move 到新地址，所有指针/引用随之失效。对于 `node_hash_*`，rehash 只搬移 slot 中存储的 `T*` 指针值（`node_slot_policy::transfer` 为 `memcpy` 指针），`T` 对象本身地址不变，因此引用/指针稳定。

**Sanitizer 检测**：Abseil 在 ASan/MSan 模式下启用 `ABSL_SWISSTABLE_ENABLE_GENERATIONS`——在 BackingArray 中嵌入 `GenerationType`，iterator 在每次 `erase`/`insert` 后检查 generation 是否匹配，提前暴露 use-after-invalidation bug。

## 性能模型

### Cache 行为与 SIMD 探测

SwissTable 的核心性能优势来自 **ctrl 数组与 slot 数组分离**的布局：

- **ctrl 数组**：每个 slot 仅 1 字节，16 个 ctrl 恰好装入一个 128-bit SSE 寄存器。一条 `_mm_loadu_si128` + `_mm_cmpeq_epi8` + `_mm_movemask_epi8` 指令链即可完成 16 个 slot 的 H2 预过滤。在一个 64 字节的 cache line 中可以装下 64 个 ctrl byte——覆盖 4 个 Group，即 64 个 slot 的元数据。
- **slot 数组**：连续存储，对大表有更好的 prefetch 友好性。一旦 SIMD 探测确定候选 slot，`PrefetchToLocalCache(slot_array + offset)` 确保 slot 数据在 `equal_to` 比较前已进入 L1 cache。

对比 `std::unordered_map` 的 `_Hash_node` 链表：每次探测需要解引用指针（可能导致 L1 cache miss），且节点分散在堆中，空间局部性差。

### 探测长度的量化模型

在负载因子 `α = n/capacity` 下，**成功查找**的期望探测长度（以 Group 为单位）：

```
成功查找: E[probe] ≈ -ln(1 - α) / α    （α < 1）
失败查找: E[probe] ≈ 1 / (1 - α)
```

在 SwissTable 的 7/8 负载因子（α = 0.875）下：

- 成功查找：`-ln(0.125) / 0.875 ≈ 2.38` 个 Group（约 38 个 slot 的 ctrl 被扫描）
- 失败查找：`1 / 0.125 = 8` 个 Group（最多 128 个 slot 的 ctrl 被扫描）

但由于 SIMD 每次扫描 16 个 slot，且 H2 预过滤将 `==` 比较次数压到 < 1/8 per find，实际的 `operator==` 调用次数极少。

### Tombstone 密度对性能的影响

墓碑不影响探测**终止条件**（只有 kEmpty 终止探测），但会：

1. **增加探测长度**：大量墓碑使得 Group 内缺少 kEmpty，探测必须继续到更远的 Group。
2. **消耗 growth_left 容量**：`erase` 不会减少 size，但 `growth_left` 不会增加（墓碑不计入可用空间）。当 tombstone 密度高时，新的 `insert` 可能因 `growth_left == 0` 而触发 rehash，即使 `size` 远小于 capacity。

**缓解方法**：调用 `rehash(0)` 强制重建表——这会清除所有墓碑，重新计算 `growth_left`。

### SOO 的性能收益

当 `value_type` 足够小（≤ 24 字节）且表仅包含 1 个元素时，SOO 完全避免堆分配。这使得临时创建的单元素 `flat_hash_map`（如函数内部的临时查找表）的构造/析构成本降至几乎为零。

## libstdc++ vs libc++ vs MSVC

| 维度 | Abseil SwissTable | libstdc++ `_Hashtable` | libc++ `__hash_table` | MSVC `_Hash` |
|------|-------------------|----------------------|---------------------|-------------|
| **底层布局** | 开放寻址：ctrl[] + slots[] | 链式：bucket[] + `_Hash_node` 链表 | 链式：bucket[] + 节点链表（圆形链表哨兵） | 链式：bucket[] + 单链表节点 |
| **负载因子** | 固定 7/8（87.5%），小表可达 1.0 | 默认 1.0，可自定义 | 默认 1.0 | 默认 1.0 |
| **缓存行为** | 连续内存 + SIMD 探测，cache 友好 | 指针追逐，cache 不友好 | 指针追逐，cache 不友好 | 指针追逐，cache 不友好 |
| **节点稳定性** | `flat`: ✗ / `node`: ✓ | ✓（节点独立分配） | ✓ | ✓ |
| **迭代器稳定性** | 插入/erase 均失效 | insert 不失效，erase 仅被删元素 | 同 libstdc++ | 同 libstdc++ |
| **桶接口** | 无 bucket 概念 | `bucket_count()`、`bucket_size()`、`begin(bucket)` 等 | 同 libstdc++ | 同 libstdc++ |
| **调试模式** | ASan/MSan generation 检测 | `_GLIBCXX_DEBUG`：迭代器失效检测、桶结构检查 | libc++ debug mode | MSVC iterator debugging (`_ITERATOR_DEBUG_LEVEL`) |
| **异常安全** | 不保证 | 基本异常安全保证 | 基本异常安全保证 | 基本异常安全保证 |
| **`erase(it)` 返回** | `void` | `iterator` | `iterator` | `iterator` |
| **内存分配** | 单次 malloc（ctrl + slots 连续） | 每个节点独立 malloc + bucket 数组 | 每个节点独立 malloc + bucket 数组 | 每个节点独立 malloc + bucket 数组 |

**核心结论**：标准库三家实现均为节点式链表，提供节点/迭代器稳定性但牺牲缓存局部性。Abseil SwissTable 以放弃稳定性为代价换取极高的查找/插入吞吐。在 `flat_hash_map<K, unique_ptr<V>>` 模式下可同时获得指针稳定性和 SwissTable 的缓存优势——这是最常见的迁移模式。

## 最小复现代码

```cpp
#include "absl/container/flat_hash_map.h"

int main() {
  absl::flat_hash_map<int, int> table;
  table.emplace(1, 42);
  return table.find(1)->second;
}
```

## 编译 / 反汇编 / benchmark 证据

### 编译命令

在参考实现目录下编译并链接 Abseil SwissTable：

```bash
# 编译 Abseil 库（CMake 方式）
cd references/impl/abseil-cpp
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17 \
         -DABSL_BUILD_TESTING=OFF -DABSL_USE_GOOGLETEST_HEAD=OFF
cmake --build . -j$(nproc)

# 编译使用 SwissTable 的程序
g++ -std=c++17 -O2 -march=native \
    -I references/impl/abseil-cpp \
    your_code.cpp \
    -L build/absl/container -labsl_raw_hash_set \
    -L build/absl/hash -labsl_hash \
    -L build/absl/base -labsl_base \
    -o demo
```

关键编译标志：

- `-march=native`：启用 SSE2/SSSE3，`GroupSse2Impl` 生效；无此标志则回退到 Portable 实现。
- `-O2` 或 `-O3`：确保 `ABSL_ATTRIBUTE_ALWAYS_INLINE` 函数被内联，`find` 路径无函数调用开销。

### `find` 的反汇编特征（x86-64, `-O2 -march=native`）

核心路径的典型指令序列：

```asm
; flat_hash_map<int,int>::find(key)  热路径
; 假设 rdi = this, esi = key
  call    hash_of              ; 计算 hash（通常内联为几条指令）
  mov     rdx, rax             ; hash
  shr     rdx, 57              ; H2: hash >> (64 - 7)
  and     eax, [rdi+cap_mask]  ; H1: hash & (capacity - 1)
  movdqu  xmm0, [ctrl+rax]    ; _mm_loadu_si128: 加载 16 字节 ctrl
  movd    xmm1, edx           ; 广播 H2
  punpcklbw xmm1, xmm1
  punpcklwd xmm1, xmm1
  pshufd  xmm1, xmm1, 0      ; _mm_set1_epi8(h2)
  pcmpeqb xmm1, xmm0         ; _mm_cmpeq_epi8: 逐字节比较
  pmovmskb eax, xmm1          ; _mm_movemask_epi8: 提取 bitmask
  test    eax, eax
  jnz     .match_found        ; 有 H2 匹配 → 检查 operator==
  ; 检查空位
  pcmpeqb xmm2, kEmpty        ; _mm_cmpeq_epi8(ctrl, 0x80)
  pmovmskb ecx, xmm2
  test    ecx, ecx
  jnz     .not_found           ; 有空位 → key 不存在
  ; 继续探测下一个 Group
  add     index, 16
  add     offset, index
  and     offset, cap_mask
  jmp     .probe_loop
```

**关键观察**：一次 `find` 的热路径仅 3 条 SIMD 指令（`movdqu` + `pcmpeqb` + `pmovmskb`）即可判断 16 个 slot 是否有 H2 匹配或空位。与 `std::unordered_map` 的 `_M_find_node` 链表遍历相比，指令数和分支数显著减少。

### Benchmark 对照

以下为 Abseil 官方 benchmark（`raw_hash_set_benchmark.cc`）的典型结论（x86-64, 单线程）：

| 操作 | `flat_hash_map<int,int>` (1M 元素) | `std::unordered_map<int,int>` (1M 元素) | 加速比 |
|------|-------------------------------------|----------------------------------------|--------|
| `find`（命中） | ~25 ns/op | ~80 ns/op | **~3x** |
| `find`（未命中） | ~20 ns/op | ~60 ns/op | **~3x** |
| `insert`（新 key） | ~100 ns/op | ~300 ns/op | **~3x** |
| `erase` + `insert` 循环 | ~120 ns/op | ~350 ns/op | **~3x** |
| 迭代全部元素 | ~15 ns/elem | ~30 ns/elem | **~2x** |

**吞吐量提升的主要来源**：

1. **SIMD 探测**：16 个 slot 的 H2 检查压缩为 3 条指令。
2. **Cache 友好**：ctrl 数组紧凑（64 bytes/cacheline = 64 slots），slot 连续存储。
3. **高负载因子**：7/8 vs 标准库的 1.0——更少的内存占用意味着更多数据留在 cache 中。
4. **单次分配**：一个 BackingArray 包含 ctrl + slots，无 per-node malloc 碎片。

**注意事项**：

- 以上数据为数量级参考，实际性能取决于 key 类型、hash 函数质量、负载因子和硬件。
- `node_hash_map` 由于节点独立分配，性能介于 `flat_hash_map` 和 `std::unordered_map` 之间。
- Abseil 的 benchmark 源码位于 `references/impl/abseil-cpp/absl/container/internal/raw_hash_set_benchmark.cc`，可用 Bazel 构建：`bazel run -c opt //absl/container:raw_hash_set_benchmark`。

## cpplings 练习入口

- [`unordered1` — 无序容器 (unordered_map / unordered_set)](../../../exercises/cpp11-std/unordered1.cpp)
- [`customhash1` — 自定义哈希](../../../exercises/cpp11-std/customhash1.cpp)
- [`cachefriendly1` — 缓存友好的数据结构](../../../exercises/topics/cachefriendly1.cpp)
- [`perf1` — 性能优化技巧：缓存友好与 string_view](../../../exercises/topics/perf1.cpp)