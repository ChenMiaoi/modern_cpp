---
title: "Optimization & Performance Idioms"
topic: unknown
feature: optimization-terms
standard: N/A
status_checked_at: 2026-06-02
---
# Optimization & Performance Idioms

## Copy Elision

The compiler omits unnecessary copy/move constructions. Since C++17, copy elision for prvalues is **mandatory**.

## SBO (Small Buffer Optimization)

Small objects are stored inline on the stack, avoiding heap allocation:

```cpp
// Typical SBO for std::function: 24-byte stack buffer
// a lambda capturing 2 pointers (16B) → SBO, no heap allocation
// a lambda capturing 3 pointers (24B) → may require heap allocation
```

## SSO (Small String Optimization)

SSO is a special case of SBO — short strings are stored inline:

```
libc++:     SSO = 22 bytes, sizeof(string) = 24
libstdc++:  SSO = 15 bytes, sizeof(string) = 32
fbstring:   SSO = 23 bytes, sizeof(string) = 24
```

## COW (Copy-On-Write)

Multiple objects share the same data block; copying only occurs on writes. Deprecated in the standard library after C++11 due to thread-safety issues — COW implementations of `std::string` are non-conforming in C++11.

## Devirtualization

The compiler replaces a virtual function call with a direct call when it can determine the dynamic type of the object:

```cpp
Derived d;
Base& b = d;
b.virtual_func();  // compiler may directly call Derived::virtual_func()
```

GCC/Clang perform devirtualization automatically via LTO and `-fdevirtualize`.

## Branch Prediction

The CPU guesses which direction a conditional branch will take before the result is known:

```cpp
// [[likely]] / [[unlikely]] (C++20) help the compiler optimize branches
if (condition) [[unlikely]] {
  error_handling();
} else [[likely]] {
  normal_path();
}
```

## Cache-Friendly Design

```
  Container choice and cache behavior:

  std::vector  → contiguous memory     → very fast traversal (prefetch-friendly)
  std::list    → scattered nodes       → slow traversal (cache misses)
  flat_map     → sorted array          → fast lookup (binary search + contiguous memory)
  std::map     → red-black tree        → node hopping (cache misses)

  Rules of thumb:
  - small containers (<1000 elements) → vector + sort + binary_search is almost always fastest
  - medium containers → flat_map/flat_set
  - large containers + frequent insert/delete → unordered_map or map
```

## Inline

The compiler replaces a function call with the function body — eliminating call overhead and enabling further optimizations:

```cpp
inline int square(int x) { return x * x; }
// advisory but not mandatory — the compiler has its own inlining heuristics

[[gnu::always_inline]] void hot_path();  // force inline (GCC/Clang)
__declspec(noinline) void cold_path();   // prevent inline (MSVC)
```

## Prefetch

Tells the CPU to load data into cache in advance:

```cpp
// GCC/Clang built-in
__builtin_prefetch(&data[i + 64]);  // prefetch data that will be accessed in the future
```

## LTO (Link-Time Optimization)

Link-time optimization — enables cross-translation-unit inlining, dead code elimination, and constant propagation. ThinLTO (LLVM) supports parallel link-time optimization and is the recommended choice for production environments.
