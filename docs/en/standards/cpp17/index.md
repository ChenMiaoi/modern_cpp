---
title: "C++17"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# C++17

C++17 (ISO/IEC 14882:2017) is a major update that introduces a large number of highly practical language features and library components, further improving the expressiveness and safety of C++.

## Core Features

### Language Level

| Feature | Description |
|---------|-------------|
| Structured Bindings | `auto [x, y] = pair;` |
| `if constexpr` | Compile-time branching, replacing SFINAE hacks |
| Fold Expressions | `(args + ...)` simplifies variadic templates |
| Class Template Argument Deduction (CTAD) | `pair p(1, 2.0);` no explicit template arguments needed |
| Inline Variables | The correct way to define global variables in header files |
| Nested Namespaces | `namespace A::B::C {}` |
| `[[nodiscard]]` / `[[maybe_unused]]` / `[[fallthrough]]` | More attributes |

### Standard Library Level

| Component | Description |
|-----------|-------------|
| `std::optional` | Value-semantic container that may be empty |
| `std::variant` | Type-safe union |
| `std::any` | Container for any type |
| `std::string_view` | Non-owning string view, zero-copy |
| `<filesystem>` | Standardized filesystem operations |
| Parallel Algorithms | `std::execution::par` execution policies |
| `std::apply` | Apply function to tuple |
| `std::byte` | Type-safe byte type |
| `std::clamp` | Numeric clamping |
| `std::invoke` | Unified call syntax |
| `std::from_chars` / `std::to_chars` | High-performance numeric conversion |
| `std::conjunction` / `disjunction` / `negation` | Type trait logical composition |

## Design Philosophy

The design philosophy of C++17 can be summarized as "practicality first":

- **Eliminate boilerplate**: Structured bindings and CTAD reduce redundant declarations
- **Zero-overhead abstractions**: `string_view` and `optional` introduce no runtime overhead
- **Compile-time capabilities**: `if constexpr` makes template code more readable
- **Safety**: `[[nodiscard]]` helps catch ignored return values

## Compiler Support

| Compiler | Full Support Version |
|----------|---------------------|
| GCC | 9+ |
| Clang | 8+ |
| MSVC | VS 2019 (16.0)+ |

## Further Reading

- [Structured Bindings](/standards/cpp17/structured-bindings)
- [std::optional](/standards/cpp17/optional)
- [std::string_view](/standards/cpp17/string-view)
