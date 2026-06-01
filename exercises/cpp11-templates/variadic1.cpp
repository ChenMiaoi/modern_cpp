// cpplings: variadic1
// 主题: C++11 模板与泛型 — 可变参数模板
//
// 练习：实现可变参数模板函数
// 删除每个 TODO 区域的 _todo_ 行，实现函数
// 让所有断言通过

#include "cpplings.h"
#include <sstream>
#include <string>

// TODO: 实现 print_all — 递归展开参数包
// 需要两个重载：
//   1. 终止版本：无额外参数，只输出换行
//   2. 递归版本：输出第一个参数（后跟空格，如果还有后续参数），然后递归处理剩余参数
// 提示: 使用 sizeof...(rest) 判断是否还有后续参数
int _todo_ = "FILL IN";

TEST("print_all 打印单个值") {
    std::ostringstream os;
    print_all(os, 42);
    ASSERT_EQ(os.str(), "42\n");
}

TEST("print_all 打印多个值") {
    std::ostringstream os;
    print_all(os, "hello", 123, 3.14);
    ASSERT_EQ(os.str(), "hello 123 3.14\n");
}

TEST("print_all 空参数列表") {
    std::ostringstream os;
    print_all(os);
    ASSERT_EQ(os.str(), "\n");
}

// TODO: 实现 sum — 计算任意数量参数的和
// 提示: 用递归展开（一个参数时返回自身，多个时 first + sum(rest...)）
//       或者用 C++17 折叠表达式: (args + ...)
int _todo_ = "FILL IN";

TEST("sum 计算整数和") {
    ASSERT_EQ(sum(1, 2, 3), 6);
    ASSERT_EQ(sum(10, 20, 30, 40), 100);
}

TEST("sum 单个参数") {
    ASSERT_EQ(sum(42), 42);
}

TEST("sum 浮点数") {
    double result = sum(1.5, 2.5, 3.0);
    ASSERT_EQ(result, 7.0);
}

// TODO: 实现 count_args — 返回参数个数
// 使用 sizeof...(Args) 或 sizeof...(args)
int _todo_ = "FILL IN";

TEST("count_args 统计参数个数") {
    static_assert(count_args(1, 2, 3) == 3, "3 个参数");
    static_assert(count_args("hello") == 1, "1 个参数");
    static_assert(count_args() == 0, "0 个参数");

    ASSERT_EQ(count_args(1, 2, 3), 3u);
    ASSERT_EQ(count_args("a", "b"), 2u);
    ASSERT_EQ(count_args(), 0u);
}

CPPLINGS_MAIN
