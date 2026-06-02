// cpplings: perf2
// 主题: 性能优化 — 分支预测与 branchless 编程
//
// TODO: 实现 branchless 版本的 abs 和 clamp
//
// 提示: 分支预测失败代价高 (~15 cycles)
//       branchless 用位运算或条件移动替代分支

#include "cpplings.h"
#include <cstdlib>
#include <algorithm>

// TODO: branchless abs (不能用 if/三元运算符)
// 提示: 对于 int，利用补码: (x ^ (x >> 31)) - (x >> 31)
inline int branchless_abs(int x) {
    int _todo_ = "FILL IN THE TODO";
    return x;
}

// TODO: branchless clamp [lo, hi]
// 提示: std::max(lo, std::min(hi, x)) 通常编译为 cmov
inline int branchless_clamp(int x, int lo, int hi) {
    int _todo_ = "FILL IN THE TODO";
    return x;
}

// TODO: branchless conditional sum
// 对数组中 > threshold 的元素求和，不能用 if
// 提示: mask = -(x > threshold); sum += x & mask;
int conditional_sum(const int* arr, int n, int threshold) {
    int _todo_ = "FILL IN THE TODO";
    return 0;
}

TEST("branchless_abs 正数") {
    ASSERT_EQ(branchless_abs(5), 5);
}

TEST("branchless_abs 负数") {
    ASSERT_EQ(branchless_abs(-5), 5);
}

TEST("branchless_abs 零") {
    ASSERT_EQ(branchless_abs(0), 0);
}

TEST("branchless_clamp 在范围内") {
    ASSERT_EQ(branchless_clamp(5, 0, 10), 5);
}

TEST("branchless_clamp 低于下界") {
    ASSERT_EQ(branchless_clamp(-5, 0, 10), 0);
}

TEST("branchless_clamp 高于上界") {
    ASSERT_EQ(branchless_clamp(15, 0, 10), 10);
}

TEST("conditional_sum 基本功能") {
    int arr[] = {1, 5, 3, 8, 2, 7, 4, 6};
    int result = conditional_sum(arr, 8, 4);
    // > 4 的元素: 5, 8, 7, 6 = 26
    ASSERT_EQ(result, 26);
}

CPPLINGS_MAIN
