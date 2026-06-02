---
title: "`std::span`"
topic: unknown
feature: span
standard: N/A
status_checked_at: 2026-06-02
---
# `std::span`

## Overview

`std::span<T>` is a non-owning view over a contiguous memory region, unifying the interface of C arrays, `std::array`, `std::vector`, and any contiguous storage. It replaces `(pointer, size)` pairs as the standard way to pass function parameters.

## Basic Usage

```cpp
#include <span>
#include <vector>
#include <iostream>

void print(std::span<const int> data) {
    for (int v : data) std::cout << v << ' ';
    std::cout << '\n';
}

int main() {
    std::vector<int> v = {1, 2, 3, 4, 5};
    int arr[] = {10, 20, 30};
    std::array<int, 3> sa = {100, 200, 300};

    print(v);    // OK
    print(arr);  // OK
    print(sa);   // OK
}
```

## Static vs Dynamic Extent

```cpp
std::span<int> dyn(data, 5);        // dynamic extent
std::span<int, 5> stat(arr, 5);     // static extent: size known at compile time

// sizeof(std::span<int>)     == 16 (pointer + size)
// sizeof(std::span<int, 5>)  == 8 (pointer only)
```

### Static Extent Automatic Deduction

```cpp
int arr[4] = {1, 2, 3, 4};
std::span s1(arr);         // span<int, 4> (static)
std::span s2(arr, 3);      // span<int> (dynamic, first 3)

std::vector<int> v = {1, 2, 3};
std::span s3(v);           // span<int> (dynamic)

std::array<int, 5> a = {};
std::span s4(a);           // span<int, 5> (static)
```

## Sub-view Operations

```cpp
int data[] = {0, 1, 2, 3, 4, 5, 6};
std::span s(data);

auto first3 = s.first(3);      // {0, 1, 2}
auto last2 = s.last(2);        // {5, 6}
auto mid = s.subspan(2, 3);    // {2, 3, 4}
auto rest = s.subspan(2);      // {2, 3, 4, 5, 6}
```

## Replacing `pointer + size`

```cpp
// Old style
void process_old(int* data, size_t count) {
    for (size_t i = 0; i < count; ++i) data[i] *= 2;
}

// New style
void process_new(std::span<int> data) {
    for (auto& v : data) v *= 2;
}

// Callers need no changes
std::vector<int> v = {1, 2, 3};
int arr[] = {4, 5, 6};
process_new(v);
process_new(arr);
```

## Read-Only Views

```cpp
double sum(std::span<const double> values) {
    double s = 0;
    for (double v : values) s += v;
    return s;
}

std::vector<double> v = {1.0, 2.5, 3.7};
double total = sum(v);  // implicit conversion to span<const double>
```

## Comparison with `std::string_view`

| Feature | `std::span<T>` | `std::string_view` |
|---------|----------------|---------------------|
| Element type | Any | Character types |
| Writable | Yes (`span<T>`) | Read-only only |
| Use case | General contiguous data | String operations |

```cpp
// Strings still use string_view
std::string_view sv = "hello";

// Non-character contiguous data uses span
std::span<const uint8_t> bytes(buffer, len);
```

## Working with Range Algorithms

```cpp
#include <span>
#include <algorithm>

void sort_in_place(std::span<int> data) {
    std::sort(data.begin(), data.end());
}

int arr[] = {3, 1, 4, 1, 5, 9};
sort_in_place(arr);
```

## `as_bytes` / `as_writable_bytes`

```cpp
#include <span>

int data[] = {0x12345678, 0x9ABCDEF0};
auto bytes = std::as_bytes(std::span(data));           // read-only byte view
auto wbytes = std::as_writable_bytes(std::span(data)); // writable byte view
```

## Common Pitfalls

```cpp
// Pitfall 1: constructing span from temporary object → dangling reference
// std::span<int> s = std::vector<int>{1, 2, 3};  // UB!

// Pitfall 2: not checking bounds
std::span<int> s(arr, 3);
// s[5] = 10;  // UB (same as raw pointer)

// Pitfall 3: static extent construction does no runtime checking
// std::span<int, 5> s5(arr, 3);  // arr has only 3 elements → UB

// Pitfall 4: span does not own data — ensure the underlying storage outlives it
std::span<int> dangling() {
    std::vector<int> v = {1, 2, 3};
    return std::span<int>(v);  // span is invalid after v is destroyed
}
```

## Summary

- `span<T>` is a lightweight non-owning view over contiguous memory, replacing the `(ptr, size)` pattern.
- Static extent (`span<T, N>`) eliminates the overhead of storing size.
- `span` does not own data — ensure the underlying storage outlives it.
- Prefer `span<const T>` for read-only parameters, `span<T>` for read-write parameters.
- Use `string_view` for strings; use `span` for non-string contiguous data.
