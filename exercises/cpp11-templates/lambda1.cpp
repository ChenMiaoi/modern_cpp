// cpplings: lambda1
// 主题: C++11 模板与泛型 — Lambda 表达式
//
// 练习：使用 lambda 表达式完成各种任务
// 删除每个 TODO 区域的 _todo_ 行，实现所需的 lambda
// 让所有断言通过

#include "cpplings.h"
#include <vector>
#include <algorithm>
#include <numeric>
#include <functional>
#include <string>

TEST("基本 lambda 语法") {
    // TODO: 创建一个 lambda，接受两个 int 参数并返回它们的和
    // 提示: auto add = [](int a, int b) { return ???; };
    int _todo_ = "FILL IN";

    ASSERT_EQ(add(3, 4), 7);
    ASSERT_EQ(add(-1, 1), 0);
}

TEST("lambda 值捕获") {
    int factor = 10;

    // TODO: 创建一个 lambda，值捕获 factor，接受一个 int 参数 x，返回 x * factor
    // 提示: 值捕获的值在 lambda 创建时拷贝，不会随外部变量改变
    int _todo_ = "FILL IN";

    ASSERT_EQ(multiply(5), 50);

    factor = 100;  // 修改后，lambda 内的值不受影响
    ASSERT_EQ(multiply(5), 50);
}

TEST("lambda 引用捕获") {
    int counter = 0;

    // TODO: 创建一个 lambda，引用捕获 counter 并递增它
    // 提示: [&counter]() { ???; };
    int _todo_ = "FILL IN";

    increment();
    increment();
    increment();
    ASSERT_EQ(counter, 3);
}

TEST("lambda 用于排序") {
    std::vector<int> nums = {5, 2, 8, 1, 9, 3};

    // TODO: 使用 std::sort 和 lambda 按降序排序
    // 提示: std::sort(nums.begin(), nums.end(), [](int a, int b) { return ???; });
    int _todo_ = "FILL IN";

    ASSERT_EQ(nums[0], 9);
    ASSERT_EQ(nums[1], 8);
    ASSERT_EQ(nums[5], 1);
}

TEST("lambda 用于过滤") {
    std::vector<int> nums = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // TODO: 使用 std::count_if 和 lambda 统计偶数个数
    // 将结果存储在 even_count 中
    int _todo_ = "FILL IN";

    ASSERT_EQ(even_count, 5);
}

TEST("lambda 用于累加") {
    std::vector<int> nums = {1, 2, 3, 4, 5};

    // TODO: 使用 std::accumulate 和 lambda 计算平方和 (1+4+9+16+25 = 55)
    // 将结果存储在 sq_sum 中
    int _todo_ = "FILL IN";

    ASSERT_EQ(sq_sum, 55);
}

TEST("泛型 lambda（C++14）") {
    // TODO: 创建一个泛型 lambda（使用 auto 参数）
    // 使它能对 int 和 double 都做加法
    int _todo_ = "FILL IN";

    ASSERT_EQ(generic_add(3, 4), 7);
    ASSERT_EQ(generic_add(1.5, 2.5), 4.0);
}

TEST("lambda 返回 lambda") {
    // TODO: 创建一个加法器工厂 make_adder
    // make_adder(base) 返回一个 lambda，该 lambda 接受 x 并返回 base + x
    int _todo_ = "FILL IN";

    auto add5 = make_adder(5);
    auto add10 = make_adder(10);

    ASSERT_EQ(add5(3), 8);
    ASSERT_EQ(add10(3), 13);
}

CPPLINGS_MAIN
