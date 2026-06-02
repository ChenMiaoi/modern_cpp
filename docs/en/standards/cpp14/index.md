---
title: "C++14"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# C++14

C++14 (ISO/IEC 14882:2014) is an incremental update to C++11, primarily completing the "unfinished business" from C++11 rather than introducing entirely new programming paradigms.

## Core Features

| Feature | Description |
|---------|-------------|
| Generic Lambda | Lambda parameters can use `auto` |
| Return Type Deduction | Functions can omit trailing return types |
| Variable Templates | `template<typename T> constexpr T pi = T(3.14159...);` |
| Binary Literals | `0b1010` syntax |
| Digit Separators | `1'000'000` for improved readability |
| `std::make_unique` | Fills the missing factory function from C++11 |
| `std::exchange` | Generic swap-and-return operation |
| `std::shared_timed_mutex` | Shared mutex with timeout support |

## Positioning

The mission of C++14 is to make C++11's design more pleasant to use. Generic lambdas and return type deduction make "writing generic code" more concise, but do not change the fundamental paradigm established by C++11.

## Compiler Support

| Compiler | Full Support Version |
|----------|---------------------|
| GCC | 5.1+ |
| Clang | 3.4+ |
| MSVC | VS 2017 (15.0)+ |

## Further Reading

- [Generic Lambda](/standards/cpp14/generic-lambda)
- [Return Type Deduction](/standards/cpp14/return-type-deduction)
- [Variable Templates](/standards/cpp14/variable-templates)
