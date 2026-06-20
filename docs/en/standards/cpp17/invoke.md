---
title: C++17 std::invoke
topic: unknown
feature: std-invoke
standard: N/A
status_checked_at: 2026-06-20
---
# C++17 std::invoke

## Overview

`std::invoke` is a free function introduced in C++17 within `<functional>` that provides a **unified call syntax**. It can invoke regular functions, lambdas, function objects, member function pointers, and member variable pointers without writing different calling code for each type. It is the foundation for `std::apply`, `std::thread`, `std::invoke_result`, and other facilities.

## Function Signature

```cpp
#include <functional>

template <class F, class... Args>
constexpr std::invoke_result_t<F, Args...> invoke(F&& f, Args&&... args);
```

- `f`: Any callable (function, lambda, member function pointer, member variable pointer, bind expression, etc.)
- `args...`: Call arguments

## Basic Usage

```cpp
#include <functional>
#include <iostream>

int free_function(int a, int b) {
    return a + b;
}

int main() {
    // Regular function
    int r1 = std::invoke(free_function, 3, 4);  // 7

    // Lambda
    auto add = [](int a, int b) { return a + b; };
    int r2 = std::invoke(add, 5, 6);  // 11

    // Function object
    struct Multiplier {
        int operator()(int a, int b) const { return a * b; }
    };
    int r3 = std::invoke(Multiplier{}, 3, 4);  // 12

    std::cout << r1 << " " << r2 << " " << r3 << "\n";
}
```

## Invoking Member Functions

This is the most valuable feature of `std::invoke` — unified syntax for calling member functions:

```cpp
#include <functional>
#include <iostream>
#include <string>

struct Widget {
    int value;
    std::string name;

    int get_value() const { return value; }
    void set_value(int v) { value = v; }
    std::string greet() const { return "Hello, " + name; }
};

int main() {
    Widget w{42, "test"};

    // Member function pointer — second argument is this
    int v = std::invoke(&Widget::get_value, w);  // 42
    std::cout << v << "\n";

    // Member function with arguments
    std::invoke(&Widget::set_value, w, 100);
    std::cout << w.value << "\n";  // 100

    // Call through pointer
    Widget* ptr = &w;
    std::string g = std::invoke(&Widget::greet, ptr);  // "Hello, test"
    std::cout << g << "\n";

    // Call through reference
    Widget& ref = w;
    int v2 = std::invoke(&Widget::get_value, ref);  // 100
}
```

## Accessing Member Variables

```cpp
#include <functional>
#include <iostream>

struct Point {
    double x, y;
};

int main() {
    Point p{3.0, 4.0};

    // Member variable pointer — returns reference
    double x = std::invoke(&Point::x, p);  // 3.0
    std::cout << "x: " << x << "\n";

    // Can modify
    std::invoke(&Point::y, p) = 5.0;
    std::cout << "y: " << p.y << "\n";  // 5.0

    // Access through pointer
    Point* ptr = &p;
    double y = std::invoke(&Point::y, *ptr);  // 5.0
}
```

## Working with std::bind

```cpp
#include <functional>
#include <iostream>

void print_three(int a, int b, int c) {
    std::cout << a << ", " << b << ", " << c << "\n";
}

int main() {
    auto bound = std::bind(print_three, 1, std::placeholders::_1, 3);

    // std::invoke can call bind expressions
    std::invoke(bound, 2);  // 1, 2, 3
}
```

## Working with Lambdas

```cpp
#include <functional>
#include <iostream>
#include <vector>
#include <algorithm>

struct Logger {
    void log(const std::string& msg) const {
        std::cout << "[LOG] " << msg << "\n";
    }
};

int main() {
    Logger logger;

    // Store member function pointer and object reference
    auto log_fn = std::bind(&Logger::log, &logger, std::placeholders::_1);

    // Call via std::invoke
    std::invoke(log_fn, "application started");
    std::invoke(log_fn, "processing data");
}
```

## std::invoke_result and Return Types

```cpp
#include <functional>
#include <type_traits>
#include <iostream>

int func(int a) { return a * 2; }

int main() {
    // Get return type
    using result_t = std::invoke_result_t<decltype(func), int>;
    static_assert(std::is_same_v<result_t, int>);

    // Check if invocable
    static_assert(std::is_invocable_v<decltype(func), int>);
    static_assert(!std::is_invocable_v<decltype(func), std::string>);

    // Get invoke result
    auto r = std::invoke(func, 5);
    std::cout << r << "\n";  // 10
}
```

## Usage in Generic Code

```cpp
#include <functional>
#include <iostream>
#include <vector>

// Generic apply_all: call a callable on every element
template <typename Container, typename Func>
void apply_all(Container& c, Func&& func) {
    for (auto& elem : c) {
        std::invoke(std::forward<Func>(func), elem);
    }
}

int main() {
    std::vector<int> nums = {1, 2, 3, 4, 5};

    // Pass lambda
    apply_all(nums, [](int& x) { x *= 2; });

    // Pass function object
    struct Doubler {
        void operator()(int& x) const { x *= 2; }
    };
    apply_all(nums, Doubler{});

    for (int n : nums) {
        std::cout << n << " ";  // 2 4 6 8 10
    }
    std::cout << "\n";
}
```

## std::apply vs std::invoke

| Feature | `std::apply` | `std::invoke` |
|---------|-------------|---------------|
| Argument source | Expanded from tuple | Passed directly |
| Member function pointer | Needs manual this-binding | Natively supported |
| Member variable access | Not supported | Supported |
| Primary use case | Tuple-driven calls | Unified call syntax |

```cpp
#include <functional>
#include <tuple>

struct Obj {
    int x;
    int get() const { return x; }
};

int main() {
    Obj obj{42};

    // std::invoke directly calls member function
    int v1 = std::invoke(&Obj::get, obj);

    // std::apply can only expand arguments, not handle member function this
    // Need manual binding:
    auto bound = std::bind(&Obj::get, &obj);
    int v2 = std::invoke(bound);
}
```

## Compiler Support

| Compiler | Minimum Version | Notes |
|----------|----------------|-------|
| GCC | 7.0 | Full support |
| Clang | 5.0 | Full support |
| MSVC | 19.11 (VS 2017 15.3) | Full support |

`std::invoke` requires C++17 compilation mode. The header is `<functional>`.

## Best Practices

- **Unified call syntax**: Use `std::invoke` in generic code to handle both free functions and member functions.
- **Pair with `std::invoke_result_t`**: Get the return type of callables for SFINAE and Concepts.
- **Member function pointers are the core use case**: `std::invoke(&Class::method, obj, args...)` is more flexible than `obj.method(args...)`.
- **Don't overuse**: Direct calls are clearer in simple cases; `std::invoke` suits generic and metaprogramming scenarios.

## Common Pitfalls

```cpp
// Pitfall 1: member function pointer needs object argument
struct Foo { int bar() { return 42; } };
Foo f;
// std::invoke(&Foo::bar);  // Compile error! Missing this
std::invoke(&Foo::bar, f);   // OK

// Pitfall 2: const member functions
struct Bar { int value() const { return 1; } };
Bar b;
// Non-const reference can also call const member functions
int v = std::invoke(&Bar::value, b);

// Pitfall 3: member variables returning references
struct S { int x; };
S s{10};
int& ref = std::invoke(&S::x, s);  // OK, ref is a reference to s.x
ref = 20;  // s.x is now 20
```
