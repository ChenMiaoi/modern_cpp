---
title: "Multidimensional Subscript Operator"
topic: unknown
feature: multidimensional-subscript
standard: N/A
status_checked_at: 2026-06-02
---
# Multidimensional Subscript Operator

C++23 allows `operator[]` to accept multiple arguments, i.e., `operator[](size_t, size_t, ...)`. This enables multidimensional containers like matrices and tensors to access elements directly with the `m[i, j]` syntax.

## Basic Syntax

```cpp
#include <vector>

struct Matrix {
    std::vector<int> data;
    size_t cols;

    Matrix(size_t rows, size_t cols) : data(rows * cols), cols(cols) {}

    int& operator[](size_t row, size_t col) {
        return data[row * cols + col];
    }
    const int& operator[](size_t row, size_t col) const {
        return data[row * cols + col];
    }
};

int main() {
    Matrix m(3, 4);
    m[1, 2] = 42;
    int val = m[1, 2];     // 42
}
```

## Pre-C++23 Workarounds

```cpp
// Option 1: operator() — always available, but not subscript syntax
struct MatrixOld {
    int& operator()(size_t r, size_t c) { return data[r * cols + c]; }
};

// Option 2: Proxy object with chained subscripts
struct MatrixProxy {
    int* row_ptr;
    int& operator[](size_t c) { return row_ptr[c]; }
};
struct MatrixWithProxy {
    MatrixProxy operator[](size_t r) { return {data.data() + r * cols}; }
};
MatrixWithProxy m;
m[1][2] = 42;

// Option 3: pair + custom key
struct MatrixTuple {
    int& operator[](std::pair<size_t, size_t> key) {
        return data[key.first * cols + key.second];
    }
};
MatrixTuple m2;
m2[{1, 2}] = 42;  // Requires braces
```

C++23's multi-argument `operator[]` is the most direct and natural approach.

## Integration with std::mdspan

```cpp
#include <mdspan>
#include <vector>

std::vector<int> data(12);
std::mdspan<int, std::extents<size_t, 3, 4>> mat(data.data());

mat[1, 2] = 42;
int val = mat[0, 3];
```

## Tensor Class Example

```cpp
#include <vector>

template <typename T>
class Tensor3D {
    std::vector<T> data_;
    size_t d0_, d1_, d2_;

public:
    Tensor3D(size_t d0, size_t d1, size_t d2)
        : data_(d0 * d1 * d2), d0_(d0), d1_(d1), d2_(d2) {}

    T& operator[](size_t i, size_t j, size_t k) {
        return data_[i * d1_ * d2_ + j * d2_ + k];
    }
    const T& operator[](size_t i, size_t j, size_t k) const {
        return data_[i * d1_ * d2_ + j * d2_ + k];
    }
};

Tensor3D<float> t(3, 4, 5);
t[1, 2, 3] = 3.14f;
```

## Comma Expression Ambiguity

Before C++23, the comma in `a[b, c]` was the comma operator, and `b, c` evaluated to `c`. C++23 changed the rules:

```cpp
struct Old {
    int& operator[](size_t i) { return data[i]; }
    int data[10] = {};
};

struct New {
    int& operator[](size_t i, size_t j) { return data[i * 10 + j]; }
    int data[100] = {};
};

Old old_arr;
int a = old_arr[1, 2];  // Still comma operator, a = old_arr[2]

New new_arr;
int b = new_arr[1, 2];  // Calls operator[](1, 2)
```

Rule: if a type defines a multi-argument `operator[]`, then `x[a, b]` calls the multidimensional version; otherwise the comma operator still applies.

## auto Return Type

```cpp
struct Fancy {
    auto& operator[](size_t i, size_t j) { return data_[i][j]; }
    std::vector<std::vector<int>> data_;
};

Fancy f;
auto& val = f[1, 2];       // Deduced as int&
```

## C++26: Tuple-Based Indexing

C++26 plans to support `std::tuple` indexing, allowing `m[std::tuple{1, 2}]` syntax, further unifying multidimensional access patterns and enabling slice indexing with `std::mdspan`'s `submdspan`.

## Caveats

- Only available in C++23 and later
- Operator precedence: in `m[i, j]`, the comma is a subscript separator, not the comma operator (only when the type defines a multidimensional `operator[]`)
- Mixing one-dimensional and multidimensional `operator[]` is valid; the compiler selects based on the number of arguments
- A templated `operator[]` can accept variadic arguments: `template <typename... Idx> auto& operator[](Idx... idx)`
