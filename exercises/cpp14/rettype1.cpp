// Exercise: 返回类型推导 (Return Type Deduction)
// C++14 允许函数使用 auto 作为返回类型，编译器会自动推导。
// 对于递归函数，需要至少一个 return 语句在递归调用之前，以确定返回类型。
//
// 任务:
//   1. 实现 factorial 函数，使用 auto 返回类型
//   2. 实现 fibonacci 函数，使用 auto 返回类型
//   3. 实现 max_of_three 函数，使用 auto 返回类型
// 提示: 递归函数需要确保编译器能从某个 return 语句推导出返回类型

#include "cpplings.h"

TEST("factorial with auto return") {
    // TODO: 实现一个返回类型为 auto 的 factorial 函数
    auto factorial = [](auto n) -> auto {
        // TODO: 实现阶乘: factorial(5) = 120
        int _todo_ = "请删除此行，实现上面的 TODO";
        if (n <= 1) return 1;
        return n * factorial(n - 1);
    };
    ASSERT_EQ(factorial(0), 1);
    ASSERT_EQ(factorial(1), 1);
    ASSERT_EQ(factorial(5), 120);
    ASSERT_EQ(factorial(10), 3628800);
}

TEST("fibonacci with auto return") {
    auto fib = [](auto n) -> auto {
        // TODO: 实现 fibonacci 数列: fib(0)=0, fib(1)=1, fib(n)=fib(n-1)+fib(n-2)
        int _todo_ = "请删除此行，实现上面的 TODO";
        if (n <= 0) return 0;
        if (n == 1) return 1;
        return fib(n - 1) + fib(n - 2);
    };
    ASSERT_EQ(fib(0), 0);
    ASSERT_EQ(fib(1), 1);
    ASSERT_EQ(fib(6), 8);
    ASSERT_EQ(fib(10), 55);
}

TEST("auto return with conditionals") {
    // TODO: 实现 max_of_three，返回三个数中最大的，使用 auto 返回类型
    auto max_of_three = [](auto a, auto b, auto c) {
        int _todo_ = "请删除此行，实现上面的 TODO";
        auto m = a > b ? a : b;
        return m > c ? m : c;
    };
    ASSERT_EQ(max_of_three(1, 2, 3), 3);
    ASSERT_EQ(max_of_three(5, 3, 4), 5);
    ASSERT_EQ(max_of_three(1, 8, 3), 8);
    ASSERT_TRUE(max_of_three(1.5, 2.5, 0.5) == 2.5);
}

TEST("auto return type deduction") {
    // TODO: 实现一个函数，当输入是整数时返回整数，输入是浮点时返回浮点
    auto identity = [](auto x) {
        int _todo_ = "请删除此行，实现上面的 TODO";
        return x;
    };
    ASSERT_EQ(identity(42), 42);
    ASSERT_TRUE(identity(3.14) == 3.14);
    ASSERT_EQ(identity(100), 100);
}

CPPLINGS_MAIN
