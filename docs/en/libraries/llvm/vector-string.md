---
title: libc++ vector and string
topic: libraries
feature: vector-string
standard: C++11
status_checked_at: 2026-06-02
implementation:
  libcxx:
    paths:
      - references/impl/llvm-project/libcxx/include/vector
      - references/impl/llvm-project/libcxx/include/string
    symbols:
      - std::vector
      - _SplitBuffer
      - __recommend
      - std::basic_string
      - __is_long
exercises: []
solutions: []
---
# libc++ vector and string

> Source path: `references/impl/llvm-project/libcxx/include/vector`, `string`

## std::vector: Three-Pointer Layout and split_buffer

### Memory Layout

```
vector<int> v = {10, 20, 30};   capacity = 5

  __begin_    __end_         __cap_
    ↓           ↓              ↓
    ┌────┬────┬────┬────────────┐
    │ 10 │ 20 │ 30 │  ?  │  ?  │
    └────┴────┴────┴────────────┘

    size     = __end_ - __begin_  = 3
    capacity = __cap_  - __begin_  = 5
    sizeof(vector) = 24 bytes (3 raw pointers, empty allocator compressed to 0 via [[no_unique_address]])
```

### emplace_back Fast and Slow Paths

```
         ┌──────────────────┐
         │  __end_ < __cap_ │
         └────────┬─────────┘
           ┌──────┴──────┐
       YES ↓              ↓ NO
  ┌────────────────┐  ┌─────────────────────────────┐
  │ Hot path (inline)│  │ Cold path (__emplace_back_slow) │
  │ placement new  │  │ 1. __recommend(2×cap)        │
  │ ++__end_       │  │ 2. Allocate split_buffer     │
  │                │  │ 3. __swap_out_circular_buffer │
  └────────────────┘  └─────────────────────────────┘
```

**`__if_likely_else` trick** (vector.h:1108): When the condition is known at compile time, the untaken branch is eliminated directly; otherwise the branch is marked `[[likely]]` for branch prediction optimization.

### split_buffer: Buffer with a Hole in the Middle

```
Original: [A B C D E] capacity=5, insert(2, X)

Step 1: Allocate split_buffer(capacity=10, position 2 left empty)
Step 2: Relocate [D, E] to the end first → [? ? ? ? ? ? ? D E ?]
Step 3: Then relocate [A, B] to the beginning → [A B ? ? ? ? ? D E ?]
Step 4: Place X in the empty spot        → [A B X ? ? ? ? D E ?]

Why relocate the second half first? Exception safety.
```

### memcpy Optimization

```cpp
// Five-way condition: trivially relocatable AND trivial move/destroy AND not constexpr
// ALL YES → __builtin_memcpy to relocate the entire range at once
// ANY NO  → per-element move_if_noexcept + destroy
```

## std::string: 24-Byte SSO

> **Note**: libc++ has two string ABI layouts (default and alternate). The following describes the **alternate layout** (`_LIBCPP_ABI_ALTERNATE_STRING_LAYOUT`), which is also the default implementation in the source code. Other ABI options like `_LIBCPP_ABI_STRING_PAIR_LAYOUT` have different layouts.

```
sizeof(basic_string) = 24 bytes

Short mode (≤ 22 bytes): lowest bit of last byte = 0
  bytes 0-22: character data (up to 22 bytes + \0)
  byte 23:    (23-size)<<1 | 0

Long mode (> 22 bytes): lowest bit of last byte = 1
  bytes 0-7:   capacity|1 (odd)
  bytes 8-15:  size
  bytes 16-23: data* (heap-allocated pointer)

SSO capacity = 22 bytes → stores millions of short strings using ~25% less memory than other implementations
```

**Why 22 bytes?** `sizeof(__long)` = 8+8+8 = 24. Short mode's `__data_[23]` occupies 23 bytes, minus 1 byte for `\0` = 22.

**How does `__is_long()` determine the mode?** Checks the lowest bit of the last byte: `0` = Short, `1` = Long. This check is performed on the hot path of nearly every string operation.

## Standard Semantics

### vector Standard Requirements

| Requirement | Clause | libc++ Implementation |
|---|---|---|
| Contiguous storage | \[vector.overview\] | `__begin_` to `__end_` contiguous memory, `data()` returns `__begin_` |
| `operator[]` performs no bounds checking | \[vector.access\] | Direct dereference `*(__begin_ + n)`, UB on out-of-bounds |
| `at()` performs bounds checking | \[vector.access\] | Throws `std::out_of_range` on out-of-bounds |
| `push_back` strong exception guarantee | \[vector.modifiers\] | Via split_buffer: allocate new buffer first then relocate; on failure original data unchanged |
| `insert` strong exception guarantee | \[vector.modifiers\] | Same as above, split_buffer's gap-in-middle strategy ensures exception safety |
| Move construction noexcept | \[vector.cons\] | `noexcept(is_nothrow_move_constructible<allocator_type>::value)` |
| `reserve` does not invalidate iterators (if n ≤ capacity) | \[vector.capacity\] | Only reallocates when `n > capacity()` |
| `shrink_to_fit` returns excess capacity | \[vector.capacity\] | `noexcept`, may create a new buffer to relocate data |

### string Standard Requirements

| Requirement | Clause | libc++ Implementation |
|---|---|---|
| Contiguous storage | \[string.require\] | SSO: `__data_[23]` contiguous; heap: `__data_` pointer contiguous |
| `operator[]` returns a reference | \[string.access\] | Both SSO and heap modes return direct references, no COW lazy copy |
| `data()` returns a writable pointer (C++17) | \[string.accessors\] | `__is_long()` branch returns the corresponding pointer, directly writable |
| `c_str()` and `data()` return the same pointer | \[string.accessors\] | Both implementations are identical, both call `__get_pointer()` |
| Non-mutating operations do not invalidate iterators | \[string.iterators\] | No COW, non-mutating operations don't trigger reallocation |
| `reserve` strong exception guarantee | \[string.capacity\] | Allocate new buffer first then copy; on failure original string unchanged |
| Move assignment noexcept | \[string.modifiers\] | `noexcept(POCMA \|\| is_always_equal)` |

## Core Source Paths

| File | Responsibility |
|---|---|
| `include/__vector/vector.h` | vector primary template: three-pointer layout, `__recommend()`, `emplace_back`, `insert` series |
| `include/__split_buffer` | `__split_buffer`: buffer with a hole in the middle for vector expansion |
| `include/__vector/vector_bool.h` | `vector<bool>` specialization: bit-compressed storage |
| `include/string` | `basic_string` primary template: SSO layout, `__is_long()`, all inline member functions |
| `include/__string/constexpr_c_functions.h` | constexpr versions of C string operations (`strlen`, `memcpy`, etc.) |
| `include/__string/char_traits.h` | `char_traits<char>` specialization: `copy`, `move`, `compare` |
| `include/__memory/uninitialized_algorithms.h` | `__uninitialized_allocator_move_if_noexcept`: vector relocation core |
| `include/__type_traits/is_trivially_relocatable.h` | `__libcpp_is_trivially_relocatable`: type trait for memcpy optimization |

## Core Classes / Functions

### vector Core Members

```cpp
template <class _Tp, class _Allocator>
class vector {
  pointer __begin_;    // data start
  pointer __end_;      // end of valid elements
  pointer __cap_;      // end of allocated capacity
  _Allocator __alloc_; // [[no_unique_address]] compressed

  // Capacity growth
  size_type __recommend(size_type __new_size) const;

  // Expansion core
  void __swap_out_circular_buffer(_SplitBuffer& __v);

  // Relocation strategy selection
  template <class _Up>
  _LIBCPP_CONSTEXPR_SINCE_CXX20 void __move_range(pointer __from_s, pointer __from_e, pointer __to);

  // Destruction guard
  class __destroy_vector;
};
```

### __split_buffer Core Members

```cpp
template <class _Tp, class _Allocator, template<class,class,class> class _Layout>
class __split_buffer {
  pointer __front_cap_;  // allocation start
  pointer __begin_;      // start of valid elements
  pointer __end_;        // end of valid elements
  pointer __back_cap_;   // allocation end

  // front_spare = __begin_ - __front_cap_
  // back_spare  = __back_cap_ - __end_

  void __construct_at_end(size_type __n);
  void push_back(const_reference __x);
  void push_front(const_reference __x);
};
```

### string Core Members

```cpp
// alternate layout (default)
struct __long {
  pointer __data_;              // heap pointer
  size_type __size_;            // string length
  size_type __cap_ : 63;       // capacity (bitfield)
  size_type __is_long_ : 1;    // long mode flag
};

struct __short {
  value_type __data_[23];       // inline character data
  unsigned char __size_ : 7;   // short string length
  unsigned char __is_long_ : 1;// long mode flag (=0)
};

// Key member functions
bool __is_long() const;         // check __is_long_ bit
pointer __get_pointer();        // returns __is_long() ? __data_ : __data_[]
size_type __get_short_size() const;
size_type __get_long_size() const;
size_type __get_short_cap() const { return __min_cap - 1; }
size_type __get_long_cap() const;
```

## Key Algorithms

### vector Capacity Growth Strategy

```cpp
size_type __recommend(size_type __new_size) const {
  const size_type __ms = max_size();
  if (__new_size > __ms)
    this->__throw_length_error();
  const size_type __cap = capacity();
  if (__cap >= __ms / 2)
    return __ms;
  return std::max<size_type>(2 * __cap, __new_size);
}
```

Growth factor is **2×**: `max(2 * capacity, new_size)`. When `capacity >= max_size / 2`, returns `max_size` directly to avoid overflow.

Difference from libstdc++: libstdc++ uses `~1.5×` growth (`capacity + capacity / 2`), libc++ uses `2×`.

### split_buffer Relocation Strategy

```
Original vector: [A B C D E], insert(2, X), capacity insufficient

1. __recommend(6) = 10
2. Construct split_buffer(10, 2):
   ┌──────────────────────────────────────────┐
   │ ? ? ? ? ? ? ? ? ? ?                      │
   │         ↑__begin_=2 (gap position)       │
   └──────────────────────────────────────────┘
3. Relocate second half [D, E] to the end first:
   [? ? ? ? ? ? ? D E ?]
4. Then relocate first half [A, B] to the beginning:
   [A B ? ? ? ? ? D E ?]
5. Construct X at position 2:
   [A B X ? ? ? ? D E ?]
6. __swap_out_circular_buffer: swap pointers, vector now points to new buffer
```

**Why relocate the second half first?** Exception safety: if relocating the first half throws, the second half is already in the correct position; if relocating the second half throws, the first half is still in its original position.

### string SSO Determination

```cpp
bool __is_long() const _NOEXCEPT {
  if (__libcpp_is_constant_evaluated() && __builtin_constant_p(__rep_.__l.__is_long_)) {
    return __rep_.__l.__is_long_;
  }
  return __rep_.__s.__is_long_;
}
```

SSO determination is performed on the hot path of nearly every string operation. libc++ optimizes with bitfields: the lowest bit of the last byte is 0 in short mode, 1 in long mode.

### string Capacity Growth

```cpp
size_type __grow_by(size_type __old_cap,
                    size_type __delta_cap,
                    size_type __old_size,
                    size_type __n_copy,
                    size_type __n_del,
                    size_type __n_add) const;
```

Growth strategy: `max(2 * __old_cap, __old_cap + __delta_cap)`, consistent with vector.

### trivially_relocatable memcpy Optimization

```cpp
// Relocation path selection in vector.h
if constexpr (__libcpp_is_trivially_relocatable<value_type>::value &&
              is_trivially_move_constructible<value_type>::value &&
              is_trivially_destructible<value_type>::value &&
              !__libcpp_is_constant_evaluated()) {
  // __builtin_memcpy to relocate the entire range at once
} else {
  // per-element move_if_noexcept + destroy
}
```

## ABI Constraints

### vector ABI Stability

- vector's layout (three pointers) remains stable across all libc++ versions
- `__split_buffer` is an internal implementation detail, not part of the ABI contract
- `__bounded_iter` (debug iterator) is enabled via the `_LIBCPP_ABI_BOUNDED_ITERATORS_IN_VECTOR` macro

### string ABI Layout Options

| Macro | Layout | Description |
|---|---|---|
| `_LIBCPP_ABI_ALTERNATE_STRING_LAYOUT` | **Default** | `__long` stores `__is_long_` in trailing bitfield, SSO capacity = 22 |
| `_LIBCPP_ABI_STRING_PAIR_LAYOUT` | Optional | `__long` stores `__is_long_` at the beginning, `__short` data first, layout compatible with old ABI |
| `_LIBCPP_ABI_NO_ITERATOR_BASES` | Optional | Iterators don't inherit from `__wrap_iter`, reducing iterator size |

### Endianness Effects

```cpp
#ifdef _LIBCPP_BIG_ENDIAN
  static const size_type __endian_factor = 2;
#else
  static const size_type __endian_factor = 1;
#endif
```

On big-endian systems, the `__cap_` bitfield storage needs to be multiplied by `__endian_factor`, because the highest bit (`__is_long_`) is at the opposite end of the byte on big-endian.

### ABI Compatibility with libstdc++/MSVC

- libc++ string (24 bytes) and libstdc++ string (32 bytes) are **layout-incompatible**
- Passing string objects across libraries causes undefined behavior
- Safe approach: use `const char*` or `string_view` to cross ABI boundaries

## Exception Safety

### vector Exception Safety Guarantees

| Operation | Guarantee | Mechanism |
|---|---|---|
| `push_back(const T&)` | **Strong guarantee** | If expansion needed, allocate split_buffer first then relocate elements; relocation failure leaves original data unchanged |
| `push_back(T&&)` | **Strong guarantee** | Same as above; if move throws, split_buffer destructor cleans up constructed elements |
| `emplace_back(Args...)` | **Strong guarantee** | Same as above; split_buffer guarantees cleanup on placement new failure |
| `insert(pos, n, val)` | **Strong guarantee** | split_buffer gap-in-middle, relocate first then construct; failure leaves original data unchanged |
| `erase(pos)` | **No-throw** | Per-element move assignment + tail destruction, no allocation involved |
| `reserve(n)` | **Strong guarantee** | Allocate new buffer first, then relocate, then release old buffer |
| `operator=(const vector&)` | **Basic guarantee** | Copy-and-swap idiom; copy failure may leave original vector partially modified |
| `operator=(vector&&)` | **No-throw** | If allocator needs propagation, swap pointers; otherwise per-element move assignment |
| `clear()` | **No-throw** | Only destroys elements, does not release memory |

### string Exception Safety Guarantees

| Operation | Guarantee | Mechanism |
|---|---|---|
| `append(const char*, size_t)` | **Strong guarantee** | If expansion needed, allocate new buffer first then copy; failure leaves original string unchanged |
| `operator+=(const string&)` | **Strong guarantee** | Calls `append`, inherits its strong guarantee |
| `replace(pos, len, str)` | **Strong guarantee** | If length change triggers expansion, allocate new buffer first; otherwise in-place replacement |
| `insert(pos, str)` | **Strong guarantee** | If expansion needed, allocate new buffer first then relocate |
| `reserve(n)` | **Strong guarantee** | Allocate new buffer first, then copy, then release old buffer |
| `operator=(const string&)` | **Basic guarantee** | Release old buffer first then copy; copy failure leaves string in valid but unspecified state |
| `operator=(string&&)` | **No-throw** | Swap pointers and metadata, source string reset to empty SSO |
| `clear()` | **No-throw** | Only sets length to 0, does not release memory |

### SSO Mode Exception Safety Advantage

In SSO mode (≤ 22 bytes), string operations don't involve heap allocation, so:
- Construction, assignment, append, etc. are no-throw within SSO
- No `bad_alloc` risk, higher exception safety level

## Iterator / Reference Invalidation

### vector Invalidation Rules

| Operation | Iterator/Pointer/Reference Invalidation | Reason |
|---|---|---|
| `push_back` triggers expansion | **All invalidated** | split_buffer allocates new buffer, old buffer released |
| `push_back` does not trigger expansion | **Not invalidated** | Only constructs new element at `__end_` |
| `insert(pos, n, val)` | **All invalidated** (if expansion) or **pos and after invalidated** (if no expansion) | Expansion reallocates; otherwise elements after pos are relocated |
| `erase(pos)` | **pos and after invalidated** | Elements after pos are moved forward to fill the gap |
| `erase(first, last)` | **first and after all invalidated** | Elements after the erased range are moved forward |
| `reserve(n)` if `n > capacity()` | **All invalidated** | Allocates new buffer, releases old buffer |
| `reserve(n)` if `n ≤ capacity()` | **Not invalidated** | No operation |
| `shrink_to_fit()` | **Possibly all invalidated** | May create a smaller buffer and relocate data |
| `clear()` | **Not invalidated** | Only destroys elements, does not release memory |
| `resize(n)` if `n > capacity()` | **All invalidated** | Triggers expansion |
| `resize(n)` if `n ≤ capacity()` | **Not invalidated** | Constructs or destroys elements in existing buffer |
| `swap()` | **Not invalidated** | Only swaps pointers, element addresses unchanged |

### string Invalidation Rules

| Operation | Iterator/Pointer/Reference Invalidation | Reason |
|---|---|---|
| SSO → heap transition (e.g., `append` causes length > 22) | **All invalidated** | `__get_pointer()` switches from `__data_[]` to heap pointer |
| In-heap expansion (e.g., `append` exceeds current capacity) | **All invalidated** | Allocates new heap buffer, releases old buffer |
| In-heap in-place modification (length unchanged) | **Not invalidated** | Data remains in the same heap buffer |
| `shrink_to_fit()` | **Possibly all invalidated** | In heap mode may reallocate a smaller buffer |
| `clear()` | **Not invalidated** | Only sets length to 0, does not release memory |
| `reserve(n)` if `n > capacity()` | **All invalidated** | Allocates new buffer |
| `swap()` | **All invalidated** | Swaps pointers and metadata |
| `erase()` | **Not invalidated** (reference to erased element invalidated) | Characters moved in-place, length shortened |

### libc++ String Lacks COW Invalidation Pitfalls

libc++ string has used eager copy (deep copy) since C++11, avoiding COW-induced invalidation pitfalls:

```cpp
std::string a = "hello";
const char* p = a.data();
std::string b = a;           // deep copy, a and b have independent data
assert(p == a.data());       // a's pointer is still valid — no COW clone
```

libstdc++ used COW before GCC 5, where the non-const `operator[]` could trigger `_M_unshare()`, invalidating already-acquired iterators.

## Performance Model

### vector Performance Characteristics

| Parameter | libc++ | libstdc++ | MSVC | Impact |
|---|---|---|---|---|
| Growth factor | **2×** | ~1.5× | 1.5× | libc++ has fewer allocations but each is larger; libstdc++/MSVC have better memory utilization |
| Expansion relocation | **memcpy** (trivially relocatable) | move + destroy | move + destroy | libc++ uses a single memcpy for trivial types, faster |
| `sizeof(vector)` | 24 bytes | 24 bytes | 24 bytes | Same three-pointer layout |
| SBO | None | None | None | vector does not use small object optimization |

### Growth Factor Trade-offs

```
2× growth (libc++):
  Allocations = O(log₂ n)
  Memory waste = up to ~50% (unused space in the last allocation)
  Allocation calls = fewer, each allocation larger

1.5× growth (libstdc++/MSVC):
  Allocations = O(log₁.₅ n) ≈ 1.4× more allocations
  Memory waste = up to ~33%
  Better memory reclaim/reuse (memory from earlier allocations may be reused by later ones)
```

### string SSO Performance Characteristics

| Parameter | libc++ | libstdc++ | MSVC | Impact |
|---|---|---|---|---|
| `sizeof(string)` | **24** | 32 | 32 | libc++ object is smaller, cache-friendly |
| SSO capacity | **22** | 15 | 15 | libc++ can inline longer strings |
| SSO construction | memcpy 24 bytes | memcpy 32 bytes | memcpy 32 bytes | libc++ copies less memory |
| Heap construction | malloc + memcpy | malloc + memcpy | malloc + memcpy | All three are the same |
| Move SSO string | memcpy 24 bytes | memcpy 32 bytes | memcpy 32 bytes | SSO cannot steal pointers, must copy |
| Move heap string | Swap pointers | Swap pointers | Swap pointers | O(1) |

### SSO Hit Rate Analysis

```
Typical application string length distribution (paths, emails, identifiers, etc.):
  ≤ 15 bytes: ~85% (libstdc++/MSVC SSO hit)
  ≤ 22 bytes: ~92% (libc++ SSO hit)

libc++ covers an additional 7% of strings → significant performance improvement
in short-string-intensive scenarios (e.g., JSON parsing, log processing)
```

### Cache Line Impact

```
sizeof(string) = 24 bytes:
  One 64-byte cache line can hold 2 string objects (16 bytes of fragmentation)
  String array traversal triggers a cache line load every 2 objects

sizeof(string) = 32 bytes (libstdc++/MSVC):
  One 64-byte cache line can hold 2 string objects (no fragmentation)
  Higher cache utilization
```

## Compilation / Benchmark Evidence

### Verify vector Three-Pointer Layout

```cpp
// vector_layout.cpp
#include <cstdio>
#include <vector>
#include <cstddef>

struct MyStruct {
  int a, b, c;
};

int main() {
  printf("sizeof(vector<int>) = %zu\n", sizeof(std::vector<int>));
  printf("sizeof(vector<MyStruct>) = %zu\n", sizeof(std::vector<MyStruct>));

  std::vector<int> v = {1, 2, 3, 4, 5};
  printf("data = %p\n", (void*)v.data());
  printf("size = %zu, capacity = %zu\n", v.size(), v.capacity());
}
```

Compile and run:
```bash
clang++ -std=c++20 -O2 vector_layout.cpp -o vector_layout && ./vector_layout
# Expected output (x86-64):
# sizeof(vector<int>) = 24
# sizeof(vector<MyStruct>) = 24
# data = 0x...
# size = 5, capacity = 8
```

### Verify SSO Boundary (22 bytes)

```cpp
// sso_boundary.cpp
#include <cstdio>
#include <string>
#include <cstring>

int main() {
  // 22 bytes: SSO
  std::string short_str("0123456789012345678901");  // len = 22
  printf("short: data=%p obj=%p is_long=%d cap=%zu\n",
         (void*)short_str.data(), (void*)&short_str,
         short_str.size() > 22 ? 1 : 0,
         short_str.capacity());

  // 23 bytes: heap allocation
  std::string long_str("01234567890123456789012");   // len = 23
  printf("long:  data=%p obj=%p is_long=%d cap=%zu\n",
         (void*)long_str.data(), (void*)&long_str,
         long_str.size() > 22 ? 1 : 0,
         long_str.capacity());
}
```

Compile and run:
```bash
clang++ -std=c++20 -O2 sso_boundary.cpp -o sso_boundary && ./sso_boundary
# Expected output:
# short: data=0x7fff... obj=0x7fff... is_long=0 cap=22
# long:  data=0x555...   obj=0x7fff... is_long=1 cap=47
```

### vector Expansion Benchmark

```cpp
// vector_bench.cpp
#include <vector>
#include <benchmark/benchmark.h>

static void BM_PushBack(benchmark::State& state) {
  for (auto _ : state) {
    std::vector<int> v;
    for (int i = 0; i < state.range(0); ++i)
      v.push_back(i);
    benchmark::DoNotOptimize(v.data());
  }
}

static void BM_ReservePushBack(benchmark::State& state) {
  for (auto _ : state) {
    std::vector<int> v;
    v.reserve(state.range(0));
    for (int i = 0; i < state.range(0); ++i)
      v.push_back(i);
    benchmark::DoNotOptimize(v.data());
  }
}

BENCHMARK(BM_PushBack)->Range(8, 1 << 20);
BENCHMARK(BM_ReservePushBack)->Range(8, 1 << 20);
BENCHMARK_MAIN();
```

Compile and run:
```bash
clang++ -std=c++20 -O2 -lbenchmark vector_bench.cpp -o vector_bench && ./vector_bench
# Expected results (reference values, x86-64):
# BM_PushBack/1024         ~800 ns
# BM_PushBack/1048576      ~1.2 ms
# BM_ReservePushBack/1024  ~300 ns  ← no expansion overhead
# BM_ReservePushBack/1048576 ~400 μs
```

### string SSO vs Heap Benchmark

```cpp
// string_bench.cpp
#include <string>
#include <benchmark/benchmark.h>

static void BM_StringSSO(benchmark::State& state) {
  for (auto _ : state) {
    std::string s = "hello world!";  // 12 bytes, SSO
    benchmark::DoNotOptimize(s);
  }
}

static void BM_StringHeap(benchmark::State& state) {
  for (auto _ : state) {
    std::string s = "this is a long string that exceeds SSO threshold";  // 48 bytes, heap
    benchmark::DoNotOptimize(s);
  }
}

static void BM_StringAppendSSO(benchmark::State& state) {
  for (auto _ : state) {
    std::string s = "hello";
    s += " world";  // 5+6=11, still within SSO
    benchmark::DoNotOptimize(s);
  }
}

static void BM_StringAppendHeap(benchmark::State& state) {
  for (auto _ : state) {
    std::string s = "hello world! this is a long string";
    s += " and more data";  // triggers heap allocation
    benchmark::DoNotOptimize(s);
  }
}

BENCHMARK(BM_StringSSO);
BENCHMARK(BM_StringHeap);
BENCHMARK(BM_StringAppendSSO);
BENCHMARK(BM_StringAppendHeap);
BENCHMARK_MAIN();
```

Typical benchmark results (reference values, x86-64 Clang 17 -O2):

| Scenario | Time (ns/op) | Notes |
|---|---|---|
| SSO construction (12 bytes) | ~5-8 | Pure memcpy 24 bytes |
| Heap construction (48 bytes) | ~15-25 | Including malloc + memcpy |
| SSO append (11 bytes) | ~5-8 | In-place append, no allocation |
| Heap append triggering expansion | ~30-50 | Including new allocation + copy |

### trivially_relocatable memcpy Verification

```bash
# Check whether vector expansion uses memcpy
clang++ -std=c++20 -O2 -S -masm=intel vector_bench.cpp -o vector_bench.s
grep -A 20 'push_back' vector_bench.s | head -30
# Expected: for trivial types (e.g., int), should see rep movsb or memcpy call instead of a loop
```

## cpplings Exercise Entry Points

- [`vector1` — vector basic operations and expansion](../../../exercises/cpp11-std/vector1.cpp)
- [`stringview1` — std::string_view non-owning string view](../../../exercises/cpp17/stringview1.cpp)
- [`perf1` — Performance optimization techniques: SBO, cache-friendly, string_view](../../../exercises/topics/perf1.cpp)
