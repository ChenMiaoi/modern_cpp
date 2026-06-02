---
title: "auto Type Deduction"
topic: unknown
feature: auto-type-deduction
standard: N/A
status_checked_at: 2026-06-02
---
# auto Type Deduction

## Overview

The `auto` keyword lets the compiler deduce a variable's type from its initializer expression, reducing redundant type declarations.

## Basic Usage

```cpp
auto i = 42;            // int
auto d = 3.14;          // double
auto s = std::string("hello");  // std::string
auto it = vec.begin();  // std::vector<int>::iterator
```

## Deduction Rules

`auto` uses template argument deduction rules (drops references and top-level `const`):

```cpp
const int ci = 10;
auto a = ci;      // int (top-level const dropped)

int& ri = i;
auto b = ri;      // int (reference dropped)

const int& cri = ci;
auto c = cri;     // int (reference and top-level const dropped)
```

### Preserving Low-level Const and References

Use `auto&` or `const auto&` to preserve references:

```cpp
const int ci = 10;
auto& d = ci;         // const int& (low-level const preserved)

int i = 42;
auto& e = i;          // int&

const int& cri = ci;
auto& f = cri;        // const int& (reference and low-level const preserved)
```

## Use Cases

### Iterators

```cpp
// Before C++11:
for (std::vector<std::pair<int, std::string>>::const_iterator it = vec.begin();
     it != vec.end(); ++it) { ... }

// C++11:
for (auto it = vec.begin(); it != vec.end(); ++it) { ... }

// C++11 range-for (better choice):
for (const auto& [key, value] : vec) { ... }  // C++17 structured bindings
```

### Template Return Values

```cpp
template<typename T, typename U>
auto add(T t, U u) -> decltype(t + u) {  // C++11 trailing return type
    return t + u;
}
```

### Complex Types

```cpp
std::map<std::string, std::vector<int>> data;
auto& bucket = data["key"];  // std::vector<int>&
```

## Caveats

### Not Usable as Function Parameter (C++11/14)

```cpp
// Not allowed in C++11/14:
void foo(auto x);  // error

// Allowed with C++20 Concepts:
void foo(auto x);  // equivalent to template<typename T> void foo(T x);
```

### Array and Reference Decay

```cpp
int arr[5] = {1, 2, 3, 4, 5};
auto a = arr;       // int* (array decays to pointer)
auto& b = arr;      // int(&)[5] (reference preserves array type)
```

### Uniform Initialization

```cpp
auto x = {1, 2, 3};  // std::initializer_list<int>!
auto y{42};           // C++11: std::initializer_list<int>
                      // C++17: int (rule changed)
```

## Best Practices

- **Prefer `auto`**: reduces redundancy, avoids narrowing, adapts to generic code
- **Use explicit types when clearer**: when the type affects readability (e.g., the type of literal `0` is ambiguous)
- **Use `const auto&` in range-for**: avoids copies, signals immutability intent
- **Smart pointers and iterators**: almost always use `auto`

## Relationship with Other Features

| Feature | Relationship |
|---------|-------------|
| `decltype` | Obtains the exact type of an expression (including references) |
| `decltype(auto)` | C++14, combines the strengths of both |
| Structured bindings | C++17, `auto [a, b] = pair;` |
| CTAD | C++17, class template argument deduction |
