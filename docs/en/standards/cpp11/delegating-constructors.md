---
title: "Delegating Constructors"
topic: unknown
feature: delegating-constructors
standard: N/A
status_checked_at: 2026-06-02
---
# Delegating Constructors

## Overview

C++11 introduced delegating constructors, allowing one constructor to call another constructor of the same class in its initializer list. This feature eliminates the long-standing problem of constructor code duplication in C++ — before C++11, multiple constructors often had to replicate the same initialization logic, typically mitigated by extracting a private `init()` member function, but this approach could not complete initialization during the initializer list phase, causing member variables to undergo the inefficient "default-construct then assign" path.

Delegating constructors enable constructors to form clear delegation chains, where each constructor writes initialization logic only once, and other constructors reuse it through delegation.

## Syntax

```cpp
class Foo {
public:
    // Target constructor (the one being delegated to)
    Foo(int a, int b) : a_(a), b_(b) {}

    // Delegating constructor: calls the target constructor via initializer list
    Foo() : Foo(0, 0) {}

    // Delegates to another delegating constructor, forming a chain
    Foo(int a) : Foo(a, 0) {}

private:
    int a_;
    int b_;
};
```

Key syntax points:

- Delegation appears in the **member initializer list** position but cannot be mixed with member initialization
- The delegation target must be a constructor of the same class
- A delegating constructor's initializer list can **only** contain the delegation call; it cannot also initialize member variables

## Delegation Chains

Multiple constructors can form a delegation chain, ultimately converging to a "target constructor":

```cpp
class Connection {
public:
    // Target constructor — convergence point for all initialization logic
    Connection(const std::string& host, int port, int timeout)
        : host_(host), port_(port), timeout_(timeout), state_(State::Disconnected)
    {
        validate_parameters();
    }

    // Chain delegation: delegates to the target constructor above
    Connection(const std::string& host, int port)
        : Connection(host, port, 30)  // default timeout
    {}

    // Another layer of delegation
    Connection(const std::string& host)
        : Connection(host, 80)        // delegates to the two-param version
    {}

    // Final default construction
    Connection()
        : Connection("localhost")     // delegates to the one-param version
    {}

private:
    std::string host_;
    int port_;
    int timeout_;
    enum class State { Connected, Disconnected } state_;

    void validate_parameters() {
        if (port_ < 0 || port_ > 65535)
            throw std::invalid_argument("Invalid port");
    }
};
```

Delegation chain direction: `Connection()` → `Connection(string)` → `Connection(string, int)` → `Connection(string, int, int)`.

Note: The C++ standard **does not prohibit circular delegation**, but circular delegation causes undefined behavior at runtime (typically infinite recursion), and the compiler will not diagnose this issue.

## Relationship with Member Initialization

A delegating constructor's initializer list **cannot contain both member initialization and delegation calls**. This is a compiler-enforced rule:

```cpp
class Widget {
public:
    int x_;
    int y_;
    double scale_;

    // ✅ Legal: delegating constructor can only delegate, not initialize members
    Widget() : Widget(0, 0) {}

    // ❌ Compilation error: cannot mix delegation with member initialization
    // Widget(int x) : Widget(x, 0), scale_(1.0) {}

    // ✅ Legal: target constructor can initialize all members
    Widget(int x, int y) : x_(x), y_(y), scale_(1.0) {}
};
```

This means the target constructor must be responsible for initializing **all** members. If some members are not explicitly initialized in the target constructor, they will be default-initialized (which for POD types may mean undefined values).

## Interaction with Base Class Initialization Lists

A delegating constructor cannot simultaneously delegate and initialize base classes. Base class initialization must also be done in the target constructor:

```cpp
class Base {
public:
    explicit Base(int id) : id_(id) {}
    int id_;
};

class Derived : public Base {
public:
    // Target constructor: initializes both base class and own members
    Derived(int id, const std::string& name)
        : Base(id), name_(name)
    {}

    // Delegating constructor — base class initialization handled by target constructor
    Derived(int id)
        : Derived(id, "unnamed")
    {}

    // Default construction — full delegation chain
    Derived()
        : Derived(42)
    {}

private:
    std::string name_;
};
```

## Code Duplication Elimination: Real-World Example

### Without Delegating Constructors (C++03 Style)

```cpp
class Matrix {
public:
    Matrix(int rows, int cols)
        : rows_(rows), cols_(cols), data_(new double[rows * cols]())
    {
        if (rows <= 0 || cols <= 0)
            throw std::invalid_argument("Invalid dimensions");
    }

    Matrix(int n)  // square matrix
        : rows_(n), cols_(n), data_(new double[n * n]())
    {
        if (n <= 0)
            throw std::invalid_argument("Invalid dimensions");
    }

    Matrix()  // default 4x4
        : rows_(4), cols_(4), data_(new double[16]())
    {}

    // ❌ Each constructor repeats new[] and validation logic
private:
    int rows_, cols_;
    std::unique_ptr<double[]> data_;
};
```

### With Delegating Constructors (C++11)

```cpp
class Matrix {
public:
    // Target constructor: all logic concentrated here
    Matrix(int rows, int cols)
        : rows_(rows), cols_(cols), data_(new double[rows * cols]())
    {
        if (rows <= 0 || cols <= 0)
            throw std::invalid_argument("Invalid dimensions");
    }

    // Delegation: square matrix
    explicit Matrix(int n) : Matrix(n, n) {}

    // Delegation: default 4×4
    Matrix() : Matrix(4) {}

private:
    int rows_, cols_;
    std::unique_ptr<double[]> data_;
};
```

Validation logic and resource allocation exist in only one place — maintenance cost is significantly reduced.

## Working with `constexpr` Constructors

Delegating constructors can be `constexpr`:

```cpp
struct Point {
    int x, y, z;

    constexpr Point(int x, int y, int z) : x(x), y(y), z(z) {}

    // constexpr delegating constructor
    constexpr Point() : Point(0, 0, 0) {}

    constexpr Point(int v) : Point(v, v, v) {}
};

// Compile-time computation available
constexpr Point origin;
constexpr Point unit(1);
constexpr Point custom(1, 2, 3);
static_assert(custom.z == 3, "z must be 3");
```

## Best Practices

1. **Design a "complete" target constructor**: Concentrate all member initialization and invariant checks in one constructor; have all other constructors delegate to it.

2. **Prefer delegation over `init()` functions**: The `init()` pattern has members already default-constructed by the time the constructor body executes, which is less efficient and cannot be used for `const` or reference member initialization.

3. **Keep delegation chains short and shallow**: Deep delegation chains (more than 3 levels) reduce readability; consider merging intermediate layers.

4. **Ensure the target constructor initializes all members**: Omitted initialization leads to uninitialized members on delegation paths.

5. **Be consistent with `explicit` semantics**: If the target constructor is `explicit`, delegating constructors do not affect this semantics — delegation is an intra-class behavior, not constrained by `explicit`.

## Common Pitfalls

### Pitfall 1: Mixing Delegation with Member Initialization

```cpp
class Bad {
    int a_, b_;
public:
    // ❌ Compilation error
    // Bad(int a) : Bad(a, 0), a_(a) {}
    //
    // ✅ Correct
    Bad(int a) : Bad(a, 0) {}

    Bad(int a, int b) : a_(a), b_(b) {}
};
```

### Pitfall 2: Circular Delegation

```cpp
class Infinite {
public:
    Infinite() : Infinite(0) {}    // delegates to Infinite(int)
    Infinite(int) : Infinite() {}  // delegates to Infinite()
    // ⚠️ Compiles, but causes stack overflow at runtime
};
```

### Pitfall 3: Exception Safety Issues

```cpp
class Resource {
    int* data_;
public:
    Resource(int n) : data_(new int[n]) {
        throw std::runtime_error("oops");  // data_ leaks!
    }

    // Delegating constructor cannot catch exceptions from target constructor to clean up
    Resource() : Resource(10) {}
};
```

When the target constructor throws an exception, the delegating constructor's `catch` block **will not execute** (because the delegating constructor's body has not yet begun). The target constructor must ensure exception safety on its own.

### Pitfall 4: Code in Delegating Constructor Executes **After** Target Constructor

```cpp
class Order {
    int priority_;
public:
    Order(int p) : priority_(p) {
        std::cout << "Target ctor body\n";
    }

    Order() : Order(42) {
        // This body runs AFTER Order(int) completes
        std::cout << "Delegating ctor body\n";
    }
};
// Output:
//   Target ctor body
//   Delegating ctor body
```

Understanding the execution order is critical for correctly placing side effects like logging and registration.
