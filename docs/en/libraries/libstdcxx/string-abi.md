---
title: libstdc++ string ABI：从 COW 到 SSO
topic: libraries
feature: string-abi
standard: C++11
status_checked_at: 2026-06-02
implementation:
  libstdcxx:
    path: references/impl/gcc/libstdc++-v3/include/bits/basic_string.h
    symbols:
      - std::basic_string
      - _M_dataplus
      - _M_local_buf
      - _M_allocated_capacity
exercises: []
solutions: []
---
# libstdc++ string: ABI Migration from COW to SSO

## The COW Era (GCC 4.x, ABI v1)

libstdc++ used copy-on-write before C++11. **The string object itself contains only 1 pointer** (4 bytes on a 32-bit system!):

```
 string a            string b            string c
 ┌────────────┐      ┌────────────┐      ┌────────────┐
 │  _M_p  ●───┼──┐   │  _M_p  ●───┼──┐   │  _M_p  ●───┼──┐
 └────────────┘  │   └────────────┘  │   └────────────┘  │
                 │                   │                   │
                 ▼                   ▼                   │
 Heap memory: ┌───────────────────────────┐                 │
              │ _M_length    = 5          │                 │
              │ _M_capacity  = 15         │  Shared block    │
              │ _M_refcount  = 3  ◄───────┼─────────────────┘
              ├───────────────────────────┤
              │ H  e  l  l  o  \0        │ ← _M_p points here
              └───────────────────────────┘
```

### The Fatal Problem with COW (Non-compliant After C++11)

```cpp
string a = "hello";
char& c = a[0];   // Obtain reference
string b = a;      // COW: b and a share data, refcount=2
c = 'H';           // Write triggers lazy copy → b's data is affected
                   // But c references a[0], COW implementation may violate the standard
```

### Global Empty String Instance

```
 ┌────────────────────────┐
 │ _M_length   = 0        │
 │ _M_capacity = 0        │
 │ _M_refcount = -1       │ ← Never released
 ├────────────────────────┤
 │ \0                     │
 └────────────────────────┘
 All empty strings' _M_p → this global instance
```

## SSO Migration (GCC 5.0, ABI v2)

```
  basic_string object (32 bytes)
  ┌───────────────────────────────────────────────────────────────────────┐
  │ Offset   0 ──  7  │ _M_dataplus._M_p       (pointer to actual char data)       │
  │ Offset   8 ── 15  │ _M_dataplus._M_alloc   (empty allocator, EBO compressed)   │
  │ Offset  16 ── 23  │ union: _M_string_length │ _M_local_buf[16] first 8 bytes   │
  │ Offset  24 ── 31  │ _M_allocated_capacity  │ _M_local_buf[16] last 8 bytes    │
  └───────────────────────────────────────────────────────────────────────┘

  Short (≤15): _M_p points to its own _M_local_buf
  Long (>15):  _M_p points to heap allocation

  SSO capacity = 16 - 1 = 15 bytes  (7 bytes less than libc++'s 22)
  sizeof = 32 bytes              (8 bytes more than libc++'s 24)
```

## Dual ABI Coexistence

```
  -D_GLIBCXX_USE_CXX11_ABI=0          Default (=1)
  ┌──────────────────────────────┐    ┌────────────────────────────┐
  │ namespace std {              │    │ namespace std {            │
  │   inline namespace __cxx11 { │    │   // (no inline namespace) │
  │     class basic_string;      │    │   class basic_string;      │
  │   }                          │    │ }                          │
  │ }                            │    │                            │
  └──────────────────────────────┘    └────────────────────────────┘

  ABI v1: _ZNSt12basic_stringIcSt11char_traitsIcESaIcEE...
  ABI v2: _ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE...
                   ^^^^^^^  namespace injection + [abi:cxx11] tag
```

The `abi_tag` attribute encodes the ABI tag into the symbol name, allowing both ABI versions of the string type to coexist within the same .so.

**The cost of ABI stability**: libstdc++ had to retain suboptimal implementations. For example, `std::list::size()` must be O(1) (ABI-locked), even though this forces `splice` to perform O(n) count updates.

## User API

From the user's perspective, what they see is `std::string` construction, concatenation, modification, and `data()/c_str()` access; the existing body of this article primarily explains the COW→SSO ABI migration behind these APIs.

## Standard Semantics

C++11 imposed several semantic constraints on `std::basic_string` that directly ended the compliance of COW implementations:

| Standard Requirement | Clause | Conflict with COW Implementation (ABI v1) |
|---|---|---|
| Contiguous storage | \[string.require\] | COW shared blocks are inherently contiguous, but `_M_mutate()` on write may reallocate, causing old pointers to dangle |
| `operator[]` returns `reference` / `const_reference` | \[string.access\] | COW `operator[]` non-const overload needs to trigger `_M_unshare()` to detach a copy; if the caller holds a prior `const_reference`, the referenced memory may have changed |
| `data()` returns `const char*` (C++11); C++17 adds non-const `data()` | \[string.accessors\] | COW `data()` may return the shared buffer address; non-const `data()` requires writable contiguous memory, contradicting shared semantics |
| Non-mutating operations do not invalidate references/pointers/iterators | \[string.iterators\] | COW's `operator[] const`, `begin() const`, etc. may trigger `_M_unshare()` (if the rep is shared), indirectly causing memory reallocation and invalidating previously obtained references |
| `c_str()` and `data()` return the same pointer | \[string.accessors\] | In COW implementations the two are semantically equivalent, but if `_M_unshare()` triggers between them, the pointer returned by `c_str()` may be invalidated |

```cpp
// C++11 standard requires this code to always be correct without UB
std::string a = "hello";
const char& r = a[0];      // Obtain const reference
std::string b = a;          // Copy
assert(&r == &a[0]);        // Reference still points to a's data—may not hold under COW if r triggered unshare

// C++17's non-const data() requires writable contiguous memory
std::string s = "test";
char* p = s.data();         // Legal in C++17, p points to writable memory
*p = 'T';                   // Must not trigger any copy-on-write
```

COW implementations had dedicated defect reports during the C++11 standard draft phase (LWG 2268, etc.); ultimately C++11 explicitly prohibited the coexistence of reference semantics and COW. GCC 5.0 (2015) switched to the SSO+deep-copy model upon release, fully satisfying these requirements.

## Object Layout

The COW single-pointer layout, the SSO-era 32-byte object layout, and dual ABI symbols have already been covered above; a side-by-side v1/v2 offset diagram will be added later.

## Core Source Paths

The libstdc++ string implementation is spread across the following header files:

| File | Responsibility |
|---|---|
| `include/bits/basic_string.h` | Main template `basic_string<CharT,Traits,Alloc>` definition, SSO layout, all inline member functions |
| `include/bits/basic_string.tcc` | Out-of-line member function template implementations (`_M_create`, `_M_mutate`, `replace`, `find` family, etc.) |
| `include/bits/allocator.h` | `std::allocator` template; `basic_string` uses EBO to compress the empty allocator to reduce object size |
| `include/bits/cow-string.h` (deprecated) | Pre-GCC 5 COW implementation, containing `_Rep`, `_M_refcount`, `_M_is_shared()`, etc.; the ABI v1 path still references some types from this file |
| `include/bits/c++config.h` | ABI fork point: defines the default value of `_GLIBCXX_USE_CXX11_ABI` (defaults to `1` on GCC 5+), and the `inline namespace` injection of `namespace __cxx11` |

**ABI v1/v2 fork logic**: `basic_string.h` expands into two paths based on the `_GLIBCXX_USE_CXX11_ABI` macro—
- `=1` (default): Uses SSO layout, `basic_string` defined in `inline namespace __cxx11`, symbols carry `[abi:cxx11]` tag
- `=0`: Uses old COW layout, `basic_string` defined directly in namespace `std`, symbols in `_ZNSs...` format

You can check the current ABI version via the `_GLIBCXX_USE_CXX11_ABI` macro:
```cpp
#include <bits/c++config.h>
static_assert(_GLIBCXX_USE_CXX11_ABI == 1, "Need ABI v2");
```

## Core Classes / Functions

### ABI v2 (SSO Era) Core Members

```cpp
// Actual data members of basic_string (simplified, allocator traits details omitted)
struct _Alloc_hider : allocator_type {   // EBO: empty allocator takes no space
    pointer _M_p;                         // Pointer to actual character data
};

_Alloc_hider _M_dataplus;                // Offset 0: pointer + allocator (EBO)
size_type    _M_string_length;            // Offset 8: current length
union {
    char       _M_local_buf[16];          // SSO buffer (capacity 15 + 1 byte '\0')
    size_type  _M_allocated_capacity;     // Stores allocated capacity in heap mode
};
```

| Member | Offset | Size | Description |
|---|---|---|---|
| `_M_dataplus._M_p` | 0 | 8 bytes | Points to `_M_local_buf` in SSO mode; points to `new char[]` in heap mode |
| `_M_dataplus._M_alloc` (EBO) | — | 0 bytes | Default `allocator<char>` is an empty class, takes no space after EBO compression |
| `_M_string_length` | 8 | 8 bytes | Shared between SSO and heap modes, always stores the current string length |
| `_M_local_buf[16]` | 16 | 16 bytes | SSO buffer; shares a union with `_M_allocated_capacity` |
| `_M_allocated_capacity` | 16 | 8 bytes | Stores allocated capacity in heap mode (alternate union view) |

### Key Member Functions

| Function | Purpose |
|---|---|
| `_M_is_local()` | Checks if `_M_dataplus._M_p == _M_local_buf`, i.e., whether in SSO mode |
| `_M_data()` | Returns `_M_dataplus._M_p` (pointer to actual character data) |
| `_M_set_length(n)` | Sets `_M_string_length = n` and writes `'\0'` at `data()[n]` |
| `_M_capacity()` | Returns `15` for SSO; returns `_M_allocated_capacity` for heap |
| `_M_create(capacity, old_cap)` | Allocates `capacity+1` bytes of heap memory, returns new pointer |
| `_M_dispose()` | No-op for SSO; calls `_M_destroy()` to release heap memory in heap mode |
| `_M_mutate(pos, len1, s, len2)` | Core mutation primitive: creates new buffer if SSO or insufficient capacity, moves/copies characters |
| `_M_leak_hard()` | Legacy function from ABI v2 (called `_M_rep()->_M_set_leaked()` in the COW era, no-op in SSO era) |

### ABI v1 (COW Era) Historical Members

```cpp
// COW layout: string object contains only 1 pointer
struct _Rep {                        // Metadata block on the heap
    size_type   _M_length;
    size_type   _M_capacity;
    _Atomic_word _M_refcount;        // Atomic reference count
    // Character data follows immediately after _Rep
};

char* _M_p;                          // Points to character data after _Rep
```

| Member/Function | Description |
|---|---|
| `_Rep::_M_refcount` | Atomic reference count; `-1` means never released (global empty string instance) |
| `_Rep::_M_is_shared()` | When `_M_refcount > 0`, indicates data is shared among multiple strings |
| `_M_rep()` | Reverse-computes the `_Rep*` address from `_M_p` (at `_M_p - sizeof(_Rep)`) |
| `_M_grab(alloc, alloc2)` | COW core: if rep is unshared, reuse directly; otherwise `_M_clone()` deep copy |
| `_M_leak()` | Marks rep as leaked (`_M_refcount = -1`), breaking the sharing |

## Key Algorithms

### SSO / Long Mode Switching

```
_M_is_local() determination:
  _M_dataplus._M_p == (pointer)_M_local_buf ?
  ├─ true  → Short mode: data on stack, capacity = 15
  └─ false → Long mode: data on heap, capacity = _M_allocated_capacity
```

Decision path during construction/assignment:
1. `size <= 15` → Write directly into `_M_local_buf`, `_M_p` points to self
2. `size > 15` → Call `_M_create(size+1, 0)` for heap allocation, copy data to heap buffer

### Capacity Growth Strategy

```cpp
// _M_check_len(n) computation logic (simplified)
size_type _M_check_len(size_type n) const {
    if (max_size() - size() < n)
        __throw_length_error("basic_string::_M_check_len");
    size_type new_size = size() + std::max(size(), n);  // max(2*current, current+needed)
    return std::min(new_size, max_size());               // Does not exceed max_size()
}
```

The growth factor is **2x** (consistent with libc++), but capped by `max_size()`. The actual allocated byte count is `new_size + 1` (including the null terminator `'\0'`).

### Historical COW Copy-on-Write Path (ABI v1)

```cpp
// Before any operation that may modify data, check shared state
void _M_check_mutate() {
    if (_M_rep()->_M_is_shared())   // refcount > 0
        _M_mutate(0, 0, 0);         // Detach copy: allocate new buffer, copy data, refcount--
}
```

The COW call chain:
`operator[]` (non-const) → `_M_check_mutate()` → `_M_is_shared()` → If shared, `_M_mutate()` → `_M_clone()` → New `_Rep` + copy characters

Empty string optimization: All empty strings share a global `_Rep` (`_M_refcount = -1`, never released), avoiding any heap allocation for zero-length strings.

### ABI Symbol Selection

At compile time, `_GLIBCXX_USE_CXX11_ABI` determines the symbol name:

| ABI Version | Mangled name for `std::string` | Inline Namespace |
|---|---|---|
| v1 (`=0`) | `_ZNSsC1Ev` (constructor) | None, directly `std::basic_string` |
| v2 (`=1`) | `_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEC1Ev` | `std::__cxx11::basic_string` |

The `__cxx11` segment in v2 symbols is automatically injected by GCC's `abi_tag` attribute, with no manual encoding needed.


## ABI Constraints

This article's topic is itself ABI; the constraints checklist for `__cxx11` namespace, `abi_tag`, `_GLIBCXX_USE_CXX11_ABI`, and mixed binary boundaries will be filled in here later.

## Exception Safety

### Exception Safety in the SSO Era (ABI v2)

| Operation | Guarantee | Mechanism |
|---|---|---|
| `reserve()` | **Strong guarantee** | Allocates new buffer via `_M_create()` first, copies data to new buffer, then releases old buffer; if allocation throws, original data is unchanged |
| `append(const char* s, size_type n)` | **Strong guarantee** | Computes required capacity via `_M_check_len()`; if beyond current capacity, allocates new buffer first then writes; original string unchanged if allocation fails |
| `replace(pos, len, s, n)` | **Strong guarantee** (no-throw when length unchanged) | If replacement length is unchanged and no capacity expansion is triggered, modifies in-place (no-throw); otherwise detaches new buffer via `_M_mutate()` |
| `operator=(const basic_string&)` | **Basic guarantee** | Calls `_M_dispose()` on old buffer before assignment, then copies new content; if copy throws, string is in a valid but unspecified state |
| `operator=(basic_string&&)` | **No-throw** | Move semantics: swaps pointer and length, sets `_M_p` to `_M_local_buf` (empty SSO), source string becomes empty |
| `clear()` | **No-throw** | Only calls `_M_set_length(0)`, does not release memory |

### Exception Safety Differences in the COW Era (ABI v1)

COW's `_M_mutate()` (copy detachment) has subtle exception safety issues:

```cpp
// COW's append path (simplified)
void append(const char* s, size_type n) {
    _M_mutate(size(), n, s, n);  // Step 1: If rep is shared, clone, then expand capacity
    // Step 2: Write new characters
    // Problem: When clone fails inside _M_mutate, the original rep's refcount has already been decremented
    // If clone throws an exception, the original string may point to a freed rep
}
```

Specifically, COW's `_M_mutate()` performs these steps:
1. Checks `_M_is_shared()`; if shared, calls `_M_clone()` to create a new `_Rep` (may throw `std::bad_alloc`)
2. After `_M_clone()` succeeds, the old rep's `_M_refcount` is decremented
3. If `_M_refcount` drops to 0, the old rep is freed
4. `_M_p` points to the new rep's character region

If step 1's `_M_clone()` throws an exception, the old rep's refcount has already been decremented (from 2 to 1), but `_M_p` still points to the old rep. This means the string object remains valid, but the sharing relationship has been broken—other strings holding the old rep will see a refcount that is 1 less. This is a **basic guarantee** (rather than a strong guarantee), because the string's internal state has undergone an irreversible change.

## Iterator / Reference Invalidation

### Invalidation Rules in the SSO Era (ABI v2)

| Operation | Do Iterators/Pointers/References Invalidate? | Reason |
|---|---|---|
| SSO → heap transition (e.g., `append` causes length > 15) | **All invalidated** | `_M_p` switches from `_M_local_buf` to a newly allocated heap address, all pointers become invalid |
| Heap expansion (e.g., `append` causes capacity overflow) | **All invalidated** | `_M_create()` allocates new buffer, releases old buffer |
| In-place heap modification (length unchanged, e.g., `operator[]` write) | **Not invalidated** | Data remains in the same heap buffer |
| `shrink_to_fit()` | **Possibly all invalidated** | May reallocate a smaller buffer in heap mode; no-op in SSO mode (not invalidated) |
| `clear()` | **Not invalidated** | Only calls `_M_set_length(0)`, capacity and buffer unchanged |
| `reserve(n)` if `n > capacity()` | **All invalidated** | Allocates new buffer, releases old buffer |
| `swap()` | **All invalidated** | Swaps pointer and length, original references point to the other string's data |
| `erase()` | **Not invalidated** (but references to erased elements are invalid) | Characters are moved in-place, length shortened, buffer unchanged |

### Additional Pitfalls in the COW Era (ABI v1)

In COW implementations, **non-mutating operations can also cause invalidation**:

```cpp
std::string a = "hello";
std::string b = a;           // COW: a and b share the same rep, refcount=2
const char* p = a.c_str();   // Get a's data pointer
char c = b[0];               // b's operator[] const triggers _M_check_mutate()
                             // If _M_is_shared() returns true, b clones a new rep
                             // a's refcount drops to 1, but p is still valid—no problem this round

// The real pitfall:
std::string c = a;
char& r = a[0];              // Obtain non-const reference → triggers _M_mutate(), a may clone
// If clone occurs, r references old rep's data, which may be dangling
```

Key difference:
- SSO era: **Only capacity-changing operations cause invalidation**—invalidation boundaries are clear and predictable
- COW era: **Any non-const access may trigger clone**—even non-const `begin()` may invalidate iterators
- COW's `c_str()` had a famous cache invalidation bug in GCC 4.x: `operator[] const` would corrupt the buffer returned by `c_str()`

### Standard Specification (§\[string.iterators\])

The standard requires:
- Non-mutating operations (`begin()` const, `end()` const, `data()` const, `operator[]` const, `c_str()`) **must not** invalidate iterators, references, or pointers
- Mutating operations only invalidate iterators when they cause reallocation

COW implementations cannot satisfy the first requirement—this is one of the core reasons C++11 forced the ABI change.

## Performance Model

### SSO Performance Characteristics

| Parameter | libstdc++ (GCC) | libc++ (Clang) | Impact |
|---|---|---|---|
| `sizeof(string)` | 32 bytes | 24 bytes | libstdc++'s `union { _M_local_buf[16]; _M_allocated_capacity; }` takes 8 more bytes than libc++'s `__short_` union |
| SSO capacity | 15 bytes | 22 bytes | libc++ overlaps `size_type` (8 bytes) with pointer (8 bytes), freeing more inline space |
| SSO trigger threshold | `len <= 15` | `len <= 22` | Approximately 85% of real-world strings are ≤ 15 bytes (paths, email addresses, identifiers, etc.); both cover most cases |
| Cache line occupancy | 32 bytes = half a cache line (64B) | 24 bytes < half a cache line | Arrays of 32-byte strings fill exactly one cache line per two objects; 24-byte strings leave 16 bytes of fragmentation |

### Copy Performance Tradeoffs

```
COW era (ABI v1):
  Copy string a → b:
    1. Copy _M_p (1 pointer)                  ← Extremely fast
    2. _M_refcount.fetch_add(1)                ← Atomic operation, ~10-50ns
    3. _M_clone() only on write (deep copy)    ← Deferred to first modification
  Issue: Every write has an _M_is_shared() check branch; atomic operations cause cache-line bouncing under multi-threading

SSO era (ABI v2):
  Copy string a → b:
    If SSO: memcpy 16 bytes (_M_local_buf)     ← Equivalent to 2 × 8-byte load+store
    If heap: malloc + memcpy(size)             ← Every copy is a deep copy
  Benefit: No atomic operations, no shared-state check branches, no cache-line bouncing
```

For short strings (≤ 15 bytes), SSO's deep copy is actually faster than COW—memcpy 16 bytes is faster than a `fetch_add` atomic operation.

### ABI Compatibility Cost

The cost of dual ABI coexistence:
1. **Binary size**: Every `basic_string` member function has two symbol versions (v1 + v2), increasing string-related code size by approximately 10-15%
2. **Link time**: Larger symbol tables, linker needs to process more symbols
3. **Template instantiation**: If translation units use different ABI macros, the same `basic_string<T>` instantiation will produce two different sets of code
4. **No mixing allowed**: v1's `std::string` and v2's `std::__cxx11::basic_string` are different types—passing one to the other's functions will cause link failure (rather than silent UB)

### Comparison with MSVC STL

MSVC's `std::string` (VS 2015+):
- SSO capacity: 15 bytes (`_BUF_SIZE = 16`)
- Object size: 32 bytes (same as libstdc++)
- No COW historical baggage (MSVC never implemented COW)
- No dual ABI version problem (MSVC's ABI is locked by compiler version number, e.g., `_MSC_VER`)


## libstdc++ vs libc++ vs MSVC

The main text has already covered some differences in SSO capacity and object size; a comparison of all three string implementations covering layout, growth strategy, debug modes, and ABI strategy will be added here later.

## Minimal Reproduction Code

```cpp
#include <string>

int main() {
  std::string s = "hello";
  s += " world";
  return static_cast<int>(s.size());
}
```

## Compile / Disassemble / Benchmark Evidence

### Verifying the SSO / Long Boundary

```cpp
// sso_boundary.cpp — Verify the 15-byte SSO threshold
#include <cstdio>
#include <string>
#include <cstring>

int main() {
    // 15 bytes: SSO
    std::string short_str("0123456789abcde");  // len = 15
    printf("short: data=%p local=%p is_local=%d cap=%zu\n",
           (void*)short_str.data(), (void*)&short_str,
           short_str.data() == (const char*)&short_str,
           short_str.capacity());

    // 16 bytes: heap allocation
    std::string long_str("0123456789abcdef");   // len = 16
    printf("long:  data=%p local=%p is_local=%d cap=%zu\n",
           (void*)long_str.data(), (void*)&long_str,
           long_str.data() == (const char*)&long_str,
           long_str.capacity());
}
```

Compile and run:
```bash
g++ -std=c++20 -O2 sso_boundary.cpp -o sso_boundary && ./sso_boundary
# Expected output:
# short: data=0x7fff... local=0x7fff... is_local=1 cap=15
# long:  data=0x555...   local=0x7fff... is_local=0 cap=31
```

### Dual ABI Symbol Name Verification

```bash
# Compile the same test program with v1 and v2 ABI respectively
g++ -std=c++20 -O2 -D_GLIBCXX_USE_CXX11_ABI=1 string_test.cpp -o v2
g++ -std=c++20 -O2 -D_GLIBCXX_USE_CXX11_ABI=0 string_test.cpp -o v1

# Inspect v2 symbols: contains __cxx11 namespace
objdump -t v2 | grep basic_string | head -5
# Expected: _ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE...
#               ^^^^^^^ ABI tag

# Inspect v1 symbols: directly in std namespace
objdump -t v1 | grep basic_string | head -5
# Expected: _ZNSs... (shorthand) or _ZNSt12basic_stringIcSt11char_traitsIcESaIcEE...
#                            No __cxx11
```

### Object Size and Layout Verification

```bash
# Compile a program with both ABIs and compare object sizes
cat > string_size.cpp << 'EOF'
#include <cstdio>
#include <string>
struct WithString { std::string s; int x; };
int main() {
    printf("sizeof(std::string)  = %zu\n", sizeof(std::string));
    printf("sizeof(WithString)   = %zu\n", sizeof(WithString));
    printf("SSO capacity         = %zu\n", std::string().capacity());
}
EOF
g++ -std=c++20 -O2 string_size.cpp -o string_size && ./string_size
# Expected (libstdc++ ABI v2, x86-64):
# sizeof(std::string)  = 32
# sizeof(WithString)   = 40  (32 + 4 + 4 padding)
# SSO capacity         = 15
```

### append Hot Path Disassembly

```bash
# Inspect the inline path of append (fast path in SSO mode)
cat > append_bench.cpp << 'EOF'
#include <string>
#include <benchmark/benchmark.h>
static void BM_AppendSSO(benchmark::State& state) {
    for (auto _ : state) {
        std::string s = "hello";
        s += " world";          // 5+6=11, still within SSO
        benchmark::DoNotOptimize(s);
    }
}
static void BM_AppendHeap(benchmark::State& state) {
    for (auto _ : state) {
        std::string s = "hello world! this is a long string";
        s += " and more data";  // Triggers heap allocation
        benchmark::DoNotOptimize(s);
    }
}
BENCHMARK(BM_AppendSSO);
BENCHMARK(BM_AppendHeap);
BENCHMARK_MAIN();
EOF
# Compile and run benchmark
g++ -std=c++20 -O2 -lbenchmark append_bench.cpp -o append_bench && ./append_bench
```

Disassemble to inspect the SSO fast path:
```bash
g++ -std=c++20 -O2 -S -masm=intel append_bench.cpp -o append_bench.s
# Search for SSO append key instructions: should see memcpy/mov operations instead of _M_create calls
grep -A 20 'append' append_bench.s | head -30
```

### Typical Benchmark Results (Reference Values, x86-64 GCC 13 -O2)

| Scenario | Latency (ns/op) | Description |
|---|---|---|
| SSO construct + copy (11 bytes) | ~5-8 | Pure memcpy 32 bytes |
| Heap construct + copy (64 bytes) | ~20-30 | Includes malloc + memcpy |
| `s += "abc"` within SSO | ~8-12 | In-place append, no allocation |
| `s += "abc"` triggers expansion | ~40-60 | Includes realloc/new allocation + copy |
| COW construct + copy (ABI v1, 64 bytes) | ~15-25 | Only pointer copy + atomic operation |
| COW `s += "abc"` (shared state) | ~30-50 | Includes clone + append |

SSO is 2-3x faster than COW for short string scenarios; COW's copy is cheaper for long strings, but the clone overhead on write may negate the benefit in hot paths.

## cpplings Exercise Entry Points

- [`stringview1` — std::string_view Non-owning String View](../../../exercises/cpp17/stringview1.cpp)
- [`perf1` — Performance Optimization Techniques: SBO, Cache Friendliness, string_view](../../../exercises/topics/perf1.cpp)
