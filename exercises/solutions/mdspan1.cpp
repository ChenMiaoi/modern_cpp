// Solution — mdspan1: 多维数组视图 (std::mdspan)
#include "cpplings.h"

#include <version>

#if __has_include(<mdspan>)
  #include <mdspan>
#endif

#if defined(__cpp_lib_mdspan) && __cpp_lib_mdspan >= 202207L
#include <array>
#include <cstddef>

TEST("mdspan — 二维元素访问 (row-major)") {
    std::array<int, 12> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    std::mdspan m(data.data(), 4, 3);
    ASSERT_EQ(m(0, 0), 1);
    ASSERT_EQ(m(0, 2), 3);
    ASSERT_EQ(m(1, 0), 4);
    ASSERT_EQ(m(3, 2), 12);
}

TEST("mdspan — extent 查询") {
    std::array<int, 6> data = {1, 2, 3, 4, 5, 6};
    std::mdspan m(data.data(), 2, 3);
    ASSERT_EQ(m.extent(0), 2u);
    ASSERT_EQ(m.extent(1), 3u);
    ASSERT_EQ(m.rank(), 2u);
}

TEST("mdspan — 静态 extent") {
    std::array<int, 6> data = {1, 2, 3, 4, 5, 6};
    std::mdspan<int, std::extents<size_t, 2, 3>> m(data.data());
    ASSERT_EQ(m.extent(0), 2u);
    ASSERT_EQ(m(1, 2), 6);
}

TEST("mdspan — 行遍历 (通过指针算术)") {
    std::array<int, 12> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    std::mdspan m(data.data(), 4, 3);
    // Row 1: elements [4, 5, 6]
    std::mdspan row1(&m(1, 0), 3);
    ASSERT_EQ(row1(0), 4);
    ASSERT_EQ(row1(1), 5);
    ASSERT_EQ(row1(2), 6);
}

TEST("mdspan — 列遍历 (layout_stride)") {
    std::array<int, 12> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    std::mdspan m(data.data(), 4, 3);
    // Column 1: elements [2, 5, 8, 11]
    std::extents<size_t, std::dynamic_extent> col_ext(4);
    std::array<size_t, 1> strides = {3};
    std::mdspan<int, decltype(col_ext), std::layout_stride> col(
        &m(0, 1), col_ext, strides);
    ASSERT_EQ(col(0), 2);
    ASSERT_EQ(col(1), 5);
    ASSERT_EQ(col(2), 8);
    ASSERT_EQ(col(3), 11);
}

TEST("mdspan — 通过 mdspan 修改数据") {
    std::array<int, 6> data = {1, 2, 3, 4, 5, 6};
    std::mdspan m(data.data(), 2, 3);
    m(1, 1) = 99;
    ASSERT_EQ(data[4], 99);  // row 1, col 1 → flat index 4
}

#else
// Fallback: simple Matrix2D wrapper

#include <vector>
#include <cstddef>

struct Matrix2D {
    int* data_;
    std::size_t rows_;
    std::size_t cols_;

    Matrix2D(int* data, std::size_t rows, std::size_t cols)
        : data_(data), rows_(rows), cols_(cols) {}

    int& operator()(std::size_t r, std::size_t c) {
        return data_[r * cols_ + c];
    }
    const int& operator()(std::size_t r, std::size_t c) const {
        return data_[r * cols_ + c];
    }

    std::size_t extent(int n) const {
        return n == 0 ? rows_ : cols_;
    }

    static constexpr int rank() { return 2; }
    static constexpr std::size_t stride(int) { return 1; }
};

TEST("mdspan — 二维元素访问 (row-major) (fallback)") {
    int data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    Matrix2D m(data, 4, 3);
    ASSERT_EQ(m(0, 0), 1);
    ASSERT_EQ(m(0, 2), 3);
    ASSERT_EQ(m(1, 0), 4);
    ASSERT_EQ(m(3, 2), 12);
}

TEST("mdspan — extent 查询 (fallback)") {
    int data[] = {1, 2, 3, 4, 5, 6};
    Matrix2D m(data, 2, 3);
    ASSERT_EQ(m.extent(0), 2u);
    ASSERT_EQ(m.extent(1), 3u);
    ASSERT_EQ(m.rank(), 2);
}

TEST("mdspan — 静态 extent (fallback)") {
    // Fallback uses dynamic sizing; test that static-like usage works
    int data[] = {1, 2, 3, 4, 5, 6};
    Matrix2D m(data, 2, 3);
    ASSERT_EQ(m(1, 2), 6);
}

TEST("mdspan — 行遍历 (fallback)") {
    int data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    Matrix2D m(data, 4, 3);
    // Row 1: [4, 5, 6]
    int row1[] = {m(1, 0), m(1, 1), m(1, 2)};
    ASSERT_EQ(row1[0], 4);
    ASSERT_EQ(row1[1], 5);
    ASSERT_EQ(row1[2], 6);
}

TEST("mdspan — 列遍历 (fallback)") {
    int data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    Matrix2D m(data, 4, 3);
    // Column 1: [2, 5, 8, 11]
    int col1[] = {m(0, 1), m(1, 1), m(2, 1), m(3, 1)};
    ASSERT_EQ(col1[0], 2);
    ASSERT_EQ(col1[1], 5);
    ASSERT_EQ(col1[2], 8);
    ASSERT_EQ(col1[3], 11);
}

TEST("mdspan — 通过 mdspan 修改数据 (fallback)") {
    int data[] = {1, 2, 3, 4, 5, 6};
    Matrix2D m(data, 2, 3);
    m(1, 1) = 99;
    ASSERT_EQ(data[4], 99);  // row 1, col 1 → flat index 4
}

#endif

CPPLINGS_MAIN
