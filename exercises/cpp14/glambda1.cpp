// Exercise: 泛型 Lambda (Generic Lambdas)
// C++14 允许 lambda 的参数使用 auto，使其成为泛型 lambda。
// 这使得一个 lambda 可以处理多种类型的参数。
//
// 任务:
//   1. 实现一个泛型 lambda add，能够对 int、double 等类型做加法
//   2. 实现一个泛型 lambda multiply，能够对不同类型做乘法
//   3. 实现一个泛型 lambda compare，比较两个值是否相等
// 提示: auto 参数让 lambda 自动推导参数类型

#include "cpplings.h"

TEST("generic add with int") {
    auto add = [](auto a, auto b) {
        // TODO: 返回 a + b
        int _todo_ = "请删除此行，实现上面的 TODO";
        return a + b;
    };
    ASSERT_EQ(add(3, 4), 7);
    ASSERT_EQ(add(10, 20), 30);
}

TEST("generic add with double") {
    auto add = [](auto a, auto b) {
        int _todo_ = "请删除此行，实现上面的 TODO";
        return a + b;
    };
    ASSERT_TRUE(add(1.5, 2.5) == 4.0);
    ASSERT_TRUE(add(0.1, 0.2) > 0.29);
}

TEST("generic multiply") {
    auto multiply = [](auto a, auto b) {
        // TODO: 返回 a * b
        int _todo_ = "请删除此行，实现上面的 TODO";
        return a * b;
    };
    ASSERT_EQ(multiply(3, 4), 12);
    ASSERT_EQ(multiply(2.5, 4.0), 10.0);
    ASSERT_EQ(multiply(6, 7), 42);
}

TEST("generic compare") {
    auto equal = [](auto a, auto b) {
        // TODO: 返回 a 是否等于 b
        int _todo_ = "请删除此行，实现上面的 TODO";
        return a == b;
    };
    ASSERT_TRUE(equal(42, 42));
    ASSERT_FALSE(equal(42, 43));
    ASSERT_TRUE(equal(3.14, 3.14));
    ASSERT_FALSE(equal(3.14, 2.71));
}

CPPLINGS_MAIN
