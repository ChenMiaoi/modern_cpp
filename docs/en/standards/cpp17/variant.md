---
title: C++17 std::variant
topic: cpp17
feature: variant
standard: C++17
status_checked_at: 2026-06-01
exercises:
  - exercises/cpp17/variant1.cpp
solutions:
  - exercises/solutions/variant1.cpp
---
# C++17 std::variant

## Overview

`std::variant<Types...>` is a type-safe tagged union introduced in C++17 within `<variant>`. Unlike a C `union`, `std::variant` tracks the currently stored type at compile time and provides type-safe access. It can hold a value of **exactly one** type from its template parameter list, defaulting to the default value of the first type.

## Creation and Assignment

```cpp
#include <variant>
#include <string>

// default construction: holds the default value of the first type
std::variant<int, double, std::string> v1;  // int{0}

// value initialization
std::variant<int, double, std::string> v2{42};
std::variant<int, double, std::string> v3{3.14};
std::variant<int, double, std::string> v4{"hello"};       // const char*
std::variant<int, double, std::string> v5{std::string{"hello"}}; // explicit string

// use in_place_type for ambiguous types
std::variant<int, long, double> v{std::in_place_type<long>, 42L};

// assignment changes the active type
v1 = "new value";  // changes to string
v1 = 100;          // changes back to int
```

## Accessing Values: std::get and std::get_if

```cpp
std::variant<int, double, std::string> v{3.14};

// access by type or index—throws std::bad_variant_access on type mismatch
double d  = std::get<double>(v);  // OK
double d2 = std::get<1>(v);       // index 1 corresponds to double

try {
    int i = std::get<int>(v);     // currently double, throws exception
} catch (const std::bad_variant_access& e) {
    std::cout << e.what() << "\n";
}

// get_if: returns pointer, returns nullptr on mismatch
auto* p = std::get_if<std::string>(&v);
if (p) std::cout << *p << "\n";
```

## Type Checking: std::holds_alternative

```cpp
std::variant<int, double, std::string> v{42};

if (std::holds_alternative<int>(v)) {
    std::cout << "int: " << std::get<int>(v) << "\n";
}

// v.index() returns the zero-based index of the active type
std::cout << v.index() << "\n";  // 0
```

## std::visit with Overloaded Lambda Pattern

```cpp
#include <variant>
#include <iostream>

// helper template: combines multiple lambdas into an overload set
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;  // C++17 deduction guide

void demo() {
    std::variant<int, double, std::string> v{"hello"};

    std::visit(overloaded{
        [](int i) { std::cout << "int: " << i << "\n"; },
        [](double d) { std::cout << "double: " << d << "\n"; },
        [](const std::string& s) { std::cout << "string: " << s << "\n"; }
    }, v);
}
```

`overloaded` inherits the `operator()` of all lambdas — this is a widely adopted community idiom.

### Fallback Handling and Multiple Variants

```cpp
// fallback: use auto&& to match remaining types
std::visit(overloaded{
    [](int i) { /* specialized */ },
    [](auto&& x) { /* fallback */ }
}, v);

// visiting multiple variants simultaneously
std::variant<int, std::string> a{1};
std::variant<double, std::string> b{"world"};
std::visit(overloaded{
    [](int ia, const std::string& sb) { /* ... */ },
    [](const auto& x, const auto& y) { /* ... */ }
}, a, b);
```

## std::monostate: Representing an Empty State

`std::variant` always holds some value. Use `std::monostate` (a zero-byte empty struct) when "no value" is needed:

```cpp
std::variant<std::monostate, int, std::string> v;  // defaults to monostate

if (std::holds_alternative<std::monostate>(v)) {
    std::cout << "empty\n";
}

v = 42;               // holds int
v = std::monostate{}; // back to "empty"
```

## valueless_by_exception

When assigning or emplacing, if constructing the new value throws an exception, the variant may enter the `valueless_by_exception` state:

```cpp
std::variant<int, std::string> v{42};
// if the string constructor throws, v.valueless_by_exception() becomes true
// afterward std::get will throw, and all holds_alternative checks return false

if (v.valueless_by_exception()) {
    std::cout << "variant is in error state\n";
}
```

Ensuring that types in the variant have `noexcept` move constructors minimizes this risk.

## Comparison with union and any

| Feature | C `union` | `std::variant` | `std::any` |
|---------|-----------|-----------------|------------|
| Type-safe | No | Yes (compile-time) | Yes (runtime) |
| Type set | Compile-time fixed | Compile-time fixed | Runtime arbitrary |
| Access method | Direct member | `get`/`visit` | `any_cast` |
| Non-trivial types | Supported since C11 | Fully supported | Fully supported |
| Performance | Minimal overhead | Index + value storage | Possible heap allocation |

## Best Practices

- **Prefer `overloaded + visit`** over `if-else` chains of `holds_alternative`.
- **Ensure types have `noexcept` move constructors** to avoid `valueless_by_exception`.
- **Use `monostate`** as the first type to make a default-constructible variant.
- **Replace virtual function calls**: when the type set is fixed, `variant + visit` avoids indirect calls and heap allocation.

## Common Pitfalls

```cpp
// Pitfall 1: type ambiguity
// std::variant<int, long> v = 42;  // ambiguous
std::variant<int, long> v{42};       // OK: matches int

// Pitfall 2: default initialization is not "empty"
std::variant<int, std::string> v;
// v.index() == 0, std::get<0>(v) == 0
// the first type must be default-constructible

// Pitfall 3: non-copyable types affect the entire variant
// variant<int, std::unique_ptr<int>> v;
// auto v2 = v;  // error: unique_ptr is not copyable

// Pitfall 4: don't use string literals to trigger ambiguity
std::variant<std::string, const char*> v{"hi"}; // selects const char*
std::variant<std::string, const char*> v2{std::string{"hi"}}; // explicitly specifies string
```
