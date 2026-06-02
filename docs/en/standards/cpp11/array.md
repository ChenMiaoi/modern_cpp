---
title: "std::array Fixed-Size Array Container"
topic: unknown
feature: array
standard: N/A
status_checked_at: 2026-06-02
---
# std::array Fixed-Size Array Container

## Overview

`std::array<T, N>` is a fixed-size array container introduced in C++11, a zero-overhead wrapper around native C arrays. Unlike `std::vector`, its size is determined at compile time and it does not allocate memory on the heap; unlike C arrays, it has a full container interface, supports copy assignment, and carries size information.

`std::array` is "an array that knows its own size," suitable for replacing all scenarios that need fixed-size sequences.

## API Overview

| Member Function | Description |
|----------------|-------------|
| `at(i)` | Bounds-checked, throws `std::out_of_range` on out-of-bounds |
| `operator[]` | Unchecked element access |
| `front()` / `back()` | First/last element reference |
| `data()` | Underlying array pointer, compatible with C APIs |
| `fill(v)` | Fill all elements with value `v` |
| `size()` | Number of elements (`constexpr`) |
| `empty()` | Whether empty (true when `N == 0`) |
| `begin()` / `end()` | Forward iterators |
| `rbegin()` / `rend()` | Reverse iterators |

## Basic Usage and Initialization

```cpp
#include <array>

// Aggregate initialization (same syntax as C arrays)
std::array<int, 5> a1 = {1, 2, 3, 4, 5};

// C++14 and later support more concise initialization
std::array<int, 5> a2{10, 20, 30, 40, 50};

// Partial initialization, remaining elements value-initialized to 0
std::array<int, 5> a3 = {1, 2};  // {1, 2, 0, 0, 0}

// All-zero initialization
std::array<int, 5> a4{};  // {0, 0, 0, 0, 0}
```

## Comparison with C Arrays and std::vector

```cpp
// C array: decays to pointer when passed as argument, loses size info, not copy-assignable
int c_arr[5] = {1, 2, 3, 4, 5};

// std::array: value semantics, copyable, size is part of the type
std::array<int, 5> arr = {1, 2, 3, 4, 5};
std::array<int, 5> arr2 = arr;  // legal full copy

// std::vector: dynamic size, heap allocation, suitable for runtime-determined sizes
std::vector<int> vec = {1, 2, 3, 4, 5};
```

**Selection guide:**
- Size known and fixed at compile time → `std::array`
- Size determined at runtime or needs dynamic adjustment → `std::vector`
- Needs C API interaction with fixed size → `std::array` (`data()` returns a raw pointer)

## constexpr Size and Compile-Time Properties

`std::array`'s `size()` returns a `constexpr` value, usable in template parameters and compile-time computation:

```cpp
constexpr std::array<int, 4> ca = {1, 2, 3, 4};

static_assert(ca.size() == 4, "size must be 4");  // compile-time check

template <typename T, std::size_t N>
constexpr std::size_t array_size(const std::array<T, N>&) {
    return N;  // compile-time size retrieval
}

static_assert(array_size(ca) == 4, "");
```

## Element Access

```cpp
std::array<int, 5> arr = {10, 20, 30, 40, 50};

int val = arr[2];    // 30, no check, out-of-bounds is undefined behavior

try {
    int v = arr.at(10);  // throws std::out_of_range
} catch (const std::out_of_range&) { /* handle */ }

int first = arr.front();  // 10
int last  = arr.back();   // 50
int* ptr  = arr.data();   // underlying array pointer, usable with C APIs
```

## fill, Iteration, and STL Algorithms

`std::array` is a full STL container, compatible with all standard algorithms:

```cpp
#include <algorithm>
#include <numeric>

std::array<int, 5> arr{};
arr.fill(42);  // {42, 42, 42, 42, 42}

// Range for
for (const auto& elem : arr) { std::cout << elem << ' '; }

// Sort, find, accumulate
std::array<int, 5> data = {5, 3, 1, 4, 2};
std::sort(data.begin(), data.end());       // {1, 2, 3, 4, 5}

auto it = std::find(data.begin(), data.end(), 3);
auto idx = std::distance(data.begin(), it);  // 2

int sum = std::accumulate(data.begin(), data.end(), 0);  // 15

// minmax_element
auto [min_it, max_it] = std::minmax_element(data.begin(), data.end());

// count_if
auto cnt = std::count_if(data.begin(), data.end(),
    [](int x) { return x > 3; });  // 2

// transform
std::array<int, 5> squared;
std::transform(data.begin(), data.end(), squared.begin(),
    [](int x) { return x * x; });
```

## Structured Bindings (C++17)

```cpp
// C++17 allows structured bindings on std::array
std::array<int, 3> point = {10, 20, 30};
auto [x, y, z] = point;  // x=10, y=20, z=30
```

## Multidimensional Fixed Arrays

```cpp
// 2D array: array of array
std::array<std::array<int, 3>, 2> matrix = {{{1, 2, 3}, {4, 5, 6}}};
int val = matrix[1][2];  // 6

for (const auto& row : matrix) {
    for (int elem : row) { std::cout << elem << ' '; }
    std::cout << '\n';
}
```

## Best Practices

1. **Prefer `std::array` over C arrays**: Type-safe, copyable, does not decay to pointer.
2. **Use `at()` for debug checks, `operator[]` for production paths**.
3. **Leverage `data()` for C API interaction**: Returns a pointer to contiguous memory.
4. **For multidimensional scenarios, nest `std::array`**: `std::array<std::array<T, C>, R>` is better than `T[R][C]`.
5. **Empty `std::array<T, 0>` is a valid type**: `size()` is 0, useful for template edge cases.

## Common Pitfalls

- **Cannot dynamically resize**: Use `std::vector` if you need `push_back`.
- **`at()` has runtime overhead**: Checks bounds on every access; may become a bottleneck in hot paths.
- **Size is part of the type**: `std::array<int, 3>` and `std::array<int, 4>` are different types.
- **Initialization syntax pitfall**: `std::array<int, 3> a = {1};` only initializes the first element to 1, the rest to 0. Double braces `{{}}` are sometimes required in C++11.
- **No `push_back` / `insert` / `erase`**: Fixed-size containers do not support dynamic size modification.
