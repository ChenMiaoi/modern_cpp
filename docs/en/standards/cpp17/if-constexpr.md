---
title: "C++17 if constexpr"
topic: unknown
feature: if-constexpr
standard: N/A
status_checked_at: 2026-06-02
---
# C++17 if constexpr

## Overview

`if constexpr` is a compile-time conditional branching statement introduced in C++17 that allows selective compilation of code branches in template functions based on compile-time constant expressions. Discarded branches do not participate in template instantiation and do not require the branch to be valid for the current template argument type. This feature replaces complex patterns such as SFINAE, tag dispatch, and template specialization.

## Basic Syntax

```cpp
if constexpr (compile-time constant expression) {
    // compiled when condition is true
} else {
    // compiled when condition is false (optional)
}
```

## Basic Usage

```cpp
#include <iostream>
#include <type_traits>

template<typename T>
void describe(T value) {
    if constexpr (std::is_integral_v<T>) {
        std::cout << "integral: " << value << "\n";
    } else if constexpr (std::is_floating_point_v<T>) {
        std::cout << "float: " << value << "\n";
    } else {
        std::cout << "other type\n";
    }
}

describe(42);       // integral: 42
describe(3.14);     // float: 3.14
describe("hello");  // other type
```

## Difference from Regular if

```cpp
template<typename T> void foo(T x) {
    if (std::is_integral_v<T>) {
        // regular if: both branches must be valid at compile time
    } else {
        // x.nonexistent_method();  // compile error—even if not taken at runtime
    }
}

template<typename T> void bar(T x) {
    if constexpr (std::is_integral_v<T>) {
        // ...
    } else {
        // x.nonexistent_method();  // discarded branch—not instantiated, no error
    }
}
```

## Discarded Branch Rules

1. **Not instantiated** — not required to be valid for the current type.
2. **Must be parseable** — syntactic structure must be valid.
3. **Non-dependent names are still looked up** — names not dependent on template parameters are still bound.
4. **Return type deduction** — each branch may return a different type.

```cpp
template<typename T>
auto convert(T val) {
    if constexpr (std::is_same_v<T, std::string>)
        return val.length();         // size_t
    else if constexpr (std::is_arithmetic_v<T>)
        return std::to_string(val);  // std::string
}
```

## Replacing SFINAE

```cpp
// C++14: two overloads + enable_if
template<typename T>
std::enable_if_t<std::is_integral_v<T>, T>
safe_add(T a, T b) {
    if (b > 0 && a > std::numeric_limits<T>::max() - b)
        throw std::overflow_error("overflow");
    return a + b;
}
template<typename T>
std::enable_if_t<!std::is_integral_v<T>, T>
safe_add(T a, T b) { return a + b; }

// C++17: single function body
template<typename T>
T safe_add(T a, T b) {
    if constexpr (std::is_integral_v<T>) {
        if (b > 0 && a > std::numeric_limits<T>::max() - b)
            throw std::overflow_error("overflow");
    }
    return a + b;
}
```

## Replacing Tag Dispatch

```cpp
// C++14: multiple overloads + tag types
template<typename Iter>
void advance_impl(Iter& it, int n, std::random_access_iterator_tag) { it += n; }
template<typename Iter>
void advance_impl(Iter& it, int n, std::input_iterator_tag) {
    for (int i = 0; i < n; ++i) ++it;
}
template<typename Iter>
void my_advance(Iter& it, int n) {
    advance_impl(it, n, typename std::iterator_traits<Iter>::iterator_category{});
}

// C++17: single function
template<typename Iter>
void my_advance(Iter& it, int n) {
    using Cat = typename std::iterator_traits<Iter>::iterator_category;
    if constexpr (std::is_base_of_v<std::random_access_iterator_tag, Cat>)
        it += n;
    else
        for (int i = 0; i < n; ++i) ++it;
}
```

## Replacing Recursive Unrolling

```cpp
#include <iostream>
#include <tuple>

template<typename Tuple, std::size_t N = 0>
void print_tuple(const Tuple& t) {
    if constexpr (N < std::tuple_size_v<Tuple>) {
        if constexpr (N > 0) std::cout << ", ";
        std::cout << std::get<N>(t);
        print_tuple<Tuple, N + 1>(t);
    }
    // when N reaches the size, condition is false, recursion ends naturally
    // no base case specialization needed
}
```

## Relationship with Template Specialization

Suitable for `if constexpr`: type branching within the same function body, replacing SFINAE, replacing tag dispatch.

Still requires template specialization: completely different class definitions/member variables, variable template specialization.

## Best Practices

- **Replace `enable_if`** — an order of magnitude more readable.
- **Replace tag dispatch** — no extra overloads and tag types needed.
- **Recursive template unrolling** — replaces base case specialization to terminate recursion.
- **Use only for compile-time conditions** — use regular `if` for runtime conditions.

## Common Pitfalls

```cpp
// Pitfall 1: condition is not a compile-time constant
int x = 42;
// if constexpr (x > 0) {}  // compile error

// Pitfall 2: syntax errors in discarded branches are still detected
// x ==== 42;  // syntax error—detected even in a discarded branch

// Pitfall 3: non-dependent names in discarded branches are still looked up
template<typename T> void tricky() {
    if constexpr (std::is_same_v<T, int>) {
        // some_undefined_name();  // non-dependent name still errors
    }
}

// Pitfall 4: cannot detect whether a member exists
// detecting members requires decltype + void_t or C++20 concepts
```
