---
title: "std::begin / std::end"
topic: unknown
feature: begin-end
standard: N/A
status_checked_at: 2026-06-20
---
# std::begin / std::end

## Overview

`std::begin` and `std::end` are free functions introduced in C++11 for obtaining iterators to a container or array in a uniform manner. Prior to C++11, obtaining begin and end iterators required member function calls (`.begin()` / `.end()`), which raw arrays do not support. `std::begin`/`std::end` unify both cases under a single syntax.

Header: `<iterator>`

```cpp
#include <iterator>
```

## Basic Usage

### Containers

```cpp
#include <vector>
#include <iterator>

std::vector<int> v = {1, 2, 3, 4, 5};

auto it_begin = std::begin(v);  // equivalent to v.begin()
auto it_end   = std::end(v);    // equivalent to v.end()

for (auto it = std::begin(v); it != std::end(v); ++it) {
    std::cout << *it << ' ';
}
```

### Raw Arrays

Raw arrays cannot call member functions, but `std::begin`/`std::end` handle them directly:

```cpp
int arr[] = {10, 20, 30, 40, 50};

for (auto it = std::begin(arr); it != std::end(arr); ++it) {
    std::cout << *it << ' ';
}
```

```cpp
// Without std::begin/std::end, manual pointer arithmetic is needed
for (int* p = arr; p != arr + 5; ++p) {
    std::cout << *p << ' ';
}
```

### Why Free Functions Are Needed

```cpp
template <typename Iter>
void print_range(Iter begin, Iter end) {
    for (auto it = begin; it != end; ++it) {
        std::cout << *it << ' ';
    }
}

// With member functions: only works for containers
std::vector<int> v = {1, 2, 3};
print_range(v.begin(), v.end());  // OK

int arr[] = {4, 5, 6};
// print_range(arr.begin(), arr.end());  // error: arrays have no member functions
print_range(std::begin(arr), std::end(arr));  // OK
```

## C++14 Enhancement: std::cbegin / std::cend

C++14 introduced `std::cbegin` and `std::cend`, which return `const_iterator`:

```cpp
#include <vector>
#include <iterator>

std::vector<int> v = {1, 2, 3};

for (auto it = std::cbegin(v); it != std::cend(v); ++it) {
    // *it = 10;  // compile error: const_iterator is not modifiable
    std::cout << *it << ' ';
}
```

```cpp
// In C++11, you needed the const overload of std::begin or std::as_const (C++17)
std::vector<int> v = {1, 2, 3};
auto it = std::begin(static_cast<const std::vector<int>&>(v));  // inconvenient
```

## Reverse Iterators: std::rbegin / std::rend

C++11 also provides reverse iterator versions:

```cpp
#include <vector>
#include <iterator>

std::vector<int> v = {1, 2, 3, 4, 5};

for (auto it = std::rbegin(v); it != std::rend(v); ++it) {
    std::cout << *it << ' ';  // 5 4 3 2 1
}
```

```cpp
// Raw arrays are also supported
int arr[] = {10, 20, 30};
for (auto it = std::rbegin(arr); it != std::rend(arr); ++it) {
    std::cout << *it << ' ';  // 30 20 10
}
```

C++14 also provides `std::crbegin` and `std::crend`.

## Interaction with Range-Based for Loop

The C++11 range-based for loop internally uses `std::begin`/`std::end`:

```cpp
std::vector<int> v = {1, 2, 3};
for (auto x : v) { /* ... */ }  // compiler transforms to use std::begin/std::end

int arr[] = {4, 5, 6};
for (auto x : arr) { /* ... */ }  // works the same way
```

## Code Examples

### Unified Iteration in Generic Algorithms

```cpp
#include <iterator>
#include <vector>
#include <list>
#include <iostream>

template <typename Container>
void print_all(const Container& c) {
    // use cbegin/cend to guarantee read-only iteration
    for (auto it = std::cbegin(c); it != std::cend(c); ++it) {
        std::cout << *it << ' ';
    }
    std::cout << '\n';
}

int main() {
    std::vector<int> v = {1, 2, 3};
    std::list<double> l = {1.1, 2.2, 3.3};
    int arr[] = {10, 20, 30};

    print_all(v);  // 1 2 3
    print_all(l);  // 1.1 2.2 3.3
    print_all(arr); // 10 20 30
}
```

### Usage with the Algorithm Library

```cpp
#include <algorithm>
#include <iterator>
#include <vector>
#include <iostream>

int main() {
    std::vector<int> v = {5, 3, 1, 4, 2};

    // std::begin/std::end can be passed directly to algorithms
    std::sort(std::begin(v), std::end(v));

    // reverse sort
    std::sort(std::rbegin(v), std::rend(v));

    for (auto x : v) {
        std::cout << x << ' ';  // 5 4 3 2 1
    }
}
```

## Notes and Pitfalls

**std::end on empty containers** — `std::end` returns an iterator equal to `std::begin` for empty containers; dereferencing it is undefined behavior:

```cpp
std::vector<int> empty;
auto it = std::begin(empty);
if (it != std::end(empty)) {
    std::cout << *it;  // safe
}
```

**Array size must be fixed** — `std::begin`/`std::end` require the array size to be known at compile time:

```cpp
int arr[] = {1, 2, 3};        // OK: compile-time size
// int n; std::cin >> n;
// int arr2[n];                // VLA: not standard C++, std::begin may not work
```

## Comparison with Pre-C++11

| Feature | C++03 | C++11 |
|---------|-------|-------|
| Container begin iterator | `c.begin()` | `std::begin(c)` |
| Container end iterator | `c.end()` | `std::end(c)` |
| Array begin pointer | `arr` or `&arr[0]` | `std::begin(arr)` |
| Array end pointer | `arr + N` | `std::end(arr)` |
| Const iterator | `c.begin()` return type | `std::cbegin(c)` (C++14) |
| Reverse iterator | `c.rbegin()` / `c.rend()` | `std::rbegin(c)` / `std::rend(c)` |

## Compiler Support

| Compiler | Supported Since | Notes |
|----------|-----------------|-------|
| GCC | 4.5+ | Full support |
| Clang | 3.1+ | Full support |
| MSVC | 2012 (17.0)+ | Full support |

The introduction of `std::begin`/`std::end` makes handling containers and arrays in generic programming more consistent, and serves as foundational infrastructure for range-based for loops and many modern C++ idioms.
