// cpplings: mdspan1
// 主题: C++23 — 多维数组视图 (std::mdspan)
//
// TODO: 使用 std::mdspan 创建多维数组视图
//   - mdspan<T, extents<...>> 表示二维矩阵
//   - 动态 extent vs 静态 extent
//   - layout_right（行主序）布局
//   - 元素访问和 extent 查询
//
// 提示: std::mdspan<int, std::dextents<size_t, 2>> m(data, rows, cols);
//       m(r, c) 访问元素，m.extent(0) 行数，m.extent(1) 列数

#include "cpplings.h"

#if __cpp_lib_mdspan >= 202207L
#include <mdspan>
#include <vector>
#include <array>
#include <cstddef>

// mdspan 直接使用，无需额外实现

TEST("mdspan — 二维元素访问 (row-major)") {
    // 创建 4×3 矩阵，值 1~12
    // std::array<int, 12> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    // std::mdspan m(data.data(), 4, 3);
    // ASSERT_EQ(m(0, 0), 1);
    // ASSERT_EQ(m(0, 2), 3);
    // ASSERT_EQ(m(1, 0), 4);
    // ASSERT_EQ(m(3, 2), 12);
    int _todo_ = "请删除此行，实现上面的 TODO";
}

TEST("mdspan — extent 查询") {
    // std::array<int, 6> data = {1, 2, 3, 4, 5, 6};
    // std::mdspan m(data.data(), 2, 3);
    // ASSERT_EQ(m.extent(0), 2u);  // 2 行
    // ASSERT_EQ(m.extent(1), 3u);  // 3 列
    // ASSERT_EQ(m.rank(), 2u);
    int _todo_ = "请删除此行，实现上面的 TODO";
}

TEST("mdspan — 静态 extent") {
    // TODO: 使用静态 extent 创建 mdspan
    // std::array<int, 6> data = {1, 2, 3, 4, 5, 6};
    // std::mdspan<int, std::extents<size_t, 2, 3>> m(data.data());
    // ASSERT_EQ(m.extent(0), 2u);
    // ASSERT_EQ(m(1, 2), 6);
    int _todo_ = "请删除此行，实现上面的 TODO";
}

TEST("mdspan — 行遍历 (通过指针算术)") {
    // TODO: 获取第 r 行的连续视图
    // 行主序下，一行是连续的
    // std::array<int, 12> data = {1,2,3,4,5,6,7,8,9,10,11,12};
    // std::mdspan m(data.data(), 4, 3);
    // // 第 1 行: [4, 5, 6]
    // std::mdspan row1(&m(1, 0), 3);
    // ASSERT_EQ(row1(0), 4);
    // ASSERT_EQ(row1(1), 5);
    // ASSERT_EQ(row1(2), 6);
    int _todo_ = "请删除此行，实现上面的 TODO";
}

TEST("mdspan — 列遍历 (layout_stride)") {
    // TODO: 使用 layout_stride 访问列
    // 行主序下，列不连续，需要 stride
    // std::array<int, 12> data = {1,2,3,4,5,6,7,8,9,10,11,12};
    // std::mdspan m(data.data(), 4, 3);
    // // 第 1 列: [2, 5, 8, 11]
    // std::extents<size_t, std::dynamic_extent> col_ext(4);
    // std::array<size_t, 1> strides = {3};
    // std::mdspan<int, decltype(col_ext), std::layout_stride> col(
    //     &m(0, 1), col_ext, strides);
    // ASSERT_EQ(col(0), 2);
    // ASSERT_EQ(col(1), 5);
    // ASSERT_EQ(col(2), 8);
    // ASSERT_EQ(col(3), 11);
    int _todo_ = "请删除此行，实现上面的 TODO";
}

TEST("mdspan — 通过 mdspan 修改数据") {
    // TODO: mdspan 是视图，修改反映到原数据
    // std::array<int, 6> data = {1, 2, 3, 4, 5, 6};
    // std::mdspan m(data.data(), 2, 3);
    // m(1, 1) = 99;
    // ASSERT_EQ(data[4], 99);  // row 1, col 1 = flat index 4
    int _todo_ = "请删除此行，实现上面的 TODO";
}

#else
// Fallback: simple Matrix2D wrapper when <mdspan> is unavailable

#include <vector>
#include <cstddef>

// TODO: 实现 Matrix2D — 简单二维矩阵视图
//   - 构造: Matrix2D(int* data, size_t rows, size_t cols)
//   - operator()(r, c) 访问元素
//   - extent(0) 返回行数，extent(1) 返回列数
//   - rank() 返回 2
//   - stride(1) 返回 1（行主序下步长）

struct Matrix2D {
    // TODO: 存储数据指针、行数、列数
    // int* data_;
    // size_t rows_, cols_;

    // TODO: 构造函数
    // Matrix2D(int* data, size_t rows, size_t cols) ...

    // TODO: operator()(r, c) — 返回 data_[r * cols_ + c] 的引用

    // TODO: extent(n) — 0 返回行数，1 返回列数

    // TODO: rank() — 返回 2

    // TODO: stride(1) — 行主序下返回 1

    int _todo_ = "请删除此行，实现上面的 TODO";
};

TEST("mdspan — 二维元素访问 (row-major) (fallback)") {
    int _todo_ = "请删除此行，实现上面的 TODO";
    // int data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    // Matrix2D m(data, 4, 3);
    // ASSERT_EQ(m(0, 0), 1);
    // ASSERT_EQ(m(0, 2), 3);
    // ASSERT_EQ(m(1, 0), 4);
    // ASSERT_EQ(m(3, 2), 12);
}

TEST("mdspan — extent 查询 (fallback)") {
    int _todo_ = "请删除此行，实现上面的 TODO";
    // int data[] = {1, 2, 3, 4, 5, 6};
    // Matrix2D m(data, 2, 3);
    // ASSERT_EQ(m.extent(0), 2u);
    // ASSERT_EQ(m.extent(1), 3u);
    // ASSERT_EQ(m.rank(), 2u);
}

TEST("mdspan — 静态 extent (fallback)") {
    int _todo_ = "请删除此行，实现上面的 TODO";
}

TEST("mdspan — 行遍历 (fallback)") {
    int _todo_ = "请删除此行，实现上面的 TODO";
}

TEST("mdspan — 列遍历 (fallback)") {
    int _todo_ = "请删除此行，实现上面的 TODO";
}

TEST("mdspan — 通过 mdspan 修改数据 (fallback)") {
    int _todo_ = "请删除此行，实现上面的 TODO";
}

#endif

CPPLINGS_MAIN
