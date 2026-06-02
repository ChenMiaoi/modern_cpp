---
title: "C++23"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# C++23

C++23 (ISO/IEC 14882:2024) continues the direction of C++20, further refining and extending the four pillars.

## Core Features

| Feature | Description |
|---------|-------------|
| `std::expected<T,E>` | Return value with error type, alternative to exceptions |
| `std::print` / `std::println` | Modern printing, alternative to `iostream` |
| `std::mdspan` | Multidimensional array view |
| `std::generator` | Standard coroutine generator implementation |
| `deducing this` | Explicit object parameter, eliminates CRTP and redundant code |
| `if consteval` | Conditional branching between compile-time and runtime |
| Multidimensional subscript operator | `matrix[i, j]` syntax |
| `std::flat_map` / `flat_set` | Containers based on sorted vectors |
| `import std` | Import the entire standard library |

## Highlights

### `deducing this`

```cpp
struct Widget {
    // C++20 requires two versions (const and non-const)
    // C++23 handles both with one
    template<typename Self>
    auto&& name(this Self&& self) { return std::forward<Self>(self).name_; }
};
```

### `std::print`

```cpp
// No need for <iostream>, type-safe unlike printf
std::print("Hello, {}! pi = {:.2f}\n", "world", 3.14159);
```

### `import std`

```cpp
// One line replaces all #include
import std;
```

## Compiler Support

| Compiler | Support Status |
|----------|---------------|
| GCC | 14+ (partial features) |
| Clang | 18+ (partial features) |
| MSVC | VS 2022 17.8+ (partial features) |

## Further Reading

- [std::expected](/standards/cpp23/expected)
- [std::print](/standards/cpp23/print)
- [Deducing this](/standards/cpp23/deducing-this)
