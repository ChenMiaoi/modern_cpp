// Exercise: 变量模板 (Variable Templates)
// C++14 允许定义变量模板，即带模板参数的变量。
// 例如: template<typename T> constexpr T pi = T(3.1415926535897932385L);
//
// 任务:
//   1. 定义一个 constexpr 变量模板 pi<T>
//   2. 定义一个变量模板 e<T> (自然常数)
//   3. 用 float 和 double 特化来测试精度
// 提示: 变量模板可以像普通变量一样使用，但带上模板参数

#include "cpplings.h"

// TODO: 定义变量模板 pi
// template<typename T> constexpr T pi = T(3.1415926535897932385L);

// TODO: 定义变量模板 e (自然常数 ≈ 2.71828182845904523536)
// template<typename T> constexpr T e = T(2.71828182845904523536L);

TEST("variable template pi with double") {
    int _todo_ = "请删除此行，实现上面的 TODO";
    // TODO: 用 double 实例化 pi，断言 pi<double> 大于 3.14 且小于 3.15
    ASSERT_TRUE(pi<double> > 3.14);
    ASSERT_TRUE(pi<double> < 3.15);
}

TEST("variable template pi with float") {
    int _todo_ = "请删除此行，实现上面的 TODO";
    // TODO: 用 float 实例化 pi，断言 pi<float> 大于 3.14f 且小于 3.15f
    // 注意 float 精度较低
    ASSERT_TRUE(pi<float> > 3.14f);
    ASSERT_TRUE(pi<float> < 3.15f);
}

TEST("variable template e with double") {
    int _todo_ = "请删除此行，实现上面的 TODO";
    // TODO: 用 double 实例化 e，断言 e<double> 大于 2.71 且小于 2.72
    ASSERT_TRUE(e<double> > 2.71);
    ASSERT_TRUE(e<double> < 2.72);
}

TEST("variable template e with float") {
    int _todo_ = "请删除此行，实现上面的 TODO";
    // TODO: 用 float 实例化 e
    ASSERT_TRUE(e<float> > 2.71f);
    ASSERT_TRUE(e<float> < 2.72f);
}

TEST("variable template used in expression") {
    int _todo_ = "请删除此行，实现上面的 TODO";
    // TODO: 使用变量模板计算圆面积: area = pi<double> * r * r, r = 2.0
    double area = pi<double> * 2.0 * 2.0;
    ASSERT_TRUE(area > 12.56);
    ASSERT_TRUE(area < 12.57);
}

CPPLINGS_MAIN
