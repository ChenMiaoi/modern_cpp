---
title: "C++14 std::make_unique"
topic: unknown
feature: make-unique
standard: N/A
status_checked_at: 2026-06-02
---
# C++14 std::make_unique

## Overview

C++11 introduced `std::unique_ptr` and `std::make_shared`, but omitted `std::make_unique`. C++14 fills this gap. `std::make_unique` constructs an object on the heap and returns a `unique_ptr`, providing an exception-safe single-expression construction. It supports both scalar types and array types.

## Syntax

```cpp
// Scalar version
auto ptr = std::make_unique<T>(args...);

// Array version (new in C++14)
auto arr = std::make_unique<T[]>(n);
```

Header: `<memory>`

## `new` + `unique_ptr` vs `make_unique`

### C++11 Approach (Has Pitfalls)

```cpp
// Problem 1: two evaluations — potential leak
process(std::unique_ptr<Widget>(new Widget()), compute_priority());
// If evaluation order is:
//   1. new Widget()
//   2. compute_priority() throws
//   3. unique_ptr construction never runs → memory leak

// Problem 2: not concise enough
auto p = std::unique_ptr<Widget>(new Widget(42, "hello"));
```

### C++14 Approach

```cpp
// Exception safe: single evaluation
process(std::make_unique<Widget>(), compute_priority());

// Concise
auto p = std::make_unique<Widget>(42, "hello");
```

## Code Examples

### Basic Usage

```cpp
#include <memory>
#include <string>
#include <iostream>

struct User {
    std::string name;
    int age;

    User(std::string n, int a) : name(std::move(n)), age(a) {}
};

int main() {
    // Constructor arguments are perfectly forwarded
    auto user = std::make_unique<User>("Alice", 30);

    std::cout << user->name << ", " << user->age << '\n';
    // Output: Alice, 30
}
```

### Array Version

```cpp
#include <memory>

// C++11 could only use new[], no make_unique for arrays
// C++14 can:
auto arr = std::make_unique<int[]>(10);  // 10 ints, value-initialized to 0

// Access
arr[0] = 42;
arr[9] = 99;

// Note: the array version does not accept constructor arguments
// auto arr2 = std::make_unique<int[]>(5, 1, 2, 3, 4, 5); // Error
```

### Key Exception-Safety Scenario

```cpp
#include <memory>

struct A { A() {} };
struct B { B() {} throw_on_copy{} };

// Interleaved evaluation of a two-argument expression can cause a leak
void unsafe(A*, int) {}
void safe(std::unique_ptr<A>, int) {}

int might_throw();

void demo() {
    // Unsafe — evaluation order of new A and might_throw() is unspecified
    // unsafe(new A(), might_throw());

    // Safe — make_unique is a complete evaluation step
    safe(std::make_unique<A>(), might_throw());
}
```

### Working with the Factory Pattern

```cpp
#include <memory>
#include <string>

struct Shape {
    virtual ~Shape() = default;
    virtual double area() const = 0;
};

struct Circle : Shape {
    double radius;
    explicit Circle(double r) : radius(r) {}
    double area() const override { return 3.14159 * radius * radius; }
};

struct Rect : Shape {
    double w, h;
    Rect(double w, double h) : w(w), h(h) {}
    double area() const override { return w * h; }
};

// Factory function
std::unique_ptr<Shape> make_shape(const std::string& type) {
    if (type == "circle") return std::make_unique<Circle>(5.0);
    if (type == "rect")   return std::make_unique<Rect>(3.0, 4.0);
    return nullptr;
}
```

### Why C++11 Omitted `make_unique`

```cpp
// C++11 standard committee's reasoning:
// - make_shared is a performance necessity (single allocation)
// - make_unique has no performance advantage (still two allocations: no control block needed)
// - But omitting make_unique forced users to write new + unique_ptr, breaking exception safety
//
// Herb Sutter and Stephan T. Lavavej filled this gap in the C++14 proposal

// C++13 workaround (implement your own):
template <typename T, typename... Args>
std::unique_ptr<T> make_unique_workaround(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
```

## Scenarios Where `make_unique` Is Not Suitable

```cpp
// 1. Custom deleter required
auto file = std::unique_ptr<FILE, decltype(&fclose)>(
    fopen("test.txt", "r"), &fclose);
// make_unique does not support custom deleters

// 2. Large raw memory block (placement new style)
auto buf = std::make_unique<char[]>(4096);  // Value-initialized to 0, has overhead
// For uninitialized memory, new is still needed

// 3. Aggregate initialization needed (not supported in C++14)
struct Point { int x; int y; };
// auto p = std::make_unique<Point>({1, 2});  // Not allowed in C++14
// Supported from C++20
```

## Best Practices

1. **Always use `make_unique` instead of `new` + `unique_ptr`**: Except when a custom deleter is needed.
2. **Understand the exception-safety essence**: `make_unique` wraps object construction and pointer creation in a single expression, avoiding leaks caused by evaluation order.
3. **Array version only supports default initialization**: `make_unique<T[]>(n)` value-initializes elements; for uninitialized allocation, `new` is still required.
4. **Do not mix `make_unique` with `shared_ptr`**: `make_unique` creates a `unique_ptr`; convert to `shared_ptr` using `std::shared_ptr<T>(std::move(uptr))`.
5. **Use with `auto` deduction**: `auto p = std::make_unique<T>(...)` is the most concise, avoiding repeated type names.
6. **C++20 improvements**: C++20 supports aggregate initialization with `make_unique`, enabling construction of POD types without custom constructors.
