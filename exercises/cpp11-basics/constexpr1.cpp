// Exercise: constexpr 基础
// 实现 constexpr 函数，使其在编译期求值
//
// 任务:
//   1. 实现 constexpr 阶乘函数 factorial
//   2. 实现 constexpr Fibonacci 函数 fibonacci
//   3. 实现 constexpr 求幂函数 power
//
// 提示: C++11 要求 constexpr 函数体只能有一条 return 语句。
//       使用三元运算符 ? : 来实现条件分支。
//       递归调用是安全的——编译器会在编译期展开。

#include "cpplings.h"

// TODO: 实现 constexpr 阶乘函数
// factorial(0) = 1, factorial(5) = 120
// 使用三元运算符: return (n <= 1) ? 1 : n * factorial(n - 1);
constexpr int factorial(int n) {
    int _todo_ = "请删除此行，实现 factorial";  // 编译错误
    return _todo_;
}

// TODO: 实现 constexpr Fibonacci 函数
// fibonacci(0) = 0, fibonacci(1) = 1, fibonacci(10) = 55
// 使用嵌套三元运算符: return (n <= 0) ? 0 : (n == 1) ? 1 : fibonacci(n-1) + fibonacci(n-2);
constexpr int fibonacci(int n) {
    int _todo2_ = "请删除此行，实现 fibonacci";  // 编译错误
    return _todo2_;
}

// TODO: 实现 constexpr 求幂函数
// power(2, 0) = 1, power(2, 10) = 1024
// 使用三元运算符: return (exp == 0) ? 1 : base * power(base, exp - 1);
constexpr int power(int base, int exp) {
    int _todo3_ = "请删除此行，实现 power";  // 编译错误
    return _todo3_;
}

TEST("factorial 在编译期求值") {
    // 这些结果必须在编译期确定
    static_assert(factorial(0) == 1, "0! = 1");
    static_assert(factorial(1) == 1, "1! = 1");
    static_assert(factorial(5) == 120, "5! = 120");
    static_assert(factorial(10) == 3628800, "10! = 3628800");

    ASSERT_EQ(factorial(0), 1);
    ASSERT_EQ(factorial(5), 120);
    ASSERT_EQ(factorial(10), 3628800);
}

TEST("fibonacci 在编译期求值") {
    static_assert(fibonacci(0) == 0, "fib(0) = 0");
    static_assert(fibonacci(1) == 1, "fib(1) = 1");
    static_assert(fibonacci(10) == 55, "fib(10) = 55");

    ASSERT_EQ(fibonacci(0), 0);
    ASSERT_EQ(fibonacci(1), 1);
    ASSERT_EQ(fibonacci(10), 55);
    ASSERT_EQ(fibonacci(20), 6765);
}

TEST("power 在编译期求值") {
    static_assert(power(2, 0) == 1, "2^0 = 1");
    static_assert(power(2, 10) == 1024, "2^10 = 1024");
    static_assert(power(3, 4) == 81, "3^4 = 81");

    ASSERT_EQ(power(2, 0), 1);
    ASSERT_EQ(power(2, 10), 1024);
    ASSERT_EQ(power(3, 4), 81);
}

TEST("constexpr 用于数组大小") {
    constexpr int sz = factorial(3);  // 6
    int arr[sz] = {10, 20, 30, 40, 50, 60};
    ASSERT_EQ(arr[0], 10);
    ASSERT_EQ(arr[5], 60);
}

CPPLINGS_MAIN
