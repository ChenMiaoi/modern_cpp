---
title: "`requires` Expression"
topic: unknown
feature: requires-expression
standard: N/A
status_checked_at: 2026-06-02
---
# `requires` Expression

## Overview

The C++20 `requires` expression is the core primitive of the constraint system, detecting at compile time whether a type satisfies specific syntactic requirements. It returns a `bool` constant and can be used directly as the body of a concept definition or as a constraint condition.

## Basic Syntax

```cpp
template <typename T>
concept Incrementable = requires(T t) {
    { ++t } -> std::same_as<T&>;
};

static_assert(Incrementable<int>);
static_assert(!Incrementable<std::string>);
```

## Four Kinds of Requirements

### 1. Simple Requirements

Verify that an expression is **syntactically valid**, without evaluating it.

```cpp
template <typename T>
concept HasSize = requires(T t) {
    t.size();
    t.begin();
};
```

### 2. Type Requirements

Verify that a type expression is valid.

```cpp
template <typename T>
concept HasValueType = requires {
    typename T::value_type;
    typename T::iterator;
};

static_assert(HasValueType<std::vector<int>>);
static_assert(!HasValueType<int>);
```

### 3. Compound Requirements

Verify that an expression is valid **and** the result satisfies type/`noexcept` constraints.

```cpp
template <typename T>
concept Hashable = requires(T t) {
    { std::hash<T>{}(t) } noexcept -> std::convertible_to<std::size_t>;
};

template <typename T>
concept Printable = requires(std::ostream& os, T t) {
    { os << t } -> std::convertible_to<std::ostream&>;
};
```

### 4. Nested Requirements

Directly impose concept constraints within a `requires` expression.

```cpp
template <typename T>
concept SortableRange = requires(T& t) {
    requires std::ranges::random_access_range<T>;
    requires std::totally_ordered<std::ranges::range_value_t<T>>;
    { std::sort(t.begin(), t.end()) };
};
```

## `requires` Clause vs `requires` Expression

```cpp
// Clause: constraint applied after template parameters
template <typename T>
    requires std::integral<T>
T add(T a, T b) { return a + b; }

// Expression: produces a bool value
constexpr bool v = requires { requires std::integral<int>; };

// Mixed: expression embedded in a clause
template <typename T>
    requires requires(T t) { t.serialize(); }
void process(T& obj) { obj.serialize(); }
```

| | `requires` Clause | `requires` Expression |
|---|---|---|
| Position | After template parameters, function signature | Anywhere a `bool` is needed |
| Result | Does not produce a value, only constrains | Produces a `bool` constant |
| Syntax | `requires Concept<Args>` | `requires { ... }` |

## Constraint Subsumption

```cpp
template <typename T>
concept C1 = requires(T t) { t.foo(); };

template <typename T>
concept C2 = C1<T> && requires(T t) { t.bar(); };
// C2 subsumes C1

void f(auto&& x) requires C1<decltype(x)> { }
void f(auto&& x) requires C2<decltype(x)> { }

struct S { void foo(); void bar(); };
S s;
f(s);  // selects C2 version: more constrained
```

Bare `requires` expressions do **not** participate in subsumption:

```cpp
void g(auto&& x) requires requires { x.foo(); } { }
void g(auto&& x) requires requires { x.foo(); x.bar(); } { }
// Ambiguity! Direct requires expressions have no subsumption relationship
```

## Common Patterns

```cpp
// Check member function signature
template <typename T>
concept Serializable = requires(const T& t, std::ostream& os) {
    { t.serialize(os) } -> std::same_as<void>;
};

// Check constructors
template <typename T>
concept DefaultAndCopy = requires {
    T{};
    T{std::declval<T>()};
};

// Check operators
template <typename T>
concept Addable = requires(T a, T b) {
    { a + b } -> std::convertible_to<T>;
    { a += b } -> std::same_as<T&>;
};
```

## Nested `requires` and Short-Circuit Evaluation

```cpp
template <typename T>
concept ComplexConcept = requires(T t) {
    typename T::value_type;                        // 1. type exists
    { t.size() } -> std::convertible_to<std::size_t>; // 2. return value
    { t.empty() } -> std::convertible_to<bool>;   // 3. return value
    requires std::copyable<T>;                     // 4. concept constraint
};
// Short-circuits top to bottom: if a preceding check fails, subsequent ones are not checked
```

## Summary

- **Simple requirements** check expression validity, **type requirements** check type existence, **compound requirements** additionally check result type and `noexcept`, **nested requirements** directly impose concept constraints.
- `requires` clauses constrain templates; `requires` expressions produce `bool`.
- Constraint subsumption only takes effect between concepts; bare `requires` expressions do not participate.
- Prefer wrapping `requires` expressions in concepts to support subsumption-based overload resolution.
