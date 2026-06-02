---
title: "多维下标运算符"
topic: unknown
feature: multidimensional-subscript
standard: N/A
status_checked_at: 2026-06-02
---
# 多维下标运算符

C++23 允许 `operator[]` 接受多个参数，即 `operator[](size_t, size_t, ...)`。这使得矩阵、张量等多维容器可以直接用 `m[i, j]` 语法访问元素。

## 基本语法

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

## C++23 之前的变通方案

```cpp
// 方案1: operator() — 一直可用，但非下标语法
struct MatrixOld {
    int& operator()(size_t r, size_t c) { return data[r * cols + c]; }
};

// 方案2: 代理对象链式下标
struct MatrixProxy {
    int* row_ptr;
    int& operator[](size_t c) { return row_ptr[c]; }
};
struct MatrixWithProxy {
    MatrixProxy operator[](size_t r) { return {data.data() + r * cols}; }
};
MatrixWithProxy m;
m[1][2] = 42;

// 方案3: pair + 自定义 key
struct MatrixTuple {
    int& operator[](std::pair<size_t, size_t> key) {
        return data[key.first * cols + key.second];
    }
};
MatrixTuple m2;
m2[{1, 2}] = 42;  // 需要花括号
```

C++23 的多参数 `operator[]` 最为直接自然。

## 与 std::mdspan 配合

```cpp
#include <mdspan>
#include <vector>

std::vector<int> data(12);
std::mdspan<int, std::extents<size_t, 3, 4>> mat(data.data());

mat[1, 2] = 42;
int val = mat[0, 3];
```

## 张量类示例

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

## 逗号表达式的歧义

C++23 之前 `a[b, c]` 中逗号是逗号运算符，`b, c` 求值为 `c`。C++23 修改了规则：

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
int a = old_arr[1, 2];  // 仍是逗号运算符，a = old_arr[2]

New new_arr;
int b = new_arr[1, 2];  // 调用 operator[](1, 2)
```

规则：如果类型定义了多参数 `operator[]`，则 `x[a, b]` 调用多维版本；否则逗号运算符仍生效。

## auto 返回类型

```cpp
struct Fancy {
    auto& operator[](size_t i, size_t j) { return data_[i][j]; }
    std::vector<std::vector<int>> data_;
};

Fancy f;
auto& val = f[1, 2];       // 推导为 int&
```

## C++26: tuple-based 索引

C++26 计划支持 `std::tuple` 索引，允许 `m[std::tuple{1, 2}]` 语法，进一步统一多维访问模式，并可配合 `std::mdspan` 的 `submdspan` 使用切片索引。

## 注意事项

- 仅 C++23 及之后可用
- 运算符优先级：`m[i, j]` 中逗号是下标分隔符，不是逗号运算符（仅当类型定义了多维 `operator[]`）
- 混合使用一维和多维 `operator[]` 合法，编译器根据参数数量选择
- 模板化的 `operator[]` 可接受可变参数：`template <typename... Idx> auto& operator[](Idx... idx)`