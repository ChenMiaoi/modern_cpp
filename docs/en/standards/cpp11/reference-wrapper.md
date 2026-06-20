---
title: "std::reference_wrapper"
topic: unknown
feature: reference-wrapper
standard: N/A
status_checked_at: 2026-06-20
---
# std::reference_wrapper

## Overview

`std::reference_wrapper` is a wrapper introduced in C++11 that wraps an lvalue reference into a **copyable, assignable** object. Plain references (`T&`) cannot be copied, assigned, or stored in standard containers. `std::reference_wrapper` solves this by holding an internal pointer to the target object, supporting copy and assignment semantics, while providing implicit conversion back to `T&`.

Header: `<functional>`

```cpp
#include <functional>
```

## Basic Usage

### Creating reference_wrapper

```cpp
int x = 42;
std::reference_wrapper<int> r1 = std::ref(x);   // recommended approach
std::reference_wrapper<int> r2(x);               // direct construction

r1 = 99;          // modify the original value through the wrapper
std::cout << x;   // outputs 99
```

### std::ref() and std::cref()

```cpp
int val = 10;
std::reference_wrapper<int>       rw  = std::ref(val);    // mutable reference
std::reference_wrapper<const int> crw = std::cref(val);   // const reference

val = 20;
std::cout << rw.get() << '\n';   // 20
// crw.get() = 30;               // compile error: cannot modify through const reference
```

`std::ref` and `std::cref` are convenience factory functions that return `std::reference_wrapper<T>` and `std::reference_wrapper<const T>` respectively.

## Implicit Conversion

`std::reference_wrapper` provides implicit conversion to `T&`, so most functions and algorithms that accept references can use it directly:

```cpp
void increment(int& v) { ++v; }

int n = 5;
std::reference_wrapper<int> rw = std::ref(n);
increment(rw);           // implicitly converts to int&
std::cout << n;          // 6
```

## Storing References in Containers

Standard containers require elements to be copyable or movable. References themselves don't satisfy this requirement, but `std::reference_wrapper` does:

```cpp
#include <vector>
#include <functional>
#include <iostream>

int main() {
    int a = 1, b = 2, c = 3;
    std::vector<std::reference_wrapper<int>> refs = {std::ref(a), std::ref(b), std::ref(c)};

    for (auto& r : refs) {
        r.get() *= 10;   // modify original objects through the wrapper
    }

    std::cout << a << ' ' << b << ' ' << c << '\n';  // 10 20 30
}
```

### Storing References to Different Types

When using `std::reference_wrapper`, all elements in a container have the same type (`reference_wrapper<int>`). For heterogeneous references, use base class pointers or other mechanisms:

```cpp
struct Base { int id = 0; };
struct Derived : Base { int extra = 42; };

Derived d;
std::reference_wrapper<Base> rw = std::ref(static_cast<Base&>(d));
```

## Usage with std::bind

`std::bind` internally uses `std::reference_wrapper` to preserve references to arguments. By default, `std::bind` copies its arguments; `std::ref` prevents this copy:

```cpp
#include <functional>
#include <iostream>

void add_one(int& v) { ++v; }

int main() {
    int value = 10;
    auto bound = std::bind(add_one, std::ref(value));
    bound();              // value is modified
    std::cout << value;   // 11
}
```

```cpp
// Without std::ref, bind copies the value
int value = 10;
auto bound = std::bind(add_one, value);  // copies value
bound();              // modifies the copy, original value unchanged
std::cout << value;   // 10 (unchanged)
```

## Cannot Bind to Rvalues

`std::reference_wrapper` can only bind to lvalues, not rvalues (temporary objects):

```cpp
int x = 42;
std::reference_wrapper<int> r1 = std::ref(x);      // OK
// std::reference_wrapper<int> r2 = std::ref(42);   // compile error: cannot bind rvalue

// Also cannot bind to temporary objects
std::string temp() { return "hello"; }
// auto r3 = std::ref(temp());  // compile error: cannot bind to return value of temporary
```

Reason: Allowing rvalue binding would result in a dangling reference, since temporaries are destroyed at the end of the expression.

## reference_wrapper Members

| Member Function | Description |
|-----------------|-------------|
| `get()` | Returns `T&`, accesses the wrapped reference |
| `operator T&()` | Implicit conversion to `T&` |
| `operator=` | Assigns to the referenced object through the wrapper |

```cpp
int x = 5, y = 10;
std::reference_wrapper<int> r1 = std::ref(x);
std::reference_wrapper<int> r2 = std::ref(y);

r1 = r2;           // equivalent to x = y, x becomes 10
r1.get() = 100;    // x = 100
```

## Code Examples

### Generic Mutable View Container

```cpp
#include <vector>
#include <functional>
#include <algorithm>
#include <iostream>

template <typename T>
class MutableView {
    std::vector<std::reference_wrapper<T>> refs_;
public:
    void add(T& obj) { refs_.push_back(std::ref(obj)); }

    void scale_all(double factor) {
        for (auto& r : refs_) {
            r.get() = static_cast<T>(r.get() * factor);
        }
    }

    void print() const {
        for (const auto& r : refs_) {
            std::cout << r.get() << ' ';
        }
        std::cout << '\n';
    }
};

int main() {
    double a = 2.0, b = 3.0, c = 4.0;
    MutableView<double> view;
    view.add(a);
    view.add(b);
    view.add(c);

    view.scale_all(10.0);
    view.print();  // 20 30 40

    std::cout << a << ' ' << b << ' ' << c << '\n';  // 20 30 40
}
```

### reference_wrapper as Callback Parameter

```cpp
#include <functional>
#include <vector>
#include <iostream>

void process(std::function<void(int&)> callback, int& val) {
    callback(val);
}

int main() {
    int counter = 0;

    auto increment = [](int& v) { ++v; };
    auto double_val = [](int& v) { v *= 2; };

    process(increment, counter);
    process(double_val, counter);

    std::cout << counter << '\n';  // 2
}
```

## Notes and Pitfalls

**Do not hold reference_wrapper to temporary objects** — while the compiler blocks `std::ref(42)`, bypassing the type system via `reinterpret_cast` or other means may produce dangling references.

**reference_wrapper is not nullptr-safe** — it does not check whether the internal pointer is null:

```cpp
std::reference_wrapper<int> r = std::ref(*static_cast<int*>(nullptr));
r.get();  // undefined behavior
```

**const correctness** — `std::ref` deduces to `reference_wrapper<T>`, while `std::cref` deduces to `reference_wrapper<const T>`. Choose carefully:

```cpp
int val = 0;
const auto& cr = std::cref(val);  // const reference wrapper
// cr.get() = 5;                  // compile error

auto r = std::ref(val);           // mutable reference wrapper
r.get() = 5;                      // OK
```

## Compiler Support

| Compiler | Supported Since | Notes |
|----------|-----------------|-------|
| GCC | 4.5+ | Full support |
| Clang | 3.1+ | Full support |
| MSVC | 2012 (17.0)+ | Full support |

`std::reference_wrapper` is widely supported in C++11 and is the only mechanism in the standard library for placing references into containers. Its integration with `std::bind` makes it an important tool for functional programming styles in C++.
