---
title: "Variadic Templates"
topic: unknown
feature: variadic-templates
standard: N/A
status_checked_at: 2026-06-02
---
# Variadic Templates

## Overview

Variadic templates are a core feature introduced in C++11 that allow templates to accept **any number of arguments of any type**. They are the building block for type-safe variadic functions, perfect forwarding, and standard library components like `std::tuple`.

Before C++11, handling a variable number of arguments relied on C-style `va_list` (no type safety, no support for non-POD types) or overloading multiple fixed-argument versions. Variadic templates completely solved this problem.

## Basic Syntax

```cpp
// Args is the template parameter pack, args is the function parameter pack
template<typename... Args>
void f(Args... args) {
    static_assert(sizeof...(Args) == sizeof...(args), "counts must match");
}
```

The ellipsis `...` has two meanings: after a type name it **declares a pack**, after a pack name it **expands a pack**. `sizeof...` returns the number of elements in a pack.

## Recursive Expansion

The standard C++11 technique for expanding parameter packs is **recursion** — providing a termination overload and peeling off arguments step by step:

```cpp
void print() { std::cout << '\n'; }  // base case

template<typename T, typename... Args>
void print(const T& first, const Args&... rest) {
    std::cout << first;
    if (sizeof...(rest) > 0) std::cout << ", ";
    print(rest...);  // strip first, recurse
}

print(1, "hello", 3.14, 'c');  // "1, hello, 3.14, c"
```

## Initializer List Expansion (C++11 Alternative to Fold Expressions)

C++17 introduced fold expressions; in C++11, use an initializer list with the comma operator:

```cpp
template<typename... Args>
void printAll(const Args&... args) {
    int dummy[] = { (std::cout << args << ' ', 0)... };
    (void)dummy;
}
```

## Perfect Forwarding

Variadic templates combined with `std::forward` preserve the lvalue/rvalue property of arguments:

```cpp
template<typename T, typename... Args>
std::unique_ptr<T> my_make_unique(Args&&... args) {
    // Args&& is a forwarding reference; std::forward preserves the original value category
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

auto w = my_make_unique<Widget>(42, std::string("hello"));
```

When an lvalue is passed, `Args` deduces to a reference type (`T&`); for rvalues, it deduces to a value type (`T`). `std::forward<Args>` decides whether to move or copy accordingly.

## `index_sequence` Technique

```cpp
// C++11: manually defined (C++14 provides std::index_sequence)
template<std::size_t... Is> struct index_sequence {};

template<std::size_t N, std::size_t... Is>
struct make_index_sequence : make_index_sequence<N - 1, N - 1, Is...> {};

template<std::size_t... Is>
struct make_index_sequence<0, Is...> : index_sequence<Is...> {};
```

Use case — iterating over a tuple by index:

```cpp
template<typename Tuple, std::size_t... Is>
void printTupleImpl(const Tuple& t, index_sequence<Is...>) {
    print(std::get<Is>(t)...);
}

template<typename... Args>
void printTuple(const std::tuple<Args...>& t) {
    printTupleImpl(t, make_index_sequence<sizeof...(Args)>{});
}
```

## Simplified Tuple Implementation

```cpp
template<typename... Types> struct Tuple;

template<typename Head, typename... Tail>
struct Tuple<Head, Tail...> : Tuple<Tail...> {
    Head value;
    Tuple(const Head& h, const Tail&... tail)
        : Tuple<Tail...>(tail...), value(h) {}
    Head& head() { return value; }
    Tuple<Tail...>& tail() { return *this; }
};

template<> struct Tuple<> {};
```

The standard library implementation is far more complex (empty base class optimization, construction/assignment/comparison), but the core idea is the same: recursive inheritance.

## Implementing `make_unique`

`std::make_unique` was introduced in C++14; in C++11, you need to implement it yourself — a perfect application of variadic templates:

```cpp
template<typename T, typename... Args>
std::unique_ptr<T> my_make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
```

## Best Practices

| Practice | Explanation |
|----------|-------------|
| `sizeof...(Args)` over `sizeof...(args)` | More intuitive for representing type count |
| Use `Args&&...` when perfect forwarding is needed | Otherwise `const Args&...` suffices |
| Termination function declared first | Ensures the compiler can see it |
| Upgrade to C++14 `index_sequence` | No longer needs manual definition |

## Common Pitfalls

**Missing recursion termination** — no termination version causes compilation failure:

```cpp
// Error: f() has no definition
template<typename T, typename... Args>
void f(T first, Args... rest) { f(rest...); }

// Correct: termination version
void f() {}
```

**`Args&&` is not always an rvalue reference** — when an lvalue is passed, it deduces to an lvalue reference. This is a forwarding reference, not an rvalue reference, and must be used with `std::forward`.

**Misplaced expansion** — `...` can only follow a pack name:

```cpp
f(args..., 42);   // OK: expands to f(a1, a2, ..., 42)
// f(args, 42)...;  // error
```

## Comparison with Pre-C++11

| Feature | `va_list` | Variadic Templates |
|---------|-----------|-------------------|
| Type safe | No | Yes |
| Supports non-POD | No | Yes |
| Supports references | No | Yes |
| Compile-time checking | No | Yes |
| Inlinable | No | Yes |
