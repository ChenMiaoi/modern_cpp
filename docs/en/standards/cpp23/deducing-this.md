---
title: Deducing this
topic: cpp23
feature: deducing-this
standard: C++23
status_checked_at: 2026-06-01
exercises:
  - exercises/cpp23/deducingthis1.cpp
solutions:
  - exercises/solutions/deducingthis1.cpp
---
# Deducing this

C++23 introduces the explicit object parameter, allowing type deduction of the `this` parameter to simplify CRTP, eliminate redundant reference-qualified overloads, and support recursive lambdas.

## Basic Syntax

```cpp
struct Widget {
    // Traditional: const/non-const requires two overloads
    void greet() const { std::cout << "const\n"; }
    void greet()       { std::cout << "non-const\n"; }

    // C++23: merged into a single template function
    template <typename Self>
    void greet(this Self&& self) {
        if constexpr (std::is_const_v<std::remove_reference_t<Self>>)
            std::cout << "const\n";
        else
            std::cout << "non-const\n";
    }
};
```

`this Self&& self` is the explicit object parameter; the compiler passes the object as the first explicit argument.

## Eliminating Redundant Reference-Qualified Overloads

```cpp
// Traditional — 4 overloads, repeated logic
struct Container {
    std::string& name() &              { return name_; }
    const std::string& name() const&   { return name_; }
    std::string name() &&             { return std::move(name_); }
};

// C++23 — single template function
struct Container {
    template <typename Self>
    auto&& name(this Self&& self) {
        return std::forward<Self>(self).name_;
    }
};
```

## CRTP Alternative

```cpp
// Traditional CRTP — verbose
template <typename Derived>
struct Base {
    void interface() { static_cast<Derived*>(this)->implementation(); }
};
struct MyClass : Base<MyClass> {
    void implementation() { /* ... */ }
};

// C++23 — concise
struct Base {
    template <typename Self>
    void interface(this Self&& self) { self.implementation(); }
};
struct MyClass : Base {
    void implementation() { /* ... */ }
};
```

### Practical Application: Polymorphic Copy

```cpp
struct Shape {
    virtual ~Shape() = default;
    template <typename Self>
    auto clone(this Self&& self) {
        return std::make_unique<std::decay_t<Self>>(std::forward<Self>(self));
    }
};

struct Circle : Shape {
    double radius;
    Circle(double r) : radius(r) {}
};

auto c = std::make_unique<Circle>(5.0);
auto c2 = c->clone();  // unique_ptr<Circle>
```

## Recursive Lambdas

Before C++23, lambdas could not directly call themselves recursively. The explicit object parameter solves this:

```cpp
// C++23 recursive lambda — zero overhead
auto factorial = [](this auto self, int n) -> int {
    return n <= 1 ? 1 : n * self(n - 1);
};
std::cout << factorial(5) << "\n";  // 120

// Recursive tree traversal
auto traverse = [](this auto self, const Tree* node) -> void {
    if (!node) return;
    self(node->left);
    std::cout << node->value << " ";
    self(node->right);
};
```

The traditional approach requires `std::function` (with indirect call overhead) or a Y-combinator (complex). The C++23 approach lets the compiler inline the recursive calls directly.

## Value Category Deduction

```cpp
struct StringWrapper {
    std::string data;
    template <typename Self>
    auto&& get(this Self&& self) {
        return std::forward<Self>(self).data;
    }
};

StringWrapper w{"hello"};
auto& s = w.get();                  // string&
auto&& s2 = std::move(w).get();    // string&&
```

## Cache-Friendly Memoization

```cpp
auto fib_memo = [](this auto self, int n,
                   std::unordered_map<int, int>& cache) -> int {
    if (n <= 1) return n;
    if (auto it = cache.find(n); it != cache.end()) return it->second;
    int result = self(n - 1, cache) + self(n - 2, cache);
    return cache[n] = result;
};
```

## Caveats

- The explicit object parameter cannot coexist with a traditional implicit `this` in the same member function
- Cannot be used with constructors or destructors (`operator()` in lambdas is fine)
- Virtual functions can use it, but `Self` deduction is based on the static type
- The `this` parameter must be the function's first parameter
