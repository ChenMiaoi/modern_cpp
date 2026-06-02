---
title: "if consteval"
topic: unknown
feature: if-consteval
standard: N/A
status_checked_at: 2026-06-02
---
# if consteval

C++23 introduces `if consteval`, allowing constexpr functions to explicitly distinguish between compile-time and runtime execution paths, replacing `std::is_constant_evaluated()` from C++20.

## Basic Syntax

```cpp
constexpr int compute(int n) {
    if consteval {
        // Compile-time path: all operations must be constexpr-valid
        return n * n;
    } else {
        // Runtime path: non-constexpr operations are allowed
        return n * n;
    }
}
```

## Difference from if constexpr

```cpp
// if constexpr — discards a branch during template instantiation
template <typename T>
void f(T t) {
    if constexpr (std::is_integral_v<T>) { /* ... */ }
    else { /* ... */ }
}

// if consteval — determines context on each call
constexpr int g(int n) {
    if consteval { return n * 2; }   // Compile-time takes this path
    else { return n * 3; }           // Runtime takes this path
}

constexpr int a = g(10);    // Compile-time: a = 20
int x = 10;
int b = g(x);               // Runtime: b = 30
```

| Property | `if constexpr` | `if consteval` |
|----------|---------------|----------------|
| When decided | Template instantiation time | On each call |
| Criterion | Type / compile-time constant | Whether the current context is constant-evaluated |
| Purpose | Conditional compilation | constexpr function optimization |

## Replacing std::is_constant_evaluated()

```cpp
// C++20
constexpr int abs_old(int n) {
    if (std::is_constant_evaluated()) return n < 0 ? -n : n;
    else return std::abs(n);
}

// C++23 — clearer, no need for <type_traits>
constexpr int abs_new(int n) {
    if consteval { return n < 0 ? -n : n; }
    else { return std::abs(n); }
}
```

## constexpr Function Optimization

The most practical scenario: use a safe but slow implementation at compile time, and a fast implementation at runtime:

```cpp
constexpr void constexpr_sort(int* first, int* last) {
    for (auto it = first; it != last; ++it)
        for (auto jt = it + 1; jt != last; ++jt)
            if (*jt < *it) std::swap(*it, *jt);
}

constexpr void smart_sort(int* first, int* last) {
    if consteval {
        constexpr_sort(first, last);  // Compile-time: slow but constexpr-safe
    } else {
        std::sort(first, last);       // Runtime: fast but not constexpr
    }
}
```

### Hash Computation

```cpp
constexpr uint32_t hash(std::string_view s) {
    if consteval {
        uint32_t h = 2166136261u;
        for (char c : s) { h ^= static_cast<uint32_t>(c); h *= 16777619u; }
        return h;
    } else {
        return runtime_hash(s.data(), s.size());  // Can use SIMD optimizations, etc.
    }
}
```

## if consteval Without else

```cpp
constexpr int process(int n) {
    if consteval {
        if (n < 0) throw "negative not allowed at compile time";
    }
    return n * 2;  // Executed in both contexts
}
```

## Caveats

- The `if consteval` branch cannot call non-constexpr functions, even if it is not executed at runtime
- Without `else`, the subsequent code executes in both contexts
- Usage in non-constexpr functions is valid, but the `consteval` branch never executes at runtime
- Unlike `if constexpr`: the former checks "whether the current context is compile-time", while the latter checks "whether a compile-time constant is true"
- `std::is_constant_evaluated()` is still available in C++23, but `if consteval` is the recommended approach
