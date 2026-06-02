---
title: "C++14 Generic Lambda"
topic: unknown
feature: generic-lambda
standard: N/A
status_checked_at: 2026-06-02
---
# C++14 Generic Lambda

## Overview

C++11 introduced lambda expressions, but parameter types had to be explicitly specified. C++14 allows lambda parameters to use the `auto` keyword, making lambdas automatically generic — equivalent to a function object with template parameters. This eliminates the need to write multiple lambdas or explicit Functor classes for similar operations on different types.

## Syntax

```cpp
// C++11 lambda — parameter types must be explicit
auto add = [](int a, int b) { return a + b; };

// C++14 generic lambda — uses auto parameters
auto add = [](auto a, auto b) { return a + b; };

// Equivalent explicit template Functor
struct Add {
    template <typename T, typename U>
    auto operator()(T a, U b) const { return a + b; }
};
```

Each `auto` parameter corresponds to a separate template parameter, and the compiler generates an `operator()` overload for each combination of argument types.

## Code Examples

### Basic Usage

```cpp
#include <iostream>
#include <string>

int main() {
    // Same lambda handles different types
    auto print = [](auto const& val) {
        std::cout << val << '\n';
    };

    print(42);          // int
    print(3.14);        // double
    print("hello");     // const char*
    print(std::string("world")); // std::string
}
```

### With STL Algorithms

```cpp
#include <algorithm>
#include <vector>
#include <string>

// Generic search: any container, any value type
auto contains = [](auto const& container, auto const& value) {
    return std::find(container.begin(), container.end(), value)
         != container.end();
};

void demo() {
    std::vector<int> vi = {1, 2, 3, 4, 5};
    std::vector<std::string> vs = {"alpha", "beta", "gamma"};

    contains(vi, 3);        // true
    contains(vs, std::string("beta")); // true
}
```

### Combining Generic Capture with Generic Parameters

```cpp
#include <functional>

auto make_adder = [](auto x) {
    // Returns a closure that captures the value of x
    return [x](auto y) { return x + y; };
};

void demo() {
    auto add5 = make_adder(5);
    add5(3);       // 8 — int
    add5(2.5);     // 7.5 — double
}
```

### Multi-parameter Generic Lambda with Perfect Forwarding

```cpp
#include <utility>
#include <iostream>

auto perfect_call = [](auto&& func, auto&&... args) {
    return std::forward<decltype(func)>(func)(
        std::forward<decltype(args)>(args)...
    );
};

void greet(const char* name, int times) {
    for (int i = 0; i < times; ++i)
        std::cout << "Hello, " << name << "!\n";
}

void demo() {
    perfect_call(greet, "World", 3);
}
```

## How the Compiler Handles Generic Lambdas

The compiler transforms a generic lambda into a closure type where `operator()` is a member template:

```cpp
// What you write:
auto lam = [](auto a, auto b) { return a + b; };

// What the compiler generates (simplified):
struct __closure_type {
    template <typename T, typename U>
    auto operator()(T a, U b) const { return a + b; }
};
```

As a result, the same lambda instantiates different function bodies for different argument types.

## Best Practices

1. **Prefer generic lambdas over redundant Functor classes**: When behavior is simple and needs to be reused across types, generic lambdas are more concise than hand-written Functor classes.
2. **Understand the difference between `auto&&` and `auto`**: Pass-by-value copies; use `auto const&` or `auto&&` for pass-by-reference. `auto const&` or `auto&&` is recommended for generic code.
3. **Avoid over-generalization**: If a lambda is only used for one type, explicit types are clearer and produce better compile-time error messages.
4. **Generic lambdas cannot be virtual**: Closure types are unique anonymous types; their `operator()` template cannot be declared `virtual`.
5. **When using with `std::invoke` / `std::function`**: `std::function` requires a fixed signature; generic lambdas cannot be directly stored in `std::function` unless specific template arguments are provided.
6. **C++20 simplification**: C++20 allows `auto` as a regular function parameter (abbreviated function template), narrowing the syntactic advantage of generic lambdas, but in C++14/17 they remain the only approach.
