---
title: "C++11"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# C++11

C++11 (ISO/IEC 14882:2011) is the most milestone version in C++ history, known as the starting point of "modern C++." It brought fundamental changes at both the language and standard library levels.

## Core Changes

### Language Level

| Feature | Significance |
|---------|-------------|
| `auto` type deduction | Reduces redundant type declarations |
| Rvalue references and move semantics | Eliminates unnecessary deep copies, greatly improving performance |
| Lambda expressions | First-class closures, simplifying STL algorithm usage |
| Variadic templates | Type-safe variadic functions, metaprogramming building blocks like `enable_if` |
| `constexpr` | Starting point for compile-time computation |
| `nullptr` | Type-safe null pointer constant |
| `enum class` | Scoped enumerations, eliminating implicit conversions |
| `static_assert` | Compile-time assertions |
| Delegating constructors | Constructor reuse |
| User-defined literals | Custom `operator""` syntax sugar |

### Standard Library Level

| Component | Significance |
|-----------|-------------|
| `unique_ptr` / `shared_ptr` / `weak_ptr` | Replaces `auto_ptr`, correct resource ownership semantics |
| `std::thread` / `std::mutex` / `std::atomic` | First standardized multithreading support |
| `std::array` | Fixed-size array, replacing C-style arrays |
| `std::tuple` | Heterogeneous container |
| `<chrono>` | Type-safe time library |
| `<regex>` | Regular expressions |
| `<random>` | High-quality random numbers |
| Unordered containers | `unordered_map`, `unordered_set` |

## Why It Was a Watershed Moment

C++ before C++11 (C++98/03) and C++ after C++11 differ fundamentally in coding style:

- **Move semantics** changed the economics of pass-by-value — returning by value is no longer expensive
- **Lambdas** made algorithm-first coding possible over hand-written loops
- **`auto`** made iterators and template return values no longer a nightmare
- **Concurrency** went from "platform-dependent" to "language-guaranteed"

## Compiler Support

| Compiler | Full Support Version |
|----------|---------------------|
| GCC | 4.8.1+ |
| Clang | 3.3+ |
| MSVC | VS 2015 (19.0)+ |

## Further Reading

Select a specific feature from the sidebar to begin reading, or start directly with [auto type deduction](/standards/cpp11/auto-type-deduction).
