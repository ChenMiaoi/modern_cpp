---
title: "C++17 Structured Bindings"
topic: unknown
feature: structured-bindings
standard: N/A
status_checked_at: 2026-06-02
---
# C++17 Structured Bindings

## Overview

Structured bindings are a syntactic feature introduced in C++17 that allow the members of a composite object (`pair`, `tuple`, array, aggregate struct) to be destructured and bound to multiple variables in a single statement. The syntax `auto [a, b] = expr;` eliminates the verbose approach of using `std::tie` or manually accessing members one by one.

## Basic Syntax

```cpp
auto [id_1, id_2, ..., id_n] = expression;    // value binding
auto& [id_1, id_2, ..., id_n] = expression;   // reference binding
const auto& [id_1, id_2, ..., id_n] = expr;   // const reference binding
```

The number of binding variables must match the number of accessible members in the object.

## Binding to pair and tuple

```cpp
#include <utility>
#include <tuple>
#include <iostream>

int main() {
    std::pair<int, std::string> p{42, "hello"};
    auto [val, str] = p;
    std::cout << val << ", " << str << "\n"; // 42, hello

    std::tuple<double, int, char> t{3.14, 7, 'X'};
    auto [d, i, c] = t;

    // reference binding—modifications reflect on the original object
    auto& [dref, iref, cref] = t;
    iref = 99;
    std::cout << std::get<1>(t) << "\n"; // 99
}
```

## Binding to Arrays

```cpp
int arr[] = {10, 20, 30};
auto [x, y, z] = arr;       // x=10, y=20, z=30

auto& [rx, ry, rz] = arr;   // reference binding
rx = 100;                    // arr[0] is now 100

// std::array works the same way
std::array<int, 3> a = {1, 2, 3};
auto [a0, a1, a2] = a;
```

## Binding to Aggregate Structs

```cpp
struct Point {
    double x;
    double y;
    double z;
};

Point pt{1.0, 2.0, 3.0};
auto [px, py, pz] = pt;  // bound by member order
```

Requires the type to be a **complete type**. The number of bindings must exactly equal the number of non-static data members—binding only a subset is not allowed.

## Usage in map Iteration

```cpp
#include <map>
#include <string>

std::map<std::string, int> scores = {
    {"Alice", 95}, {"Bob", 87}, {"Carol", 92}
};

// much clearer than it->first / it->second
for (const auto& [name, score] : scores) {
    std::cout << name << ": " << score << "\n";
}
```

This is one of the most common use cases for structured bindings.

## Reference Binding vs. Value Binding

```cpp
std::tuple<int, std::string> t{3, "edit"};

// value binding: independent copy
auto [val, str] = t;

// const reference binding: can extend temporary object lifetime
const auto& [cval, cstr] = std::tuple<int, std::string>{2, "temp"};

// non-const reference binding: in-place modification
auto& [mval, mstr] = t;
mstr = "edited";  // the string in t is modified
```

Note: `auto&` cannot bind to temporaries, while `const auto&` and `auto&&` can extend their lifetime.

## Underlying Mechanism

Structured bindings are compiler syntactic sugar. For aggregate types, the compiler generates a hidden variable `e`, and each binding variable references the corresponding member of `e`:

```cpp
// auto [a, b] = point;
// equivalent to (conceptually):
auto __e = point;
auto& a = __e.x;
auto& b = __e.y;
```

For `tuple`-like types, `std::get<I>(__e)` is used; for arrays, `__e[I]` is used.

## Best Practices

- **Prefer for `map`/`unordered_map` iteration** — greatly improves readability.
- **When functions return multiple values**, use `tuple` or struct with structured bindings for clearer code than output parameters.
- **When modification of the original object is needed**, use `auto&` or `const auto&` binding.
- **Avoid value binding for large objects** — use reference binding to avoid copies.
- **When certain members should be ignored**, fall back to `std::tie` with `std::ignore`.

## Common Pitfalls

```cpp
// Pitfall 1: bit-field members cannot use structured bindings
struct Flags { unsigned int read : 1; unsigned int write : 1; };
Flags f{1, 0};
// auto [r, w] = f;  // error: bit-fields cannot be referenced

// Pitfall 2: count mismatch
// auto [a, b] = std::tuple<int,int,int>{1,2,3};  // error: 3 members but only 2 bindings

// Pitfall 3: value binding does not modify the original object
std::pair<int,int> p{1, 2};
auto [a, b] = p;
a = 99;
// p.first is still 1—a is an independent copy

// Pitfall 4: vector does not support structured bindings
// auto [a, b, c] = std::vector{1, 2, 3};  // error: not an aggregate/tuple-like type
```
