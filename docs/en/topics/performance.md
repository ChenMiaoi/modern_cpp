---
title: Performance Optimization
topic: topics
feature: performance
status_checked_at: 2026-06-02
standard: N/A
---

# Performance Optimization

## Move Semantics and Avoiding Copies

Move semantics allow transferring resource ownership from one object to another, avoiding expensive deep copies:

```cpp
std::vector<int> create_large_vector() {
    std::vector<int> v(1000000);
    std::iota(v.begin(), v.end(), 0);
    return v;  // NRVO or move, O(1)
}

// Accept by value — caller can choose move or copy
void process(std::vector<int> data) { /* ... */ }
process(create_large_vector());        // move
process(std::move(my_vec));            // move

// Perfect forwarding — preserves value category
template <typename T>
void wrapper(T&& arg) { target(std::forward<T>(arg)); }

// ⚠️ Do not use std::move on return values — it prevents NRVO
std::string make_string() {
    std::string s = "hello";
    return s; // ✅ allows compiler optimization (NRVO)
}
```

## Small String Optimization (SSO)

Most standard library implementations use SSO for `std::string`: short strings (typically ≤ 15–22 bytes) are stored directly inside the object, avoiding heap allocation.

```cpp
std::string short_str = "hello";  // zero heap allocation (SSO buffer)
std::string long_str(100, 'x');   // one heap allocation
// GCC: empty capacity = 15, libc++: 22
```

SSO implications: short string operations are fast (no heap allocation), but `sizeof(std::string)` is larger (24 bytes for libc++, 32 bytes for libstdc++/MSVC), and moving a short string may require a copy (data is inline).

## Cache-Friendly Data Structures

CPU cache effects on performance often far exceed algorithmic complexity. O(n) traversal of contiguous memory is typically much faster than O(log n) traversal of scattered nodes.

```cpp
// Cache-unfriendly — linked list nodes scattered across the heap
struct ListNode { int value; ListNode* next; };

// SoA (Structure of Arrays) — most friendly for SIMD and cache prefetch
struct Particles { std::vector<float> x, y, z; };
// When only x is needed, fills entire cache line; AoS wastes 2/3 of space

// C++23: flat_map — sorted vector instead of red-black tree
#include <flat_map>
std::flat_map<int, std::string> fm; // several times faster for small datasets

// Object pool — avoids fragmented allocation
template <typename T, std::size_t BlockSize = 4096>
class ObjectPool {
    struct Block { alignas(T) char data[sizeof(T) * BlockSize]; };
    std::vector<std::unique_ptr<Block>> blocks_;
    std::size_t used_ = 0;
public:
    template <typename... Args>
    T* allocate(Args&&... args) {
        if (used_ >= BlockSize * blocks_.size())
            blocks_.push_back(std::make_unique<Block>());
        auto* block = blocks_.back().get();
        auto offset = used_ % BlockSize;
        ++used_;
        return new (block->data + offset * sizeof(T)) T(std::forward<Args>(args)...);
    }
};
```

## Branch Prediction Hints

C++20's `[[likely]]`/`[[unlikely]]` help the compiler optimize branch layout:

```cpp
int process(int value) {
    if (value >= 0) [[likely]] { return value * 2; }
    else [[unlikely]] { throw std::out_of_range("negative"); }
}

// GCC/Clang legacy approach
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
```

Affects code layout (hot/cold path separation), does not control conditional move instruction generation.

## SIMD Vectorization

> **Status note** (2026-06-02): `std::simd` has been incorporated into the C++26 draft (P1928) and has been voted in. GCC 14+ provides `<experimental/simd>` as an experimental implementation (namespace `std::experimental`). Clang/LLVM and MSVC do not yet provide full support. The examples below use the TS namespace; switch to `std::simd` once compilers provide official support.

```cpp
#include <experimental/simd>
namespace stdx = std::experimental;

void vector_add(const float* a, const float* b, float* out, std::size_t n) {
    using V = stdx::native_simd<float>; // 4(SSE)/8(AVX)/16(AVX-512)
    constexpr auto w = V::size();
    std::size_t i = 0;
    for (; i + w <= n; i += w) {
        // ⚠️ vector_aligned requires pointers to satisfy native_simd alignment requirements
        // In practice, ensure a/b/out are allocated with V::alignment() alignment
        V va(a + i, stdx::vector_aligned);
        V vb(b + i, stdx::vector_aligned);
        (va + vb).copy_to(out + i, stdx::vector_aligned);
    }
    for (; i < n; ++i) out[i] = a[i] + b[i]; // scalar tail handling
}
```

Compile example (GCC 14+): `g++ -std=c++26 -O3 -march=native -I/path/to/experimental/simd`

## Allocator-Aware Containers

```cpp
// C++17 PMR allocator
#include <memory_resource>
char buffer[4096];
std::pmr::monotonic_buffer_resource pool{buffer, sizeof(buffer)};
std::pmr::vector<int> vec{&pool};  // zero system calls

// Thread-local reuse avoids hot-path allocation
thread_local std::string buf;
void hot_path(const char* input) {
    buf.clear(); buf.append(input);
}
```

## Benchmarking Tools

```cpp
// Google Benchmark — industry standard
#include <benchmark/benchmark.h>
static void BM_PushBack(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<int> v;
        for (int i = 0; i < state.range(0); ++i) v.push_back(i);
        benchmark::DoNotOptimize(v.data());
    }
}
BENCHMARK(BM_PushBack)->Range(8, 1 << 20);
```

- **quick-bench.com**: online comparative benchmarking
- **perf** (Linux): `perf stat -e cache-misses,branch-misses ./bench`
- **Tracy Profiler**: real-time frame-level profiling, cross-platform

## Agner Fog Guidelines

```cpp
struct alignas(64) CacheLineAligned { float data[16]; }; // cache line aligned

// __restrict__ hints to the compiler that vectorization is safe
void add(float* __restrict__ a, const float* __restrict__ b, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) a[i] += b[i];
}
```

**Core discipline: measure first, optimize second.** Without benchmark data, any optimization is guesswork.
