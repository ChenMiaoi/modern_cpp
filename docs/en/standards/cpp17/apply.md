---
title: C++17 std::apply
topic: unknown
feature: std-apply
standard: N/A
status_checked_at: 2026-06-20
---
# C++17 std::apply

## Overview

`std::apply` is a free function introduced in C++17 within `<tuple>` that **applies a callable object to the elements of a tuple**. It expands the tuple into a function argument list, eliminating the boilerplate of manually writing `std::get<0>(t), std::get<1>(t), ...`. Its implementation is based on `std::index_sequence` for compile-time index expansion.

## Function Signature

```cpp
#include <tuple>
#include <utility>

template <class F, class Tuple>
constexpr decltype(auto) apply(F&& f, Tuple&& t);
```

- `f`: Any callable (function pointer, lambda, `std::bind` expression, member function pointer, etc.)
- `t`: A tuple-like object (`std::tuple`, `std::pair`, `std::array`)

## How It Works

The core of `std::apply` uses `std::index_sequence` to generate indices at compile time and expand the tuple:

```cpp
// Simplified implementation
template <class F, class Tuple, size_t... I>
decltype(auto) apply_impl(F&& f, Tuple&& t, std::index_sequence<I...>) {
    return std::forward<F>(f)(std::get<I>(std::forward<Tuple>(t))...);
}

template <class F, class Tuple>
decltype(auto) apply(F&& f, Tuple&& t) {
    constexpr auto size = std::tuple_size_v<std::remove_cvref_t<Tuple>>;
    return apply_impl(std::forward<F>(f), std::forward<Tuple>(t),
                      std::make_index_sequence<size>{});
}
```

`std::index_sequence<I...>` generates `0, 1, 2, ...` at compile time. Combined with pack expansion `std::get<I>(t)...`, each element is passed as an independent argument.

## Basic Usage

```cpp
#include <tuple>
#include <iostream>
#include <string>

int add(int a, int b) {
    return a + b;
}

void greet(std::string name, int age) {
    std::cout << "Hello " << name << ", age " << age << "\n";
}

int main() {
    auto t1 = std::make_tuple(3, 4);
    int result = std::apply(add, t1);
    std::cout << "add result: " << result << "\n";  // 7

    auto t2 = std::make_tuple("Alice", 30);
    std::apply(greet, t2);  // Hello Alice, age 30
}
```

## Using with Lambdas

```cpp
#include <tuple>
#include <iostream>

int main() {
    auto t = std::make_tuple(1, 2.0, "three");

    // Lambda receives expanded arguments
    std::apply([](auto&&... args) {
        ((std::cout << args << " "), ...);
        std::cout << "\n";
    }, t);  // 1 2 three
}
```

## Working with pair and array

```cpp
#include <tuple>
#include <utility>
#include <array>

int multiply(int a, int b) { return a * b; }

int main() {
    // std::pair also works
    std::pair<int, int> p{5, 6};
    int r1 = std::apply(multiply, p);  // 30

    // std::array also works
    std::array<int, 2> arr{7, 8};
    int r2 = std::apply(multiply, arr);  // 56
}
```

## Practical Use Cases

### Replacing Manual emplace

```cpp
#include <tuple>
#include <vector>
#include <string>

struct Task {
    int id;
    std::string name;
    double priority;
};

int main() {
    std::vector<Task> tasks;

    // Use std::apply with emplace
    auto args = std::make_tuple(1, "build", 0.5);
    std::apply([&tasks](auto&&... a) {
        tasks.emplace_back(std::forward<decltype(a)>(a)...);
    }, args);
}
```

### Batch Invocation over Containers

```cpp
#include <tuple>
#include <vector>
#include <iostream>

void process(int x, int y, int z) {
    std::cout << x << "," << y << "," << z << "\n";
}

int main() {
    std::vector<std::tuple<int, int, int>> data = {
        {1, 2, 3},
        {4, 5, 6},
        {7, 8, 9}
    };

    for (const auto& row : data) {
        std::apply(process, row);
    }
}
```

### Constructing Objects with std::make_from_tuple

```cpp
#include <tuple>
#include <string>
#include <iostream>

struct Config {
    std::string host;
    int port;
    bool verbose;

    Config(std::string h, int p, bool v)
        : host(std::move(h)), port(p), verbose(v) {}
};

int main() {
    auto args = std::make_tuple("localhost", 8080, true);
    Config cfg = std::make_from_tuple<Config>(std::move(args));
}
```

## Relationship with std::invoke

`std::apply` and `std::invoke` (C++17) are complementary:

| Feature | `std::apply` | `std::invoke` |
|---------|-------------|---------------|
| Argument passing | Expanded from tuple | Passed directly |
| Member function pointer | Needs tuple pairing | Natively supported |
| Primary use case | Batch/tuple-driven calls | Unified call syntax |

```cpp
#include <tuple>
#include <functional>

struct Foo {
    int value;
    int get_value() const { return value; }
};

int main() {
    Foo foo{42};

    // std::invoke calls member function
    int v = std::invoke(&Foo::get_value, foo);

    // std::apply can also work, but more verbose
    auto t = std::make_tuple(&Foo::get_value, &foo);
    // std::apply doesn't directly support member function pointer this-binding
}
```

## Compiler Support

| Compiler | Minimum Version | Notes |
|----------|----------------|-------|
| GCC | 7.0 | Full support |
| Clang | 5.0 | Full support |
| MSVC | 19.11 (VS 2017 15.3) | Full support |

`constexpr std::apply` is available in C++17 when both `f` and the tuple elements are `constexpr`.

## Best Practices

- **Simplify tuple expansion**: Anywhere you need `std::get<I>(t)...`, consider using `std::apply`.
- **Pair with emplace**: Use `std::apply` with `emplace_back` to reduce copies when inserting complex objects into containers.
- **Watch reference collapsing**: `std::apply` forwards tuple elements; ensure lambda parameters use perfect forwarding (`auto&&`).
- **Don't overuse**: Direct function calls are clearer in simple cases; `std::apply` suits tuple-driven batch patterns.

## Common Pitfalls

```cpp
// Pitfall 1: tuple element lifetime
auto make_args() {
    return std::make_tuple(1, std::string("hello"));
    // string is a temporary; ensure lifetime before apply call
}

// Pitfall 2: reference parameters need ref wrapper
int x = 10;
auto t = std::make_tuple(std::ref(x));
std::apply([](int& v) { v = 20; }, t);
// x is now 20

// Pitfall 3: empty tuple
std::apply([]() { std::cout << "no args\n"; }, std::make_tuple());
// OK: zero-argument call
```
