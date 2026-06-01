// cpplings: lambda1 — solution
// 主题: C++11 模板与泛型 — Lambda 表达式

#include "cpplings.h"
#include <vector>
#include <algorithm>
#include <numeric>
#include <functional>
#include <string>

TEST("基本 lambda 语法") {
    auto add = [](int a, int b) { return a + b; };

    ASSERT_EQ(add(3, 4), 7);
    ASSERT_EQ(add(-1, 1), 0);
}

TEST("lambda 值捕获") {
    int factor = 10;

    auto multiply = [factor](int x) { return x * factor; };

    ASSERT_EQ(multiply(5), 50);

    factor = 100;
    ASSERT_EQ(multiply(5), 50);
}

TEST("lambda 引用捕获") {
    int counter = 0;

    auto increment = [&counter]() { ++counter; };

    increment();
    increment();
    increment();
    ASSERT_EQ(counter, 3);
}

TEST("lambda 用于排序") {
    std::vector<int> nums = {5, 2, 8, 1, 9, 3};

    std::sort(nums.begin(), nums.end(), [](int a, int b) { return a > b; });

    ASSERT_EQ(nums[0], 9);
    ASSERT_EQ(nums[1], 8);
    ASSERT_EQ(nums[5], 1);
}

TEST("lambda 用于过滤") {
    std::vector<int> nums = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    int even_count = std::count_if(nums.begin(), nums.end(),
        [](int n) { return n % 2 == 0; });

    ASSERT_EQ(even_count, 5);
}

TEST("lambda 用于累加") {
    std::vector<int> nums = {1, 2, 3, 4, 5};

    int sq_sum = std::accumulate(nums.begin(), nums.end(), 0,
        [](int acc, int n) { return acc + n * n; });

    ASSERT_EQ(sq_sum, 55);
}

TEST("泛型 lambda（C++14）") {
    auto generic_add = [](auto a, auto b) { return a + b; };

    ASSERT_EQ(generic_add(3, 4), 7);
    ASSERT_EQ(generic_add(1.5, 2.5), 4.0);
}

TEST("lambda 返回 lambda") {
    auto make_adder = [](int base) {
        return [base](int x) { return base + x; };
    };

    auto add5 = make_adder(5);
    auto add10 = make_adder(10);

    ASSERT_EQ(add5(3), 8);
    ASSERT_EQ(add10(3), 13);
}

CPPLINGS_MAIN
