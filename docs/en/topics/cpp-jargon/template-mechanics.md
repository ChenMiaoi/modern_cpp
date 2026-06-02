---
title: "Template Mechanics Terminology"
topic: unknown
feature: template-mechanics
standard: N/A
status_checked_at: 2026-06-02
---
# Template Mechanics Terminology

## SFINAE (Substitution Failure Is Not An Error)

When template argument substitution fails, the compiler does not emit an error — it removes the overload from the candidate set:

```cpp
// enable_if: when the condition is false, type does not exist → substitution failure → this overload is ignored
template<typename T>
typename std::enable_if<std::is_integral<T>::value, T>::type
safe_divide(T a, T b) { return b != 0 ? a / b : 0; }

template<typename T>
typename std::enable_if<std::is_floating_point<T>::value, T>::type
safe_divide(T a, T b) { return b != 0.0 ? a / b : 0.0; }
```

SFINAE was the core template constraint mechanism before C++20 — ugly but effective.

## CRTP (Curiously Recurring Template Pattern)

The base class template parameter is the derived class itself — achieving compile-time polymorphism:

```cpp
template<typename Derived>
class Base {
public:
  void interface() {
    static_cast<Derived*>(this)->implementation();  // compile-time dispatch
  }
};

class MyClass : public Base<MyClass> {
public:
  void implementation() { /* concrete implementation */ }
};
```

CRTP's advantage: zero runtime overhead (no vtable); the compiler can inline `implementation()`.

## CTAD (Class Template Argument Deduction, C++17)

The compiler deduces class template arguments from constructors:

```cpp
// C++11/14: must write template arguments
std::pair<int, double> p1(42, 3.14);
auto p2 = std::make_pair(42, 3.14);

// C++17: direct deduction
std::pair p3(42, 3.14);  // CTAD: pair<int, double>
std::vector v{1, 2, 3};  // CTAD: vector<int>
```

### Deduction Guide

When automatic deduction is not sufficient, you can explicitly define deduction rules:

```cpp
template<typename T>
struct MyContainer {
  MyContainer(std::initializer_list<T>);
};

// deduction guide: deduce T from initializer_list
template<typename T>
MyContainer(std::initializer_list<T>) -> MyContainer<T>;
```

## Concept (C++20)

Named constraints on template parameters, replacing SFINAE black magic:

```cpp
template<typename T>
concept Hashable = requires(T a) {
  { std::hash<T>{}(a) } -> std::convertible_to<std::size_t>;
};

template<Hashable T>
void process(T value) { ... }  // clear, readable, friendly error messages
```

### Subsumption

The subsumption relationship between C++20 Concepts — more constrained concepts take priority:

```cpp
template<typename T> concept C = requires { typename T::type; };
template<typename T> concept D = C<T> && requires { typename T::value_type; };

void f(C auto);  // general version
void f(D auto);  // more specialized version — preferred when D is satisfied
```

## Variadic Template & Parameter Pack

```cpp
template<typename... Args>
void print(Args&&... args) {
  (std::cout << ... << args) << "\n";  // fold expression (C++17)
}
```

### Fold Expression (C++17)

```cpp
template<typename... Args>
auto sum(Args... args) {
  return (args + ...);  // unary right fold
  // equivalent to: arg1 + (arg2 + (arg3 + ...))
}
```

## Expression Template

A deferred computation template technique that avoids intermediate temporary objects:

```cpp
// a = b * c + d * e
// naive implementation: 3 temporary objects
// expression template: 0 temporary objects — the entire expression tree is computed at once during assignment

// b * c returns mul_expr<B, C> — no multiplication performed
// +   returns add_expr<mul_expr1, mul_expr2> — no addition performed
// =   unfolds the entire expression and computes directly into a
```

See the expression template implementation in Boost.Multiprecision (refer to Boost source code in `references/impl/`).

## Template Template Parameter

A template parameter that accepts another template as its argument:

```cpp
template<typename T, template<typename> class Container>
struct Holds {
  Container<T> data;
};

Holds<int, std::vector> h;  // Container = std::vector
Holds<int, std::list> h2;   // Container = std::list
```
