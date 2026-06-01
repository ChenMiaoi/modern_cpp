// Solution: 返回类型推导 (Return Type Deduction)

#include "cpplings.h"

TEST("factorial with auto return") {
    auto factorial = [](auto n) -> auto {
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
    auto max_of_three = [](auto a, auto b, auto c) {
        auto m = a > b ? a : b;
        return m > c ? m : c;
    };
    ASSERT_EQ(max_of_three(1, 2, 3), 3);
    ASSERT_EQ(max_of_three(5, 3, 4), 5);
    ASSERT_EQ(max_of_three(1, 8, 3), 8);
    ASSERT_TRUE(max_of_three(1.5, 2.5, 0.5) == 2.5);
}

TEST("auto return type deduction") {
    auto identity = [](auto x) {
        return x;
    };
    ASSERT_EQ(identity(42), 42);
    ASSERT_TRUE(identity(3.14) == 3.14);
    ASSERT_EQ(identity(100), 100);
}

CPPLINGS_MAIN
