---
title: "C++20"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# C++20

C++20 (ISO/IEC 14882:2020) is the most influential release since C++11, introducing four "cornerstone" features: **Concepts**, **Ranges**, **Coroutines**, and **Modules**.

## The Four Cornerstones

### Concepts

Apply named constraints to template parameters, replacing SFINAE and `static_assert`:

```cpp
template<typename T>
concept Sortable = requires(T a, T b) {
    { a < b } -> std::convertible_to<bool>;
};

void sort(Sortable auto& container);
```

### Ranges

Lazy-evaluation-based pipeline data processing:

```cpp
auto result = numbers
    | views::filter([](int n) { return n > 0; })
    | views::transform([](int n) { return n * n; })
    | views::take(10);
```

### Coroutines

Low-level support for stackless coroutines (`co_await`, `co_yield`, `co_return`), providing language-level support for asynchronous programming and the generator pattern. High-level wrappers in the standard library (`std::generator`) were deferred to C++23.

### Modules

A modular compilation system that replaces `#include`, addressing header compilation efficiency and macro pollution problems.

## Other Important Features

| Feature | Description |
|---------|-------------|
| `<=>` three-way comparison operator | Automatic generation of comparison operators |
| `consteval` / `constinit` | More precise compile-time semantics |
| `std::format` | Type-safe formatting library (Python-style) |
| `std::span` | Non-owning sequence view over contiguous memory |
| `std::jthread` | Thread with automatic join |
| `std::latch` / `std::barrier` / `std::counting_semaphore` | Synchronization primitives |
| `std::source_location` | Replaces `__FILE__`/`__LINE__` |
| `std::ranges` algorithms | Range-based find, sort, transform |
| `std::erase` / `std::erase_if` | Uniform container erasure |
| Calendars and time zones | Major extensions to the `chrono` library |

## Compiler Support

| Compiler | Support Status |
|----------|----------------|
| GCC | 10+ (core features), 12+ more complete |
| Clang | 15+ (Modules still experimental) |
| MSVC | VS 2022 (17.0)+ |

## Further Reading

- [Concepts](/standards/cpp20/concepts)
- [Ranges](/standards/cpp20/ranges)
- [Coroutines](/standards/cpp20/coroutines)
- [Modules](/standards/cpp20/modules)
