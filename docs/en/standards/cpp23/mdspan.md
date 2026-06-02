---
title: std::mdspan
topic: cpp23
feature: mdspan
standard: C++23
status_checked_at: 2026-06-01
exercises:
  - exercises/cpp23/mdspan1.cpp
solutions:
  - exercises/solutions/mdspan1.cpp
---
# std::mdspan

`std::mdspan` is a multidimensional array view introduced in C++23, providing multidimensional access to contiguous memory. It does not own memory; it only describes how to map linear indices to multidimensional coordinates.

## Basic Usage

```cpp
#include <mdspan>
#include <vector>
#include <iostream>

int main() {
    std::vector<int> data(12);
    std::iota(data.begin(), data.end(), 1);

    // Static extents
    std::mdspan<int, std::extents<size_t, 3, 4>> mat(data.data());

    // Dynamic extents
    std::mdspan dyn(data.data(), 3, 4);

    for (size_t i = 0; i < mat.extent(0); ++i) {
        for (size_t j = 0; j < mat.extent(1); ++j) {
            std::cout << mat[i, j] << " ";  // C++23 multidimensional subscript
        }
        std::cout << "\n";
    }
}
```

## Extents (Dimension Information)

```cpp
// Static — size known at compile time
using Matrix3x4 = std::mdspan<int, std::extents<size_t, 3, 4>>;
static_assert(Matrix3x4::static_extent(0) == 3);

// Dynamic — determined at runtime
using DynMatrix = std::mdspan<int, std::dextents<std::size_t, 2>>;
DynMatrix m(data.data(), rows, cols);

// Mixed
using Semi = std::mdspan<int, std::extents<size_t, 3, std::dynamic_extent>>;
Semi m2(data.data(), 4);  // Only dynamic dimensions need to be specified

// Dimension queries
m.rank();           // Number of dimensions
m.extent(0);        // Size of dimension 0
m.static_extent(0); // std::dynamic_extent (if dynamic)
m.size();            // Total element count
```

## Layout (Layout Policy)

```cpp
// layout_right — row-major (C-style, default)
std::mdspan<int, std::extents<size_t, 2, 3>, std::layout_right> m1(data.data());

// layout_left — column-major (Fortran-style)
std::mdspan<int, std::extents<size_t, 2, 3>, std::layout_left> m2(data.data());

// layout_stride — custom strides
std::array<size_t, 2> strides{6, 2};
std::layout_stride::mapping mapping(
    std::extents<size_t, 3, 4>{}, strides);
std::mdspan m3(data.data(), mapping);
```

## Matrix Operation Example

```cpp
#include <mdspan>
#include <vector>

using Matrix = std::mdspan<double, std::dextents<std::size_t, 2>>;

void matmul(Matrix A, Matrix B, Matrix C) {
    size_t M = A.extent(0), N = B.extent(1), K = A.extent(1);
    for (size_t i = 0; i < M; ++i)
        for (size_t j = 0; j < N; ++j) {
            double sum = 0.0;
            for (size_t k = 0; k < K; ++k)
                sum += A[i, k] * B[k, j];
            C[i, j] = sum;
        }
}

int main() {
    constexpr size_t M = 4, N = 3, K = 2;
    std::vector<double> a_data(M * K, 1.0), b_data(K * N, 2.0), c_data(M * N, 0.0);
    Matrix A(a_data.data(), M, K), B(b_data.data(), K, N), C(c_data.data(), M, N);
    matmul(A, B, C);
}
```

## Comparison with Raw 2D Arrays

```cpp
// Raw approach — decay, no bounds info
void process(int arr[][4], int rows);  // Second dimension must be known at compile time

// mdspan — safe, flexible
void process(std::mdspan<int, std::dextents<std::size_t, 2>> m) {
    // m.extent(0), m.extent(1) can be obtained at runtime
}
```

## submdspan (Sub-views)

C++23 introduces `std::submdspan` to create sub-matrix views (zero-copy):

```cpp
#include <mdspan>

std::mdspan full(data.data(), 8, 8);

// Take rows 2-4, columns 1-3
auto sub = std::submdspan(full,
    std::tuple{2, 5}, std::tuple{1, 4});
// sub.extent(0) == 3, sub.extent(1) == 3

// Take a single row
auto row3 = std::submdspan(full, 3, std::full_extent);

// Take a single column
auto col2 = std::submdspan(full, std::full_extent, 2);
```

## Accessor (Access Policy)

Accessor controls how elements are accessed from the underlying handle. By default, `std::default_accessor<T>` uses direct pointer dereference. Custom accessors can add features like bounds checking.

## Caveats

- `mdspan` is a zero-overhead abstraction: no virtual functions, no heap allocation, no bounds checking (by default)
- Multidimensional subscript `m[i, j]` uses C++23's multi-argument `operator[]` feature
- An empty `mdspan` (any extent is 0) is valid; `data_handle()` may be null
- Thread safety depends on the underlying data; `mdspan` itself provides no synchronization
