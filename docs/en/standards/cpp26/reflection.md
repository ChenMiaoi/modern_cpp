---
title: C++26 Reflection
topic: cpp26
feature: reflection
standard: C++26
status_checked_at: 2026-06-02
proposal:
  main: P2996
  revision: R7
  checked_at: 2026-06-02
syntax_status: accepted
compiler_support:
  clang: experimental (-freflection)
  gcc: experimental branch
  msvc: in progress
exercises: []
solutions: []
---

# C++26 Reflection

## Overview

Reflection is C++26's compile-time type introspection mechanism, generating meta-objects via `^` and injecting code via `[: :]` (splice). It is one of the most significant language features in C++ history.

**Proposal status:** P2996R7 has been accepted for C++26. P3394 (annotations) is still evolving.

## Meta-objects and the `^` Operator

```cpp
#include <meta>

constexpr auto meta_int = ^int;
constexpr auto meta_vec = ^std::vector<int>;
constexpr auto meta_ns  = ^std;          // Namespace meta-object
constexpr auto meta_fn  = ^std::sort;    // Function meta-object
```

Meta-objects are opaque and can only be used through query functions and splice.

## Query Functions

```cpp
#include <meta>
#include <print>

enum class Color { Red, Green, Blue, Alpha };

constexpr auto colors = std::meta::enumerators_of(^Color);
static_assert(colors.size() == 4);
constexpr auto first = std::meta::name_of(colors[0]);  // "Red"
```

| Function | Returns | Description |
|----------|---------|-------------|
| `name_of(^T)` | `string_view` | Name |
| `members_of(^T)` | `vector<info>` | All members |
| `enumerators_of(^T)` | `vector<info>` | Enum values |
| `nonstatic_data_members_of(^T)` | `vector<info>` | Data members |
| `type_of(member)` | `info` | Member type |
| `value_of<T>(^e)` | `T` | Compile-time constant value |

## Splice Operator `[: :]`

```cpp
using T = [:^int:];           // T = int
Point p{3, 4};
[:^int:] val = 42;            // int val = 42;

constexpr auto mems = std::meta::nonstatic_data_members_of(^Point);
```

## Enum to String

```cpp
#include <meta>
#include <print>

enum class Status { Pending, Active, Suspended, Closed };

consteval std::string_view enum_to_string(Status s) {
    template for (constexpr auto e : std::meta::enumerators_of(^Status)) {
        if (s == [:e:]) return std::meta::name_of(e);
    }
    return "unknown";
}

std::println("{}", enum_to_string(Status::Active));  // Active
```

No manual mapping tables or macros needed; adding new enum values will not be missed.

## Automated Serialization

```cpp
#include <meta>
#include <string>
#include <sstream>

struct User { std::string name; int age; bool active; };

template <typename T>
std::string to_json(T const& obj) {
    std::ostringstream os; os << "{";
    bool first = true;
    template for (constexpr auto mem : std::meta::nonstatic_data_members_of(^T)) {
        if (!first) os << ", "; first = false;
        os << "\"" << std::meta::name_of(mem) << "\": ";
        if constexpr (std::is_same_v<[: std::meta::type_of(mem) :], std::string>)
            os << "\"" << obj.[:mem:] << "\"";
        else if constexpr (std::is_same_v<[: std::meta::type_of(mem) :], bool>)
            os << (obj.[:mem:] ? "true" : "false");
        else os << obj.[:mem:];
    }
    os << "}"; return os.str();
}
// to_json(User{"Alice", 30, true}) => {"name": "Alice", "age": 30, "active": true}
```

## Comparison with Template Metaprogramming

| Dimension | Traditional Template Metaprogramming | C++26 Reflection |
|-----------|--------------------------------------|------------------|
| Syntax | SFINAE, `if constexpr` | `^`, `[: :]`, `template for` |
| Enum traversal | Requires macros or external tools | Native `enumerators_of` |
| Member traversal | Requires tupleification | `nonstatic_data_members_of` |
| Learning curve | Extremely steep | Moderate |

Reflection complements, rather than replaces, template metaprogramming.

## Related Proposals

- **P2996**: Core reflection + splice
- **P3394**: Annotation syntax `[[=attr(...)]]`
- **P3436**: Reflection-related constexpr extensions
- **P3068**: constexpr exceptions

## Implementation Status

| Compiler | Status |
|----------|--------|
| Clang/LLVM | Reference implementation, `-freflection` experimental |
| GCC | Experimental branch |
| MSVC | In progress |

## Summary

C++26 Reflection brings compile-time type introspection through `^` and `[: :]`. Combined with `template for` and constexpr container extensions, tasks that previously required extensive template techniques — such as enum-to-string conversion and automated serialization — become concise and direct.
