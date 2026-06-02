---
title: libstdc++ vector 与 unordered_map _Hashtable
topic: libraries
feature: vector-hashtable
standard: C++11
status_checked_at: 2026-06-02
implementation:
  libstdcxx:
    paths:
      - references/impl/gcc/libstdc++-v3/include/bits/stl_vector.h
      - references/impl/gcc/libstdc++-v3/include/bits/hashtable.h
      - references/impl/gcc/libstdc++-v3/include/bits/hashtable_policy.h
    symbols:
      - std::vector
      - std::_Hashtable
      - __detail::_Hash_node
      - _Prime_rehash_policy
exercises:
  - exercises/cpp11-std/unordered1.cpp
  - exercises/cpp11-std/customhash1.cpp
solutions:
  - exercises/solutions/unordered1.cpp
  - exercises/solutions/customhash1.cpp
---
# libstdc++ vector and unordered_map _Hashtable

## std::vector

### Three-Pointer Layout

```cpp
struct _Vector_impl_data {
  pointer _M_start;           // 数据起始
  pointer _M_finish;          // size = _M_finish - _M_start
  pointer _M_end_of_storage;  // capacity = _M_end_of_storage - _M_start
};
// sizeof(vector<T>) = 24（与 libc++ 完全相同）
```

### Growth Formula

```cpp
size_type _M_check_len(size_type __n) const {
  const size_type __len = size() + std::max(size(), __n);
  // push_back: 新容量 = size + size = 2×size（与 libc++ 相同）
  // insert(n个): 新容量 = size + max(size, n)
  return (__len < size() || __len > max_size()) ? max_size() : __len;
}
```

**Differences from libc++**: libstdc++ always takes the `move_if_noexcept` + `destroy` path and does not have libc++'s trivially relocatable `memcpy` optimization.

### Mid-Insert: Three-Phase Copy

```cpp
// 无 split_buffer 概念，直接三段操作：
// 1. move 后半段到新位置
// 2. 构造新元素
// 3. move 前半段（如果有扩容）
```

## std::unordered_map: Node-Based `_Hashtable`

> Source paths: `references/impl/gcc/libstdc++-v3/include/bits/hashtable.h`
>
> `references/impl/gcc/libstdc++-v3/include/bits/hashtable_policy.h`

libstdc++'s `std::unordered_map` / `std::unordered_set` does **not** use SwissTable. Their core implementation is the node-based, bucket + chaining `std::_Hashtable`:

```cpp
std::_Hashtable<_Key, _Value, _Alloc, _ExtractKey, _Equal, _Hash,
                _RangeHash, _Unused, _RehashPolicy, _Traits>
```

The key alias in the source is `using __node_type = __detail::_Hash_node<_Value, __hash_cached::value>;`. The description of the overall structure in `hashtable.h` is straightforward: `_M_buckets` is the bucket array, elements themselves are `__node_type` nodes linked together via `_M_nxt` to form singly-linked lists. You can think of it as:

- A bucket array responsible for mapping a key to a particular chain
- A series of `__node_type` / `_Hash_node<_Value, _Traits::__hash_cached::value>` nodes that actually hold `value_type`
- Each bucket records the entry point of its corresponding linked list

### Structure Diagram

```text
bucket array (_M_buckets)
[0] ──> before_begin ──> node(K1,V1) ──> node(K9,V9) ──> ...
[1] ──> nullptr
[2] ──> before_begin ──> node(K2,V2) ──> ...
...

node = _Hash_node<_Value, _Traits::__hash_cached::value>
     = next pointer + value + (optional) cached hash code
```

Conceptually, the bucket array can be understood as an "array of linked-list head pointers"; the actual type in the source is `__buckets_ptr = __node_base_ptr*`, meaning it first points to a node base class, then via `_M_nxt` reaches the real `_Hash_node`. In essence, this is still a classic chained hash table, not an open-addressing table.

### Load Factor and Rehash Policy

libstdc++'s default `max_load_factor` is 1.0, controlled by the rehash policy:

- `_Prime_rehash_policy(float __z = 1.0)`
- `_Power2_rehash_policy(float __z = 1.0)`

In other words, the default strategy triggers expansion when the average number of elements per bucket approaches 1; this is a fundamentally different design from SwissTable's typical 7/8 load limit.

### Stability and Cost

The key characteristics of the node-based `_Hashtable` are:

- Element objects reside in independent nodes and, unlike open-addressing schemes, are not relocated into contiguous slots during expansion or probing
- References / pointers to non-erased elements remain stable
- Iterator implementation is node-based, with stronger stability than open-addressing; however, per `unordered_map` semantics, iterators should not be reused after `rehash`

The costs are equally clear:

- Extra per-node allocation is required
- After a bucket hit, a pointer chase is needed to reach the node
- No SIMD probing, no ctrl byte array, no H1/H2 split hashing, no open addressing

In other words, **libstdc++'s `std::unordered_map` is a traditional node-based `_Hashtable`; SwissTable belongs to the class of high-performance open-addressing implementations like Abseil `flat_hash_map` and is not part of libstdc++ `unordered_map`.**

## Comparison with Other Hash Table Implementations

| Implementation | Basic Strategy | Metadata / Probing | Typical Load Factor | Stability |
|------|----------|---------------|--------------|--------|
| libstdc++ `_Hashtable` | Node-based chaining, bucket + `_Hash_node` singly-linked list | No ctrl bytes; locate chain by bucket, then traverse nodes | 1.0 | Stable references; node-based iterators |
| Abseil SwissTable (`flat_hash_map`) | Open addressing | SIMD probing, H1/H2 split, ctrl byte array | 7/8 | Expansion/rearrangement may relocate slots |
| Folly F14 | Chunk-based hash table | SIMD tag probing, 14-element chunks | High-load optimized | Varies by variant, not a traditional node chain |

## User API

`std::vector<T>` targets sequential growth and random access; `std::unordered_map<Key, T>` targets amortized O(1) key-based access. The main text above already drills into the implementation skeleton of both; the mapping between user-facing API entry points and internal paths will be completed here.

## Standard Semantics

### `std::vector`

- **Contiguous storage**: The standard requires elements to occupy contiguous memory (`&v[n] == &v[0] + n`); `data()` returns a pointer to the first element
- **Amortized O(1) push_back**: Capacity at least doubles on each reallocation, making N `push_back` calls cost O(N) total, amortized O(1)
- **Iterator invalidation**: Any operation that triggers reallocation (`push_back`, `insert`, `reserve`, `resize`) invalidates all iterators, pointers, and references
- **`erase` semantics**: Only invalidates iterators at or after the erased position; returns an iterator to the position following the last erased element
- **`shrink_to_fit`**: Is a non-binding request (since C++11); implementations may ignore it

### `std::unordered_map`

- **Amortized O(1) lookup**: The standard requires `find`, `count`, `contains` to be O(1) on average and O(N) in the worst case
- **Bucket interface**: The standard exposes `bucket_count()`, `bucket(key)`, `begin(n)/end(n)` and other local iterator interfaces, allowing users to directly access each bucket's linked list
- **`load_factor()` / `max_load_factor()`**: `load_factor() == size() / bucket_count()`; default `max_load_factor()` is 1.0; users can call `rehash(n)` or `reserve(n)` to trigger a rehash
- **Reference stability**: The standard guarantees that `insert` and `rehash` do not invalidate references or pointers to existing elements (§[unord.req.general]); this is the core promise of a node-based implementation
- **Iterator invalidation**: `rehash` invalidates all iterators but does not invalidate references; `erase` only invalidates iterators pointing to the erased element

## Object Layout

The three-pointer layout of `vector` and the bucket + node structure of `_Hashtable` are already covered above; a unified object memory comparison table will be added here.

## Core Source Paths

### vector

| File | Content |
|------|------|
| `bits/stl_vector.h` | `_Vector_base` / `_Vector_impl_data` (three-pointer layout), `_Vector_impl` (inherits allocator + three-pointers), `vector` primary template class definition and all inline member functions |
| `bits/vector.tcc` | Template out-of-line implementations: `reserve()`, `emplace_back()`, `_M_realloc_insert()`, `_M_realloc_append()`, `insert()`, `erase()`, `operator=` |
| `bits/stl_bvector.h` | `vector<bool>` specialization: `_Bit_reference` (proxy reference), `_Bit_iterator` (bit-level iterator), `_M_reallocate()`, `_M_fill_insert()` |

### unordered_map / unordered_set

| File | Content |
|------|------|
| `bits/hashtable.h` | `_Hashtable` primary template: data members (`_M_buckets`, `_M_before_begin`, `_M_element_count`), `_M_find_node()`, `_M_find_before_node_tr()`, `_M_insert_unique_node()`, `_M_insert_bucket_begin()`, `_M_rehash()` |
| `bits/hashtable_policy.h` | Policies and node types: `_Hash_node_base`, `_Hash_node_value_base`, `_Hash_node`, `_Prime_rehash_policy`, `_Power2_rehash_policy`, `_Hashtable_traits`, `_ReuseOrAllocNode` |
| `bits/unordered_map.h` | `std::unordered_map` / `std::unordered_multimap` aliases, instantiating `_Hashtable` parameters |
| `bits/unordered_set.h` | `std::unordered_set` / `std::unordered_multiset` aliases |

## Core Classes / Functions

### vector Side

| Type / Function | Source Location | Description |
|-------------|----------|------|
| `_Vector_impl_data` | `stl_vector.h:98` | Three pointers: `_M_start`, `_M_finish`, `_M_end_of_storage`, zero construction cost |
| `_Vector_impl` | `stl_vector.h:139` | Inherits `_Tp_alloc_type` + `_Vector_impl_data`; EBO eliminates empty allocator |
| `_M_check_len(n, s)` | `stl_vector.h:2272` | Computes new capacity: `size() + max(size(), n)`; throws `length_error` if it exceeds `max_size()` |
| `_M_realloc_insert(pos, arg)` | `vector.tcc:433` | Three-phase reallocation: ① allocate new buffer ② construct target element at new position ③ `__uninitialized_move_if_noexcept_a` moves both halves; finally `_Destroy` the old buffer and `_M_deallocate` |
| `_M_realloc_append(arg)` | `vector.tcc:542` | Tail fast path: skips the second-half move in the three-phase sequence, directly constructs + moves the first half |
| `_M_erase_at_end(p)` | `stl_vector.h` | Calls `_Destroy` from `p` to `_M_finish`, updates `_M_finish`; the underlying implementation of `clear()` |

### unordered_map Side

| Type / Function | Source Location | Description |
|-------------|----------|------|
| `_Hash_node_base` | `hashtable_policy.h:293` | Linked-list node base class, contains only `_M_nxt` pointer (8 bytes) |
| `_Hash_node<Value, Cache>` | `hashtable_policy.h:360` | Inherits `_Hash_node_base` + `_Hash_node_value`; value stored via `__aligned_buffer`; when `Cache=true`, additionally carries `size_t _M_hash_code` |
| `_Hashtable` | `hashtable.h:189` | Primary template with 10 template parameters; CRTP inherits `_Hashtable_base`, `_Map_base`, `_Rehash_base`, `_Hashtable_alloc` |
| `_M_find_node(bkt, key, code)` | `hashtable.h:924` | Calls `_M_find_before_node()` to get the predecessor node, returns `__before_n->_M_nxt` or `nullptr` |
| `_M_find_before_node_tr(bkt, k, code)` | `hashtable.h:2252` | Traverses the bucket chain, calls `_M_equals_tr` to compare hash code and key equality; returns the predecessor node or `nullptr` |
| `_M_insert_unique_node(bkt, code, node)` | `hashtable.h:2492` | ① Calls `_M_need_rehash()` to determine if expansion is needed ② if so, calls `_M_rehash()` ③ recomputes the bucket index ④ calls `_M_insert_bucket_begin()` to insert at the chain head |
| `_M_insert_bucket_begin(bkt, node)` | `hashtable.h:944` | Non-empty bucket: inserts after `_M_buckets[bkt]->_M_nxt`; empty bucket: sets `_M_before_begin._M_nxt` and updates the original begin bucket's pointer |
| `_Prime_rehash_policy` | `hashtable_policy.h:597` | `max_load_factor` defaults to 1.0; `_M_next_bkt(n)` returns the smallest prime ≥ n; `_M_need_rehash()` checks whether `size()/bucket_count()` exceeds the load factor |

## Key Algorithms

### vector: Reallocation and Relocation

| Trigger Condition | Algorithm Path |
|----------|----------|
| `push_back` / `emplace_back`: `_M_finish == _M_end_of_storage` | `_M_realloc_append` → `_M_check_len(1)` → allocate `2×size` → construct new element → `__uninitialized_move_if_noexcept_a` relocates old elements → `_Destroy` old buffer → deallocate |
| `insert(pos, val)`: insufficient capacity | `_M_realloc_insert` → `_M_check_len(1)` → allocate → construct new element → `move_if_noexcept` first half + second half → destroy old buffer |
| `insert(pos, first, last)`: insufficient capacity | `_M_check_len(n)` → `size() + max(size(), n)` → same three-phase relocation, but n may be > 1 |
| `_S_use_relocate() == true` (trivially relocatable or `noexcept_move`) | Direct `__relocate_a`, skipping the move + destroy two-step |

Key source snippet (`vector.tcc:433`):

```cpp
const size_type __len1 = _M_check_len(1u, "vector::_M_realloc_insert");
_Alloc_result __r = this->_M_allocate_at_least(__len1);
// 先在新缓冲的插入位置构造目标元素
_Alloc_traits::construct(this->_M_impl, __new_start + __elems_before, ...);
// 搬移插入点之前的元素
__new_finish = __uninitialized_move_if_noexcept_a(__old_start, __position.base(), __new_start, ...);
// 搬移插入点之后的元素
__new_finish = __uninitialized_move_if_noexcept_a(__position.base(), __old_finish, __new_finish, ...);
// 释放旧缓冲
```

### unordered_map: Bucket Lookup and Chained Search

| Trigger Condition | Algorithm Path |
|----------|----------|
| `find(key)` | `_M_hash_code_tr(key)` → `_M_bucket_index(hash_code)` to get bucket → `_M_find_before_node_tr(bkt, key, code)` → traverse chain calling `_M_equals_tr` to compare |
| `insert/emplace` | `_M_compute_hash_code(key)` → `_M_bucket_index` → `_M_find_before_node_tr` checks for duplicates → `_M_allocate_node` constructs → `_M_insert_unique_node(bkt, code, node)` |
| rehash (inside `insert_unique_node`) | `_M_need_rehash(bkt_count, elt_count, 1)` → if expansion needed: `_M_rehash(new_count)` → allocate new bucket array → re-`_M_bucket_index` all nodes → `_M_insert_bucket_begin` |
| Small-table optimization (`size ≤ 20` and hash is fast hash) | `_M_locate_tr` skips bucket lookup, directly linearly scans the entire table |

Core lookup path (`hashtable.h:2252`):

```cpp
// _M_find_before_node_tr：沿 bucket __bkt 的链表遍历
__node_base_ptr __prev_p = _M_buckets[__bkt];
for (__node_ptr __p = __prev_p->_M_nxt;; __p = __p->_M_next()) {
    if (_M_equals_tr(__k, __code, *__p)) return __prev_p;  // 命中
    if (!__p->_M_nxt || _M_bucket_index(*__p->_M_next()) != __bkt) break;
    __prev_p = __p;
}
return nullptr;
```

## ABI Constraints

### vector ABI

- **Object size**: `sizeof(vector<T>) = sizeof(_Vector_impl_data) + sizeof(_Tp_alloc_type)` = 24 bytes (when EBO eliminates the empty allocator) or 32 bytes (with a stateful allocator)
- **Three-pointer layout is fixed**: Any change to the order of `_M_start` / `_M_finish` / `_M_end_of_storage`, addition of members, or change to pointer types is an ABI break
- **Iterator type**: Regular iterators are raw pointers `T*`; `vector<bool>` iterators are `_Bit_iterator` (pointer + bit offset), and changing its layout affects all translation units using `vector<bool>::iterator`
- **Exception boundary**: The strong guarantee of `push_back` / `insert` depends on `move_if_noexcept`; if a type's move constructor is not `noexcept`, it falls back to copy, which affects code generation at the ABI level

### unordered_map ABI

- **`_Hashtable` data members** (`hashtable.h:358-370`):
  - `__buckets_ptr _M_buckets` (8 bytes)
  - `size_type _M_bucket_count` (8 bytes)
  - `__node_base _M_before_begin` (8 bytes, single pointer)
  - `size_type _M_element_count` (8 bytes)
  - `_RehashPolicy _M_rehash_policy` (`_Prime_rehash_policy` = float + size_t = 16 bytes)
  - `__node_base_ptr _M_single_bucket` (8 bytes)
  - Total approximately 56 bytes (64-bit platform)
- **`_Hash_node` layout**: `_M_nxt` (8) + `value` (after alignment) + optional `hash_code` (8); changing the node inheritance chain or alignment changes the allocation size per element, constituting an ABI break
- **`_Prime_rehash_policy`**: `_M_max_load_factor` (float) + `_M_next_resize` (size_t) + `_S_growth_factor = 2`; changing the prime table or growth factor alters rehash behavior, affecting all compiled `unordered_*` binaries
- **Debug iterators**: In libstdc++'s `debug` mode, iterators additionally carry a container pointer for invalidation checking; changing this structure only affects the debug ABI

## Exception Safety

### vector: Strong Guarantee

The exception safety strategy of `_M_realloc_insert` (`vector.tcc:433`) has two layers:

1. **Allocation failure** (`_M_allocate_at_least` throws `bad_alloc`): the old buffer is completely untouched, container state is unchanged → **strong guarantee**
2. **Element construction / relocation failure**:
   - When `_S_use_relocate()` is true, `__relocate_a` is `noexcept` (bitwise move) and will not throw
   - Otherwise, the path goes through `__uninitialized_move_if_noexcept_a`: for each element, `move_if_noexcept` is called → if the type's move constructor is `noexcept`, it moves; otherwise it copies; **any exception triggers RAII guards (`_Guard_alloc` / `_Guard_elts`) to destroy already-constructed new elements and release the new buffer**
   - The exception is repropagated on the old buffer's elements → the old container is completely unchanged → **strong guarantee**
3. **Key dependency**: Whether the user type's move constructor is marked `noexcept` determines whether expansion moves or copies; types without `noexcept` fall back to copy during expansion, degrading performance but still guaranteeing exception safety

### `_Hashtable`: Basic Guarantee

Exception path of `_M_insert_unique_node` (`hashtable.h:2492`):

1. **`_M_need_rehash` does not throw**: Pure arithmetic (comparison, multiplication), `noexcept`
2. **`_M_rehash` throws** (bucket allocation failure): `__rehash_guard_t` RAII object restores `_M_next_resize` during stack unwinding, and already-allocated nodes are released by `_Scoped_node` → container unchanged → **strong guarantee**
3. **`_M_allocate_node` throws** (node construction failure): `_Scoped_node` ensures the already-allocated node is released; container's `_M_element_count` is unchanged → **strong guarantee**
4. **hash / key_equal throws**: Inside `_M_insert_unique_node`, `_M_store_code` / `_M_insert_bucket_begin` do not call user code (hash code was already computed externally); if `_M_hash_code_tr` (at the insert entry point) throws, the container is completely unchanged → **strong guarantee**
5. **Summary**: libstdc++'s `_Hashtable` achieves at least a basic guarantee on all exception paths; for single-element `insert`, the path after successful hash computation effectively achieves a strong guarantee

## Iterator / Reference Invalidation

### vector

| Operation | Iterators | Pointers / References |
|------|--------|-----------|
| `push_back` / `emplace_back` (triggers reallocation) | **All invalidated** | **All invalidated** |
| `push_back` / `emplace_back` (no reallocation) | Not invalidated | Not invalidated |
| `insert(pos, val)` (triggers reallocation) | **All invalidated** | **All invalidated** |
| `insert(pos, val)` (no reallocation) | **Invalidated after pos** | **Invalidated after pos** |
| `erase(pos)` | **Invalidated after pos** | **Invalidated after pos** |
| `erase(first, last)` | **Invalidated after last** | **Invalidated after last** |
| `reserve(n)` (n > capacity) | **All invalidated** | **All invalidated** |
| `shrink_to_fit` | **All invalidated** (if actually shrinks) | **All invalidated** (if actually shrinks) |
| `clear()` | Does not return an iterator; all iterators are semantically invalidated | Not invalidated (memory not released) |

Root cause: vector's three-pointer layout means any reallocation replaces `_M_start`, causing all iterators based on old pointers to point to deallocated memory.

### unordered_map

| Operation | Iterators | References / Pointers |
|------|--------|-----------|
| `insert` / `emplace` (triggers rehash) | **All invalidated** | **Not invalidated** (nodes are independently allocated) |
| `insert` / `emplace` (no rehash triggered) | Not invalidated | Not invalidated |
| `erase(pos)` | **Only pos invalidated** | Only the erased element invalidated |
| `rehash(n)` | **All invalidated** | **Not invalidated** |
| `reserve(n)` | **All invalidated** (if rehash is triggered) | **Not invalidated** |
| `clear()` | **All invalidated** | **All invalidated** |

Key difference: `_Hashtable`'s node-based design means that rehash only changes the pointers in the bucket array; the node objects themselves (`_Hash_node`) never change address, so references and pointers remain valid throughout. This is explicitly required by the C++ standard §[unord.req.general]: `rehash` only invalidates iterators, not references.

Implementation detail: `_M_rehash` (`hashtable.h`) during rehash: ① allocates a new bucket array → ② traverses all nodes, recomputing each bucket via `_M_bucket_index` → ③ `_M_insert_bucket_begin` rebuilds the chains → ④ deallocates the old bucket array. Node objects are never moved or destroyed throughout the entire process.

## Performance Model

The core difference between "contiguous memory vs pointer chasing" has been described above; a unified performance model covering cache lines, allocation count, hash hit chain length, and expansion cost will be added here.

## libstdc++ vs libc++ vs MSVC

### vector

| Dimension | libstdc++ | libc++ | MSVC STL |
|------|-----------|--------|----------|
| Object size | 24 bytes (3 pointers) | 24 bytes (3 pointers) | 24 bytes (3 pointers) |
| Growth factor | 2× | 2× | 1.5× |
| Relocation strategy | `move_if_noexcept` + destroy; trivially relocatable types use `__relocate_a` | trivially copyable types use `memcpy` optimization; others move + destroy | Similar to libstdc++, no memcpy optimization |
| `vector<bool>` | `stl_bvector.h` specialization, `_Bit_iterator` (pointer + bit offset) | Similar bit-compressed specialization | Similar bit-compressed specialization |
| Debug iterators | `_GLIBCXX_DEBUG` macro enables debug mode, iterators additionally associate with container | No built-in debug iterators | `_ITERATOR_DEBUG_LEVEL=1/2`, iterators hold container pointer and sequence number |
| ASan support | `__sanitizer_annotate_contiguous_container` marks used/unused regions | Similar ASan annotation | Similar ASan annotation |

### unordered_map

| Dimension | libstdc++ | libc++ | MSVC STL |
|------|-----------|--------|----------|
| Core structure | `_Hashtable`: bucket array + `_Hash_node` singly-linked list | `_HashTable`: bucket array + node chain (`__hash_node`), similar structure but different details | `_Hash`: bucket array + node chain |
| Bucket strategy | `_Prime_rehash_policy`: prime bucket count, default max_load_factor=1.0 | Similar prime strategy, default max_load_factor=1.0 | Power-of-two bucket count |
| Hash cache | Optional: `_Hash_node_code_cache<true>` caches `size_t _M_hash_code` | Caches hash code by default (`__hash_cached`) | Typically does not cache hash code |
| Node allocation | `_ReuseOrAllocNode`: erased nodes can be reused by subsequent inserts | No reuse pool, allocate/deallocate each time | No reuse pool |
| Debug iterators | Under `_GLIBCXX_DEBUG`, iterators hold container pointer, check for use-after-invalidation | No built-in debug iterators | Under `_ITERATOR_DEBUG_LEVEL`, iterators hold container pointer + sequence number |
| Small-table optimization | When `size ≤ __small_size_threshold` (default 20), linear scan skips bucket lookup | No such optimization | No such optimization |
| `local_iterator` | Supported, traverses along bucket chain | Supported | Supported |

## Minimal Reproduction Code

```cpp
#include <unordered_map>
#include <vector>

int main() {
  std::vector<int> v;
  v.reserve(4);
  v.push_back(1);

  std::unordered_map<int, int> m;
  m.emplace(1, 42);
}
```

## Compile / Disassembly / Benchmark Evidence

### Viewing Assembly for vector Reallocation Hot Path

```bash
# 生成 vector push_back 扩容路径的汇编
g++ -std=c++20 -O2 -S -o vector_push.s vector_push.cpp

# 反汇编查看 _M_realloc_insert / _M_realloc_append 的实现
objdump -d -C a.out | grep -A30 '_M_realloc_insert\|_M_realloc_append'
```

Key assembly characteristics (x86-64, `-O2`):
- `_M_realloc_insert` calls `_M_check_len` → `operator new` → `__uninitialized_move_if_noexcept_a` loop (`rep movsb` or per-element move) → `operator delete`
- `_M_realloc_append` is similar but skips the second-half relocation
- When `_S_use_relocate()` is true, it takes `__relocate_a`, visible as a more compact `rep movsb` sequence

### Viewing Assembly for `_Hashtable::find`

```bash
# 生成 unordered_map find 路径的汇编
g++ -std=c++20 -O2 -S -o unordered_find.s unordered_find.cpp

# 反汇编查看 _M_find_node / _M_find_before_node_tr
objdump -d -C a.out | grep -A20 '_M_find_before_node_tr\|_M_find_node'
```

Key assembly characteristics:
- `_M_hash_code_tr` → calls the user `hash` function
- `_M_bucket_index(hash_code)` → `hash_code % bucket_count` (libstdc++ uses `_RangeHash`, i.e., modulo operation)
- `_M_find_before_node_tr` internally is a linked-list traversal loop: load `_M_nxt` → compare hash code (if cached) → compare key → advance along `_M_nxt` or break out

### microbenchmark Comparison Framework

```bash
# Google Benchmark 示例：vector push_back 扩容
# unordered_map find 随机访问
# 对比 libstdc++ / libc++（需 -stdlib=libc++）/ MSVC
```

Typical comparison dimensions:

| Scenario | libstdc++ | libc++ | MSVC |
|------|-----------|--------|------|
| vector push_back 1M elements | 2× growth, approximately log₂(1M) ≈ 20 reallocs | Same as libstdc++; trivially copyable types use memcpy for faster relocation | 1.5× growth, approximately 33 reallocs, slightly more total bytes relocated |
| unordered_map find (1M elements, uniform distribution) | One hash + 1-2 comparisons per bucket on each find | Similar | Similar; power-of-two buckets may slightly reduce rehash overhead |
| unordered_map insert + erase loop | `_ReuseOrAllocNode` reuses nodes, reducing malloc calls | malloc/free each time | malloc/free each time |


## cpplings Exercise Entry Points

- [`unordered1` — Unordered Containers (unordered_map / unordered_set)](../../../exercises/cpp11-std/unordered1.cpp)
- [`customhash1` — Custom Hashing](../../../exercises/cpp11-std/customhash1.cpp)
- [`noexcept1` — The Impact of noexcept: Move Semantics and vector`](../../../exercises/topics/noexcept1.cpp)
- [`cachefriendly1` — Cache-Friendly Data Structures](../../../exercises/topics/cachefriendly1.cpp)
