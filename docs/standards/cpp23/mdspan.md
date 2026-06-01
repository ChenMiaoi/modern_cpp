# std::mdspan

`std::mdspan` 是 C++23 引入的多维数组视图，提供对连续内存的多维访问。它不拥有内存，仅描述如何将线性索引映射到多维坐标。

## 基本用法

```cpp
#include <mdspan>
#include <vector>
#include <iostream>

int main() {
    std::vector<int> data(12);
    std::iota(data.begin(), data.end(), 1);

    // 静态 extents
    std::mdspan<int, std::extents<size_t, 3, 4>> mat(data.data());

    // 动态 extents
    std::mdspan dyn(data.data(), 3, 4);

    for (size_t i = 0; i < mat.extent(0); ++i) {
        for (size_t j = 0; j < mat.extent(1); ++j) {
            std::cout << mat[i, j] << " ";  // C++23 多维下标
        }
        std::cout << "\n";
    }
}
```

## Extents（维度信息）

```cpp
// 静态 — 编译期已知大小
using Matrix3x4 = std::mdspan<int, std::extents<size_t, 3, 4>>;
static_assert(Matrix3x4::static_extent(0) == 3);

// 动态 — 运行时确定
using DynMatrix = std::mdspan<int, std::dynamic_extent, std::dynamic_extent>;
DynMatrix m(data.data(), rows, cols);

// 混合
using Semi = std::mdspan<int, std::extents<size_t, 3, std::dynamic_extent>>;
Semi m2(data.data(), 4);  // 只需指定动态维度

// 维度查询
m.rank();           // 维度数
m.extent(0);        // 第 0 维大小
m.static_extent(0); // std::dynamic_extent（若为动态）
m.size();            // 总元素数
```

## Layout（布局策略）

```cpp
// layout_right — 行优先（C 风格，默认）
std::mdspan<int, std::extents<size_t, 2, 3>, std::layout_right> m1(data.data());

// layout_left — 列优先（Fortran 风格）
std::mdspan<int, std::extents<size_t, 2, 3>, std::layout_left> m2(data.data());

// layout_stride — 自定义步幅
std::array<size_t, 2> strides{6, 2};
std::layout_stride::mapping mapping(
    std::extents<size_t, 3, 4>{}, strides);
std::mdspan m3(data.data(), mapping);
```

## 矩阵操作示例

```cpp
#include <mdspan>
#include <vector>

using Matrix = std::mdspan<double, std::dynamic_extent, std::dynamic_extent>;

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

## 与原始二维数组对比

```cpp
// 原始方式 — 退化、无边界信息
void process(int arr[][4], int rows);  // 第二维必须编译期已知

// mdspan — 安全、灵活
void process(std::mdspan<int, std::dynamic_extent, std::dynamic_extent> m) {
    // m.extent(0), m.extent(1) 可在运行时获取
}
```

## submdspan（子视图）

C++23 引入 `std::submdspan` 创建子矩阵视图（零拷贝）：

```cpp
#include <mdspan>

std::mdspan full(data.data(), 8, 8);

// 取第 2-4 行、第 1-3 列
auto sub = std::submdspan(full,
    std::tuple{2, 5}, std::tuple{1, 4});
// sub.extent(0) == 3, sub.extent(1) == 3

// 取单行
auto row3 = std::submdspan(full, 3, std::full_extent);

// 取单列
auto col2 = std::submdspan(full, std::full_extent, 2);
```

## Accessor（访问策略）

Accessor 控制从底层句柄访问元素的方式，默认使用 `std::default_accessor<T>` 直接指针解引用。可自定义以添加边界检查等特性。

## 注意事项

- `mdspan` 是零开销抽象：无虚函数、无堆分配、无边界检查（默认）
- 多维下标 `m[i, j]` 使用 C++23 的 `operator[]` 多参数特性
- 空 `mdspan`（任意 extent 为 0）合法，`data_handle()` 可以为空
- 线程安全性取决于底层数据，`mdspan` 本身无同步
