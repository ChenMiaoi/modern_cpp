---
title: "C++98"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# C++98

C++98 (ISO/IEC 14882:1998) is the first international standard for the C++ language. Building upon C with Classes and early C++ practice, it formally established the core framework of the language.

## Historical Background

- **Standardization Date**: 1998
- **Predecessor**: Bjarne Stroustrup began the design in 1979, going through early implementations such as Cfront
- **Significance**: Unified generic programming, object-oriented programming, and systems-level programming capabilities in a single language

## Core Features

| Feature | Description |
|---------|-------------|
| Classes and Inheritance | Single inheritance, multiple inheritance, virtual functions, abstract base classes |
| Templates | Function templates, class templates (but not all partial specialization scenarios) |
| Exception Handling | `try`/`catch`/`throw` mechanism |
| Namespaces | `namespace` for avoiding name conflicts |
| RTTI | `typeid`, `dynamic_cast` runtime type identification |
| `const` and References | `const` correctness, lvalue references |
| Operator Overloading | Supports custom operator semantics for user-defined types |
| `new` / `delete` | Dynamic memory management (replacing C's `malloc`/`free`) |

## Standard Library Components

- **STL Containers**: `vector`, `list`, `deque`, `map`, `set`, `stack`, `queue`
- **Iterators**: Five-category iterator hierarchy
- **Algorithms**: `sort`, `find`, `transform`, `accumulate`, etc.
- **Function Objects**: `less`, `greater`, `bind1st`/`bind2nd`
- **Strings**: `std::string`
- **I/O Streams**: `iostream`, `fstream`, `sstream`
- **Smart Pointers**: `auto_ptr` (deprecated)

## Limitations

- Limited template capabilities, no support for explicit instantiation control of external templates
- Lack of a unified memory model and multithreading support
- `auto_ptr`'s copy semantics have pitfalls
- No `nullptr`; `0` or `NULL` is used to represent null pointers
- Implicit conversion of enumeration types, which can easily lead to name conflicts
- Modern features such as lambdas and type deduction are not available

## Further Reading

- [Language Features](/standards/cpp98/features)
- [Standard Library](/standards/cpp98/standard-library)
