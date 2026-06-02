---
title: C++20 Concepts
topic: cpp20
feature: concepts
standard: C++20
status_checked_at: 2026-06-02
standard_refs:
  - draft: N4861
    clause: "[concepts]"
proposals: []
exercises: []
solutions: []
---
# C++20 Concepts

## Overview

C++20 introduces Concepts, one of the most significant improvements to the template system. Concepts allow developers to define named constraints on template parameters, elevating type requirements from comments to compiler-checked first-class citizens. Before this, template error messages were cryptic, and constraints could only be expressed indirectly through SFINAE tricks. Concepts solve these problems: error messages directly point out which constraint is not satisfied, code intent is clear at a glance, and `enable_if` and other metaprogramming boilerplate is no longer needed.

The C++20 standard library provides many predefined concepts in the `<concepts>` and `<ranges>` headers.

## Defining a Concept

The syntax is `template <parameter-list> concept name = constraint-expression;`, where the constraint expression must evaluate to a `bool` constant expression.

```cpp
#include <concepts>
#include <type_traits>

// Based on type traits
template <typename T>
concept Hashable = requires(T a) {
    { std::hash<T>{}(a) } -> std::convertible_to<std::size_t>;
};

// Composing existing concepts
template <typename T>
concept SignedInteger = std::integral<T> && std::is_signed_v<T>;

// Multi-parameter concept
template <typename From, typename To>
concept ImplicitlyConvertibleTo = requires(From(&f)()) {
    { f() } -> std::convertible_to<To>;
};
```

## requires Expressions vs requires Clauses

`requires` has two roles: a **requires expression** is an expression that produces a `bool`, checking whether an operation is valid; a **requires clause** is placed after the template parameter list to constrain the template.

```cpp
#include <concepts>
#include <iostream>

// requires expression — checks if the type is outputtable to ostream
template <typename T>
concept Printable = requires(std::ostream& os, T val) {
    { os << val } -> std::same_as<std::ostream&>;
};

// requires clause — constrains the function template
template <typename T>
    requires Printable<T>
void log(const T& value) {
    std::cout << value << '\n';
}

// Simple requires, type requires, and compound requires can be combined
template <typename T>
concept Sortable = requires(T& container) {
    typename T::value_type;                              // type requires
    { container.begin() } -> std::input_or_output_iterator;
    { container.end() }   -> std::input_or_output_iterator;
};
```

## Predefined Concepts

```cpp
#include <concepts>

// same_as: T and U are the same type (including cv/ref qualifiers)
static_assert(std::same_as<int, int>);
static_assert(!std::same_as<int, const int>);

// convertible_to: From can be implicitly converted to To, and static_cast<To> is valid
static_assert(std::convertible_to<int, double>);
static_assert(!std::convertible_to<int, std::string>);

// integral: T is an integral type (bool, char, int, long, etc.)
static_assert(std::integral<int>);
static_assert(std::integral<char>);
static_assert(!std::integral<double>);

// floating_point: T is a floating-point type
static_assert(std::floating_point<float>);
static_assert(std::floating_point<double>);
static_assert(!std::floating_point<int>);
```

```cpp
// derived_from<D, B>: D publicly derives from B (including self)
struct Base {};
struct Derived : Base {};
static_assert(std::derived_from<Derived, Base>);
static_assert(!std::derived_from<Base, Derived>);

// invocable<F, Args...>: F can be called with Args...
auto square = [](int x) { return x * x; };
static_assert(std::invocable<decltype(square), int>);
static_assert(!std::invocable<decltype(square), std::string>);
```

## Constraining Templates with Concepts

Three ways to apply a concept to template parameters:

```cpp
#include <concepts>
#include <vector>
#include <algorithm>

// Method 1: constrained template parameter (concept prefix)
template <std::integral T>
T gcd(T a, T b) {
    while (b != 0) { T tmp = b; b = a % b; a = tmp; }
    return a;
}

// Method 2: requires clause (for complex constraints)
template <typename T>
    requires std::floating_point<T>
T average(T a, T b) {
    return (a + b) / T{2};
}

// Method 3: combining both
template <std::ranges::range R>
    requires std::sortable<std::ranges::iterator_t<R>>
void my_sort(R& range) {
    std::ranges::sort(range);
}
```

## Abbreviated Function Templates and Constrained auto

C++20 allows using `Concept auto` in place of full template declarations. Each `auto` generates an independent template parameter.

```cpp
#include <ranges>
#include <vector>
#include <iostream>

// Abbreviated function template: std::ranges::range auto&
void print_range(std::ranges::range auto&& container) {
    for (const auto& elem : container)
        std::cout << elem << ' ';
    std::cout << '\n';
}

int main() {
    // constrained auto in variable declarations
    std::integral auto x = 42;
    std::floating_point auto pi = 3.14159;

    std::vector<int> v{5, 2, 8, 1, 9};
    print_range(v);   // OK

    // Error example (compile-time failure):
    // std::integral auto bad = 3.14;  // double does not satisfy integral
}
```

## Concept vs SFINAE vs static_assert

| Dimension | Concept | SFINAE (`enable_if`) | `static_assert` |
|-----------|---------|----------------------|-----------------|
| Error messages | Clearly identifies the unsatisfied constraint | Verbose and cryptic | Direct but does not participate in overload resolution |
| Overload resolution | Participates, selects most specialized match | Participates, syntactically complex | Does not participate — hard error |
| Compilation speed | Faster (constraint caching) | Slower (repeated instantiation) | Fast |
| Readability | High | Low | Medium |
| Composability | Natural composition with `&&`, `||` | Requires `conjunction`/`disjunction` | Not applicable |

```cpp
// C++17 SFINAE
template <typename T,
          typename = std::enable_if_t<std::is_arithmetic_v<T>>>
T clamp_val(T v, T lo, T hi) {
    return (v < lo) ? lo : (hi < v) ? hi : v;
}

// C++20 Concepts — equivalent but clearer
template <std::integral T>
T clamp_val2(T v, T lo, T hi) {
    return (v < lo) ? lo : (hi < v) ? hi : v;
}
```

## Best Practices

1. **Prefer standard library concepts**. `std::integral`, `std::ranges::range`, etc. are well-tested with clear semantics.
2. **Replace SFINAE with concepts**. New code should not use `std::enable_if`; gradually migrate existing SFINAE code.
3. **Name concepts as adjective phrases**: `Sortable`, `Hashable`, `Printable`, reflecting the semantics the type should satisfy.
4. **Keep concept granularity fine**. One concept should express one constraint; combine them with `&&` for better reusability.
5. **Constrain at the declaration, not the definition**. If the header already has `requires`, the source file should not repeat it.

## Common Pitfalls

1. **Confusing `requires` expressions with `requires` clauses**. `requires expr { ... }` produces a `bool`; `template <...> requires C` constrains a template. They have different syntax and different purposes.
2. **Each `auto` in constrained auto is an independent template parameter**. In `f(std::integral auto a, std::integral auto b)`, `a` and `b` can have different types. For the same type, you must explicitly write `template <std::integral T> void f(T a, T b);`.
3. **Concepts are not types**. You cannot write `std::integral x = 42;` — a concept is a compile-time predicate used only for constraining.
4. **Subsumption rules are easy to get wrong**. The compiler judges constraint containment at the syntactic level; two semantically identical but syntactically different concepts will not substitute for each other.
5. **Do not use `static_assert` instead of concepts for overload selection**. `static_assert` failure does not fall back to other overloads — use concepts or requires clauses for SFINAE semantics.
