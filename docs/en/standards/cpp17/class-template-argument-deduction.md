---
title: "C++17 Class Template Argument Deduction (CTAD)"
topic: unknown
feature: class-template-argument-deduction
standard: N/A
status_checked_at: 2026-06-02
---
# C++17 Class Template Argument Deduction (CTAD)

## Overview

C++17 introduced **Class Template Argument Deduction (CTAD)**, allowing the omission of template arguments when constructing class template objects. The compiler automatically deduces them from the constructor arguments. This eliminates the dependency on `make_xxx` factory functions and simplifies code.

## Syntax

```cpp
// Before C++17: explicit template arguments required
std::pair<int, double> p1(1, 3.14);
auto p2 = std::make_pair(1, 3.14);

// C++17: direct deduction
std::pair p3(1, 3.14);          // pair<int, double>
std::vector v{1, 2, 3};         // vector<int>
```

## Deduction Rules

### Implicit Deduction Guides

The compiler automatically generates implicit deduction guides from each constructor:

```cpp
template <typename T>
struct Wrapper {
    T value;
    Wrapper(T v) : value(v) {}
};
// implicitly generated: template <typename T> Wrapper(T) -> Wrapper<T>;

Wrapper w(42);       // T = int
Wrapper w2("hello"); // T = const char*
```

### Explicit Deduction Guides

```cpp
template <typename T>
struct Box { T content; };

Box(const char*) -> Box<std::string>;  // explicit guide

Box b("hello");   // Box<std::string>, not Box<const char*>
Box b2(42);       // Box<int>
```

## Replacing make_pair / make_tuple

```cpp
// C++14
auto p = std::make_pair(1, "hello");

// C++17 CTAD
std::pair p(1, "hello");
std::tuple t(1, 3.14, "x");
```

`make_xxx` can still be used for scenarios requiring perfect forwarding or `decay`.

## std::array CTAD

```cpp
// C++14: verbose
std::array<int, 3> a1 = {1, 2, 3};

// C++17 CTAD
std::array a2 = {1, 2, 3};     // array<int, 3>
std::array a3 = {1.0, 2.0};   // array<double, 2>
```

## User-Defined Deduction Guides

### Deducing from Iterator Pairs

```cpp
template <typename T>
class SimpleVector {
    T* data_;
    std::size_t size_;
public:
    template <typename Iter>
    SimpleVector(Iter first, Iter last) { /* ... */ }
};

template <typename Iter>
SimpleVector(Iter, Iter)
    -> SimpleVector<typename std::iterator_traits<Iter>::value_type>;

std::vector<int> v = {1, 2, 3};
SimpleVector sv(v.begin(), v.end());  // SimpleVector<int>
```

### Multiple Constructor Scenarios

```cpp
template <typename T>
struct Range {
    T begin_, end_, step_;
    Range(T begin, T end) : begin_(begin), end_(end), step_(1) {}
    Range(T begin, T end, T step) : begin_(begin), end_(end), step_(step) {}
};

Range r1(0, 10);           // Range<int>
Range r2(0.0, 10.0, 0.5); // Range<double>
// Range r3(0, 10.0);     // error: T cannot be both int and double
```

## Inheritance and CTAD

```cpp
template <typename T>
struct Base {
    T value;
    Base(T v) : value(v) {}
};

template <typename T>
struct Derived : Base<T> {
    using Base<T>::Base;
};

Derived d(42);  // Derived<int>
```

## Limitations

1. **Aggregate initialization is limited**: C++17 has incomplete CTAD support for aggregate types; C++20 completes it.
2. **Alias templates do not participate in CTAD**:
   ```cpp
   template <typename T> using Vec = std::vector<T>;
   // Vec v = {1, 2, 3};  // error
   ```
3. **Deduction guides must be in the same namespace as the class definition**.

## Best Practices

1. **Prefer CTAD over `make_xxx`**.
2. **Provide explicit deduction guides for custom classes**.
3. **Be aware of implicit conversions**: CTAD deduces exact types and does not perform implicit conversions.

## Common Pitfalls

- **Braces vs. parentheses**: `std::vector v1{3, 100}` deduces as `vector<int>` (two elements), consistent with non-CTAD behavior but more easily confused.
- **Alias templates do not support CTAD**: must use the original template name.
- **Deduction failure is a hard error**: no fallback to other constructors.
- **Implicit deduction guides may match unexpectedly**: guides generated from template constructors may be broader than expected.
