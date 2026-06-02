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
# Abseil SwissTable: SIMD-Accelerated Open-Addressing Hash Table

> Source path: `references/impl/abseil-cpp/absl/container/internal/raw_hash_set.h`

Made widely known by Matt Kulukundis's CppCon 2017 talk "Designing a Fast, Efficient, Cache-friendly Hash Table". SwissTable is the underlying implementation behind Abseil's `flat_hash_map`/`flat_hash_set`, and its design philosophy has influenced numerous high-performance hash table implementations.

## Hash Value Splitting: H1/H2

For any key `x`, after computing `hash(x)` the value is split into two parts:

- **H1** (low bits): The full hash value is retained directly; later, `hash & (capacity - 1)` extracts the low bits to locate the initial probe position. Because capacity is always a power of two, this is equivalent to taking the low `log2(capacity)` bits.
- **H2** (high 7 bits): `hash >> (sizeof(size_t) * 8 - 7)` extracts the top 7 bits of the hash, yielding a 7-bit tag value (`h2_t = uint8_t`, range `0x00`~`0x7F`), which is written into the corresponding slot's control byte. H2 serves as **pre-filtering**: before performing the actual `operator==` on a slot, the 1-byte H2 is used to quickly rule out slots that cannot match. The false positive probability of a 7-bit H2 is `1/128`, and in practice the number of false comparisons per `find` is less than 1/8.

```cpp
// 源码：raw_hash_set.h
// H1 直接保留完整哈希值，后续 mask 操作取低位
inline size_t H1(size_t hash) { return hash; }
// H2 取最高 7 位作为标签
inline h2_t H2(size_t hash) {
    return static_cast<h2_t>(hash >> (sizeof(size_t) * 8 - 7));
}
```

## Control Byte Layout

SwissTable compresses per-slot metadata into a single `ctrl_t` (`enum class ctrl_t : int8_t`), **stored separately** from the slot data as a parallel array:

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

The choice of these values is subject to strict constraints (enforced via `static_assert` in the source):

- **MSB=1** for `kEmpty`, `kDeleted`, `kSentinel` allows SIMD to distinguish "occupied" (positive) from "special values" (negative) in a single pass using `_mm_cmpgt_epi8`.
- `kSentinel = -1` enables zero-cost generation in SSE2 via `pcmpeqd xmm, xmm` (all-ones vector), without loading from memory.
- `kEmpty = -128` enables saturated subtraction `_mm_subs_epi8` to batch-convert `kEmpty` to `kDeleted` (used in `ConvertSpecialToEmptyAndFullToDeleted` during rehash).

## Full Memory Layout

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

**Why are ctrl and slots separated?** 16 control bytes fit exactly into a 128-bit SSE register, allowing a single SIMD operation to complete a probe. If interleaved with slots, a 64-byte cache line could only hold metadata for a small number of slots. With separation, only control bytes need to be loaded into SIMD registers during probing, and the vast majority of probes can determine a hit or the need to continue after just the first `_mm_cmpeq_epi8` + `_mm_movemask_epi8`.

`clones[kWidth - 1]` is a copy of the first `kWidth - 1` bytes of the `ctrl` array. When the probe position is near the end of the `ctrl` array, GroupSse2Impl's 16-byte load would "wrap around" to the beginning of the array—the `clones` array provides a valid copy of this wrapped-around data, preventing out-of-bounds reads.

## SSE2 Probe Core (GroupSse2Impl)

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

**`Match()` complete flow**:

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

## Probe Sequence (probe_seq)

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

The probe sequence uses triangular number increments: step N advances by `N * kWidth` (16 under SSE2). For example, with capacity=64, the probe sequence is 0, 16, 48, 96%64=32, ... The step is always a multiple of the Group width, ensuring every Group is probed and avoiding fixed-stride cycles.

## Load Factor and Rehash

- **Large tables** (capacity > kWidth=16): maximum load factor of **7/8** (87.5%).
- **Small tables** (capacity <= kWidth): load factor allowed to reach 1.0 (all slots filled), because the entire table is a single Group and SSE2 completes the probe in one pass.
- The `growth_left` field tracks "how many more elements can be inserted in the current capacity."

During rehash, only occupied slots (`IsFull(ctrl[i])`) are transferred; both EMPTY and DELETED are discarded—all control bytes in the new table are initialized to `kEmpty`, and tombstones are naturally cleaned up.

## Tombstone Handling and Erase Optimization

`erase` does not simply set the control byte to `kEmpty`. The key optimization in the source:

```
如果被擦除 slot 所在 Group 内存在其他 EMPTY slot，
则可以安全地将该 slot 标记为 EMPTY（而非 DELETED）。
```

Rationale: If a Group contains an EMPTY bit, that Group has never been completely filled. The `find` probe sequence stops when it encounters a Group with EMPTY, so the erased slot cannot be part of another key's probe chain—marking it EMPTY allows subsequent probes to terminate earlier.

If the Group has no EMPTY slots, the slot must be marked as `kDeleted` (tombstone), because other keys' probe chains may span across it.

## Small Object Optimization (SOO)

Modern Abseil also implements Small Object Optimization: when `value_type` is small enough (size/alignment) and the capacity is also small, the entire hash table is stored inline within the `raw_hash_set` object itself, completely avoiding heap allocation.

## flat_hash_map vs node_hash_map

```cpp
#include "absl/container/flat_hash_map.h"

absl::flat_hash_map<std::string, int> scores;
scores["alice"] = 95;

// 当 key/value 较大或需要指针稳定性时，用 node_hash_map
absl::node_hash_map<std::string, std::vector<int>> big_values;
```

| Feature | `flat_hash_map` | `node_hash_map` |
|------|----------------|----------------|
| Storage | Inline (key+value directly in slot) | Independent node allocation |
| Cache-friendly | ✓ (contiguous memory) | ✗ (pointer chasing) |
| Iterator stability | ✗ (invalidated after insert/erase) | ✓ |
| Reference stability | ✗ | ✓ |
| Use case | Most scenarios | When reference stability is needed |

## Differences from libstdc++ `_Hashtable`

libstdc++'s `std::unordered_map` uses a node-based `_Hashtable` (bucket + `_Hash_node` linked list) rather than open addressing. Here are the key differences between Abseil SwissTable and libstdc++ `_Hashtable`:

| Dimension | Abseil SwissTable | libstdc++ `_Hashtable` |
|------|-------------------|----------------------|
| Memory layout | Open addressing: ctrl parallel array + contiguous slot array | Chained: bucket array + `_Hash_node` linked list |
| Cache behavior | Contiguous memory, SIMD probing | Pointer chasing, cache-unfriendly |
| Load factor | 7/8 (87.5%) | 1.0 (default) |
| Hash splitting | H1/H2 separation, 7-bit tag pre-filtering | No pre-filtering |
| Iterator stability | ✗ (invalidated after rehash) | ✓ (nodes independently allocated) |
| Reference stability | ✗ | ✓ |
| Use case | High-performance query-intensive scenarios | Scenarios requiring iterator/reference stability |

## User API

The user-facing containers covered in this document are primarily `absl::flat_hash_map` / `absl::flat_hash_set`; the main text already drills directly into their shared underlying `raw_hash_set`.

## Standard Semantics

Abseil SwissTable containers (`flat_hash_map`, `flat_hash_set`, `node_hash_map`, `node_hash_set`) are **not** C++ standard containers—they do not appear in any C++ standard version, nor do they follow the standard iterator stability/exception safety contracts. Abseil positions them as **high-performance drop-in replacements** for `std::unordered_*`, with broadly compatible APIs but explicit differences:

**Overlap with standard `unordered_*`**:

- Same core interface: `find`, `insert`, `emplace`, `erase`, `operator[]`, `count`, `contains` (entered the standard in C++20), `equal_range`.
- Same type aliases: `key_type`, `mapped_type`, `value_type`, `size_type`, `allocator_type`.
- Support for the `Allocator` template parameter, following `propagate_on_container_*` traits.
- Support for construction from `std::initializer_list` and iterator ranges.

**Key differences from the standard**:

| Dimension | Standard `unordered_map` | Abseil `flat_hash_map` |
|------|---------------------|----------------------|
| `erase(iterator)` return type | `iterator` (points to next element) | `void` (use `erase(it++)` to obtain next iterator) |
| `bucket_count()` / `bucket_size()` | Available, exposes bucket structure | No bucket concept; provides `capacity()` |
| `max_load_factor()` read/write | Readable and writable | Not exposed (fixed at 7/8) |
| Iterator stability | Insert does not invalidate iterators | **All inserts may invalidate iterators** |
| Exception safety | Basic exception safety guarantee | **No exception safety guarantee** (explicitly stated in source code comments) |
| Heterogeneous lookup | Supported since C++20 with transparent Hash/Eq | Supported from the beginning (`is_transparent`) |
| `reserve()` semantics | Reserve n buckets | Reserve capacity for n elements (no bucket concept) |

**Abseil extensions**:

- **Heterogeneous lookup**: When both `Hash` and `Eq` define `is_transparent`, `find`, `erase`, `count`, `operator[]` accept non-`key_type` arguments, avoiding temporary key object construction. This is one of Abseil's key selling points; the standard only partially supports this since C++20.
- **`capacity()`**: Exposes the table's internal capacity (total number of slots), useful for estimating memory usage.
- **`erase(begin(), end())` special semantics**: Equivalent to `clear()` but retains already-`reserve`d capacity, without shrinking the allocation.
- **`ABSL_ATTRIBUTE_OWNER`**: Marks the type as an owner type, helping compiler lifetime analysis detect use-after-move.

## Object Layout

The layout of ctrl bytes, slots, sentinel, and clones is already covered above; a byte-offset-expanded `BackingArray` view will be added later.
## Core Source Paths

Source paths for the core implementation files (all relative to `references/impl/abseil-cpp/`):

**User-facing header files** (thin wrappers that directly `#include` the internal headers below):

```
absl/container/flat_hash_map.h    → absl::flat_hash_map
absl/container/flat_hash_set.h    → absl::flat_hash_set
absl/container/node_hash_map.h    → absl::node_hash_map
absl/container/node_hash_set.h    → absl::node_hash_set
```

**Internal implementation** (`absl/container/internal/`):

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

**Call chain** (using `flat_hash_map::find` as an example):

```
flat_hash_map<K,V>::find(key)
  → raw_hash_map<FlatHashMapPolicy<K,V>, ...>::find(key)
    → raw_hash_set<...>::find(key)
      → find_large(key, hash_of(key))           // 大表路径
        → probe(common(), hash)                  // 构造 probe_seq
        → Group(ctrl + offset).Match(H2(hash))   // SIMD 探测
        → equal_to(key, slot)                     // 逐候选比较
```


## Core Classes / Functions

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

## Key Algorithms

### Lookup (`find`) Algorithm Path

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

**Key property**: Lookup does not stop when encountering kDeleted—only kEmpty terminates the probe. This is because a deleted slot may be in the middle of an existing key's probe chain.

### Insertion (`find_or_prepare_insert`) Algorithm Path

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

**Insertion reuses tombstones**: `PrepareInsertLarge`, after finding a Group with EMPTY, searches within that Group for the first non-Full bit (either EMPTY or DELETED will do), preferring tombstone positions to reduce unnecessary rehashes.

### Erasure (`erase`) Algorithm Path

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

### Rehash Algorithm Path

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

**Rehash clears tombstones**: During rehash, only `IsFull` slots are transferred; kEmpty and kDeleted are all discarded. The new table's ctrl is fully initialized to kEmpty, and tombstones are naturally cleaned up. This is the only way to recover from performance degradation after extensive `erase` operations (`erase` does not trigger rehash).

## ABI Constraints

Abseil containers have **no stable ABI**—this stands in stark contrast to the cross-version stable symbol guarantees that libstdc++/libc++ provide for `std::unordered_map`.

**Abseil's ABI strategy**:

- **Header-only inlining first**: Core types like `raw_hash_set`, `GroupSse2Impl`, `probe_seq` are all templates, fully defined in headers. The vast majority of logic (`find`, `emplace`, `erase`) is inlined by the compiler at call sites, generating no cross-TU symbols.
- **Type erasure reduces bloat**: To control code bloat, operations that do not depend on template parameters are extracted into type-erased functions (defined in `raw_hash_set.cc`): `PrepareInsertLarge`, `EraseMetaOnlyLarge`, `Rehash`, `Copy`, `ClearBackingArray`, etc. These functions obtain type information through a `PolicyFunctions` struct (containing function pointers and type metadata).
- **No cross-library stable ABI guarantee**: Abseil's versioning policy is **source compatibility**, not binary compatibility. Upgrading the Abseil version requires full recompilation. Source code comments explicitly state: *"Using `flat_hash_map` at interface boundaries in dynamically loaded libraries (e.g. .dll, .so) is unsupported due to the way `absl::Hash` values may be randomized across dynamically loaded libraries."*
- **`absl::Hash` randomization**: `absl::Hash` results may differ across processes (even across different .so files) (per-process seed), meaning that passing SwissTable instances across dynamic libraries is not only ABI-unstable but also semantically incorrect.
- **Template instantiation control**: The template parameter combination of `raw_hash_set` (`Policy, Hash, Eq, Alloc`) determines the final instantiated type. Attributes like `ABSL_ATTRIBUTE_OWNER` do not affect ABI but do affect compiler static analysis.

**Comparison with the standard library**:

| Dimension | libstdc++ `std::unordered_map` | Abseil `flat_hash_map` |
|------|-------------------------------|----------------------|
| ABI stability | Yes (symbols in libstdc++.so) | No (header-only templates, no stable ABI) |
| Cross-.so usage | Supported | **Not supported** |
| Version upgrade | Binary-compatible possible | Must fully recompile |
| Symbol export | `_Hashtable`-related symbols | Type-erased function symbols (internal, not public ABI) |

## Exception Safety

Abseil SwissTable containers **do not provide exception safety guarantees**—source code comments explicitly state: *"flat_hash_map is not exception-safe"*, *"node_hash_map is not exception-safe"*. This is a deliberate design choice, not a deficiency: Google's internal code disables C++ exceptions, and Abseil code was never designed to coexist with exceptions.

**Scenario-by-scenario analysis**:

| Scenario | Behavior |
|------|------|
| **Slot construction throws** (e.g. `pair<string,int>` string constructor) | `emplace_at` constructs the slot via placement new. If the constructor throws, the ctrl byte has already been updated to H2 but the slot is uninitialized—the table is in an inconsistent state. Abseil does not roll back the ctrl byte. |
| **Transfer throws during rehash** | `transfer` is typically `memcpy`/`std::move`. For trivially relocatable types (most PODs) it's memcpy, which cannot throw. For non-trivial types, move construction may throw, leaving both old and new tables in an inconsistent state. |
| **Allocator throws** | `Allocate()` calls `std::allocator::allocate()` which may throw `std::bad_alloc`. In the rehash path, if the new array allocation fails, the old table may be partially transferred. |
| **`find` / `count` / `contains`** | Do not modify the table, do not trigger allocation, and theoretically do not throw (assuming `hash` and `eq` do not throw). |

**Practical impact**: If your code is compiled with exceptions enabled (`-fexceptions`), SwissTable operations may corrupt the table's internal consistency when an exception occurs. Abseil's recommended practices are:

1. Ensure `Hash` and `Eq` functions do not throw exceptions.
2. For value types whose construction may throw, consider constructing the value before insertion, then passing it in via `insert(value_type&&)`—at least the table is not modified if construction fails.
3. If exception safety is required, use `std::unordered_map` or wrap Abseil containers with exception handling logic.

## Iterator / Reference Invalidation

**`flat_hash_map` / `flat_hash_set` (inline storage)**:

| Operation | Iterator invalidation | Reference/pointer invalidation |
|------|--------------|----------------------|
| `insert` / `emplace` | **All invalidated** (may trigger rehash, moving all slots) | **All invalidated** |
| `erase` | **All invalidated** (unlike standard containers, erase also invalidates all iterators) | **All invalidated** |
| `rehash` / `reserve` | **All invalidated** | **All invalidated** |
| `operator[]` / `try_emplace` | Same as `insert` | Same as `insert` |
| `find` / `count` / `contains` | Not invalidated | Not invalidated |
| `clear` | **All invalidated** | **All invalidated** |
| Move construction/assignment | Source object enters valid-but-unspecified state | — |

**`node_hash_map` / `node_hash_set` (node pointer storage)**:

| Operation | Iterator invalidation | Reference/pointer invalidation |
|------|--------------|----------------------|
| `insert` / `emplace` | **All invalidated** | **Not invalidated** (nodes independently heap-allocated, pointers stable) |
| `erase` | **All invalidated** | Erased element invalidated, others not |
| `rehash` / `reserve` | **All invalidated** | **Not invalidated** (only pointers transferred, nodes not moved) |

**Key understanding**: SwissTable iterators internally hold `ctrl_t*` + `slot_type*` + `generation_ptr`; any operation that changes the address of the ctrl array or slot array invalidates iterators. For `flat_hash_*`, during rehash slots are memcpy/moved to new addresses, invalidating all pointers/references. For `node_hash_*`, rehash only moves the `T*` pointer values stored in slots (`node_slot_policy::transfer` is `memcpy` of pointers); the `T` objects themselves are not relocated, so references/pointers remain stable.

**Sanitizer detection**: Abseil enables `ABSL_SWISSTABLE_ENABLE_GENERATIONS` under ASan/MSan—a `GenerationType` is embedded in the BackingArray, and iterators check whether the generation matches after each `erase`/`insert`, exposing use-after-invalidation bugs early.

## Performance Model

### Cache Behavior and SIMD Probing

SwissTable's core performance advantage comes from the **separation of ctrl array and slot array** layout:

- **ctrl array**: Each slot is only 1 byte; 16 ctrl bytes fit exactly into a 128-bit SSE register. A chain of `_mm_loadu_si128` + `_mm_cmpeq_epi8` + `_mm_movemask_epi8` instructions completes H2 pre-filtering for 16 slots. A 64-byte cache line can hold 64 ctrl bytes—covering 4 Groups, i.e. 64 slots' metadata.
- **slot array**: Contiguous storage provides better prefetch friendliness for large tables. Once SIMD probing identifies candidate slots, `PrefetchToLocalCache(slot_array + offset)` ensures slot data is in L1 cache before the `equal_to` comparison.

In contrast, `std::unordered_map`'s `_Hash_node` linked list requires pointer dereferencing per probe (potentially causing L1 cache misses), with nodes scattered across the heap and poor spatial locality.

### Quantitative Model of Probe Length

At load factor `α = n/capacity`, the expected probe length (in Groups) for **successful lookups**:

```
成功查找: E[probe] ≈ -ln(1 - α) / α    （α < 1）
失败查找: E[probe] ≈ 1 / (1 - α)
```

At SwissTable's 7/8 load factor (α = 0.875):

- Successful lookup: `-ln(0.125) / 0.875 ≈ 2.38` Groups (approximately 38 slots' ctrl scanned)
- Failed lookup: `1 / 0.125 = 8` Groups (at most 128 slots' ctrl scanned)

However, since SIMD scans 16 slots at a time and H2 pre-filtering reduces `==` comparisons to less than 1/8 per find, the actual number of `operator==` calls is minimal.

### Impact of Tombstone Density on Performance

Tombstones do not affect the probe **termination condition** (only kEmpty terminates probing), but they:

1. **Increase probe length**: Abundant tombstones cause Groups to lack kEmpty, forcing probing to continue to more distant Groups.
2. **Consume growth_left capacity**: `erase` does not reduce size, but `growth_left` does not increase (tombstones are not counted as available space). When tombstone density is high, new `insert` operations may trigger rehash due to `growth_left == 0`, even when `size` is far below capacity.

**Mitigation**: Call `rehash(0)` to force a table rebuild—this clears all tombstones and recalculates `growth_left`.

### SOO Performance Benefits

When `value_type` is small enough (≤ 24 bytes) and the table contains only 1 element, SOO completely avoids heap allocation. This brings the construction/destruction cost of temporarily created single-element `flat_hash_map`s (e.g. temporary lookup tables within functions) to nearly zero.

## libstdc++ vs libc++ vs MSVC

| Dimension | Abseil SwissTable | libstdc++ `_Hashtable` | libc++ `__hash_table` | MSVC `_Hash` |
|------|-------------------|----------------------|---------------------|-------------|
| **Underlying layout** | Open addressing: ctrl[] + slots[] | Chained: bucket[] + `_Hash_node` linked list | Chained: bucket[] + node linked list (circular list sentinel) | Chained: bucket[] + singly-linked list nodes |
| **Load factor** | Fixed 7/8 (87.5%), small tables up to 1.0 | Default 1.0, customizable | Default 1.0 | Default 1.0 |
| **Cache behavior** | Contiguous memory + SIMD probing, cache-friendly | Pointer chasing, cache-unfriendly | Pointer chasing, cache-unfriendly | Pointer chasing, cache-unfriendly |
| **Node stability** | `flat`: ✗ / `node`: ✓ | ✓ (nodes independently allocated) | ✓ | ✓ |
| **Iterator stability** | Invalidated by both insert/erase | insert does not invalidate; erase only invalidates erased element | Same as libstdc++ | Same as libstdc++ |
| **Bucket interface** | No bucket concept | `bucket_count()`, `bucket_size()`, `begin(bucket)`, etc. | Same as libstdc++ | Same as libstdc++ |
| **Debug mode** | ASan/MSan generation detection | `_GLIBCXX_DEBUG`: iterator invalidation detection, bucket structure checks | libc++ debug mode | MSVC iterator debugging (`_ITERATOR_DEBUG_LEVEL`) |
| **Exception safety** | Not guaranteed | Basic exception safety guarantee | Basic exception safety guarantee | Basic exception safety guarantee |
| **`erase(it)` return** | `void` | `iterator` | `iterator` | `iterator` |
| **Memory allocation** | Single malloc (ctrl + slots contiguous) | Per-node malloc + bucket array | Per-node malloc + bucket array | Per-node malloc + bucket array |

**Core conclusion**: All three standard library implementations use node-based linked lists, providing node/iterator stability at the cost of cache locality. Abseil SwissTable trades stability for extremely high lookup/insertion throughput. Using the `flat_hash_map<K, unique_ptr<V>>` pattern, you can get both pointer stability and SwissTable's cache advantages—this is the most common migration pattern.

## Minimal Reproduction Code

```cpp
#include "absl/container/flat_hash_map.h"

int main() {
  absl::flat_hash_map<int, int> table;
  table.emplace(1, 42);
  return table.find(1)->second;
}
```

## Compilation / Disassembly / Benchmark Evidence

### Compilation Commands

Compile and link Abseil SwissTable in the reference implementation directory:

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

Key compilation flags:

- `-march=native`: Enables SSE2/SSSE3, activating `GroupSse2Impl`; without this flag, it falls back to the Portable implementation.
- `-O2` or `-O3`: Ensures `ABSL_ATTRIBUTE_ALWAYS_INLINE` functions are inlined, eliminating function call overhead in the `find` path.

### `find` Disassembly Characteristics (x86-64, `-O2 -march=native`)

Typical instruction sequence on the core path:

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

**Key observation**: The hot path of a single `find` uses only 3 SIMD instructions (`movdqu` + `pcmpeqb` + `pmovmskb`) to determine whether any of 16 slots have an H2 match or empty. Compared to `std::unordered_map`'s `_M_find_node` linked list traversal, the instruction count and branch count are significantly reduced.

### Benchmark Comparison

The following are typical conclusions from Abseil's official benchmarks (`raw_hash_set_benchmark.cc`) (x86-64, single-threaded):

| Operation | `flat_hash_map<int,int>` (1M elements) | `std::unordered_map<int,int>` (1M elements) | Speedup |
|------|-------------------------------------|----------------------------------------|--------|
| `find` (hit) | ~25 ns/op | ~80 ns/op | **~3x** |
| `find` (miss) | ~20 ns/op | ~60 ns/op | **~3x** |
| `insert` (new key) | ~100 ns/op | ~300 ns/op | **~3x** |
| `erase` + `insert` cycle | ~120 ns/op | ~350 ns/op | **~3x** |
| Iterate all elements | ~15 ns/elem | ~30 ns/elem | **~2x** |

**Primary sources of throughput improvement**:

1. **SIMD probing**: 16 slots' H2 checks compressed to 3 instructions.
2. **Cache-friendly**: ctrl array is compact (64 bytes/cacheline = 64 slots), slots stored contiguously.
3. **High load factor**: 7/8 vs standard library's 1.0—less memory usage means more data stays in cache.
4. **Single allocation**: One BackingArray contains ctrl + slots, no per-node malloc fragmentation.

**Caveats**:

- The above figures are order-of-magnitude references; actual performance depends on key type, hash function quality, load factor, and hardware.
- `node_hash_map`, due to independent node allocation, has performance between `flat_hash_map` and `std::unordered_map`.
- Abseil's benchmark source is at `references/impl/abseil-cpp/absl/container/internal/raw_hash_set_benchmark.cc`, buildable with Bazel: `bazel run -c opt //absl/container:raw_hash_set_benchmark`.

## cpplings Exercise Entry Points

- [`unordered1` — Unordered containers (unordered_map / unordered_set)](../../../exercises/cpp11-std/unordered1.cpp)
- [`customhash1` — Custom hash](../../../exercises/cpp11-std/customhash1.cpp)
- [`cachefriendly1` — Cache-friendly data structures](../../../exercises/topics/cachefriendly1.cpp)
- [`perf1` — Performance optimization techniques: cache-friendliness and string_view](../../../exercises/topics/perf1.cpp)
