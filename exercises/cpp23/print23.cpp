// cpplings: print23
// 主题: C++23 — std::print / std::println 格式化输出
//
// TODO: 实现 C++23 风格的 print/println 函数
// 使用 snprintf 实现格式化，返回格式化后的字符串
//
// 提示: std::print 类似 Python 的 print + format，
//       这里我们用字符串返回来验证输出内容

#include "cpplings.h"
#include <string>
#include <cstdio>
#include <cstdarg>

// C++23 <print> 不可用时的简易替代
// 实现 format_int, format_float, format_string 来格式化各类型

// TODO: format_int — 将整数格式化为字符串
std::string format_int(int value) {
    int _todo_ = "FILL IN THE TODO";
    return "";
}

// TODO: format_float — 将浮点数格式化为指定小数位的字符串
std::string format_float(double value, int precision) {
    int _todo_ = "FILL IN THE TODO";
    return "";
}

// TODO: format_string — 将字符串原样返回（演示多态接口）
std::string format_string(const std::string& s) {
    int _todo_ = "FILL IN THE TODO";
    return "";
}

// TODO: print_int_string — 格式化 "name: value" 形式
// 例如 print_int_string("age", 25) => "age: 25"
std::string print_int_string(const char* name, int value) {
    int _todo_ = "FILL IN THE TODO";
    return "";
}

// TODO: print_all — 组合多个值的格式化输出
// 例如 print_all("Alice", 30, 3.14) => "Alice 30 3.14"
std::string print_all(const std::string& s, int i, double d) {
    int _todo_ = "FILL IN THE TODO";
    return "";
}

TEST("format_int 格式化正整数") {
    ASSERT_EQ(format_int(42), "42");
}

TEST("format_int 格式化负整数") {
    ASSERT_EQ(format_int(-7), "-7");
}

TEST("format_int 格式化零") {
    ASSERT_EQ(format_int(0), "0");
}

TEST("format_float 指定精度") {
    ASSERT_EQ(format_float(3.14159, 2), "3.14");
}

TEST("format_float 整数精度") {
    ASSERT_EQ(format_float(2.0, 0), "2");
}

TEST("format_string 原样返回") {
    ASSERT_EQ(format_string("hello"), "hello");
}

TEST("print_int_string 组合输出") {
    ASSERT_EQ(print_int_string("age", 25), "age: 25");
}

TEST("print_all 多值格式化") {
    std::string result = print_all("Alice", 30, 3.14);
    ASSERT_TRUE(result.find("Alice") != std::string::npos);
    ASSERT_TRUE(result.find("30") != std::string::npos);
}

CPPLINGS_MAIN
