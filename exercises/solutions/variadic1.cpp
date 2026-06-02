// cpplings: variadic1 — solution
// 主题: C++11 模板与泛型 — 可变参数模板

#include "cpplings.h"
#include <sstream>
#include <string>
#include <type_traits>

// print_all: 递归展开参数包
void print_all(std::ostream& os) {
    os << "\n";
}

template <typename T>
void print_all(std::ostream& os, const T& last) {
    os << last << "\n";
}

template <typename T, typename U, typename... Args>
void print_all(std::ostream& os, const T& first, const U& second, const Args&... rest) {
    os << first << " ";
    print_all(os, second, rest...);
}

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

// sum: 递归展开实现参数求和
template <typename T>
T sum(const T& val) {
    return val;
}

template <typename T, typename U, typename... Args>
typename std::common_type<T, U, Args...>::type
sum(const T& first, const U& second, const Args&... rest) {
    return first + sum(second, rest...);
}

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

// count_args: constexpr 返回参数个数
template <typename... Args>
constexpr std::size_t count_args(const Args&...) {
    return sizeof...(Args);
}

TEST("count_args 统计参数个数") {
    static_assert(count_args(1, 2, 3) == 3, "3 个参数");
    static_assert(count_args("hello") == 1, "1 个参数");
    static_assert(count_args() == 0, "0 个参数");

    ASSERT_EQ(count_args(1, 2, 3), 3u);
    ASSERT_EQ(count_args("a", "b"), 2u);
    ASSERT_EQ(count_args(), 0u);
}

CPPLINGS_MAIN
