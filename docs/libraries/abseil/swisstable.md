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

待补：补上 SwissTable 与标准 `unordered_*` 语义的重合面、差异点以及 Abseil 扩展能力在本文中的定位。

## 对象布局

上文已经覆盖 ctrl bytes、slots、sentinel 与 clones 的布局；后续补一张按字节偏移展开的 `BackingArray` 视图。

## 核心源码路径

本文开头已给出 `raw_hash_set.h`；后续补 `flat_hash_map.h` / `node_hash_map.h` 到 `container_internal` 的入口链路。

## 核心类 / 函数

待补：统一整理 `raw_hash_set`、`ctrl_t`、`GroupSse2Impl`、`probe_seq`、`find_or_prepare_insert` 等关键类型与函数。

## 关键算法

上文已经覆盖 H1/H2 拆分、SIMD probing、墓碑处理与 rehash；后续补一张“查找 / 插入 / 擦除”的算法路径摘要。

## ABI 约束

待补：说明 Abseil 容器更依赖头文件内联与模板实例化约束，而不是标准库那种长期稳定 ABI 承诺。

## 异常安全

待补：补充 slot 构造失败、rehash 期间搬移失败以及 allocator 抛异常时的回滚边界。

## iterator / reference invalidation

待补：明确 `flat_hash_*` 在插入、erase、rehash 后的 iterator/reference 失效边界，并对比 `node_hash_*` 的稳定性。

## 性能模型

正文已经给出 ctrl/slot 分离、16-byte SIMD group 与 7/8 负载因子的核心思路；后续补 cache miss、探测长度与 tombstone 密度的量化模型。

## libstdc++ vs libc++ vs MSVC

待补：这里补齐 SwissTable 与三家标准库 `unordered_*` 的 bucket 组织、负载因子、节点稳定性与调试模式差异。

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

待补：补上 `find` / `erase` / `rehash` 的反汇编与 microbenchmark，对照标准库节点式哈希表。

## cpplings 练习入口

- [`unordered1` — 无序容器 (unordered_map / unordered_set)](../../../exercises/cpp11-std/unordered1.cpp)
- [`customhash1` — 自定义哈希](../../../exercises/cpp11-std/customhash1.cpp)
- [`cachefriendly1` — 缓存友好的数据结构](../../../exercises/topics/cachefriendly1.cpp)
- [`perf1` — 性能优化技巧：缓存友好与 string_view](../../../exercises/topics/perf1.cpp)