// cpplings: print23 — 解答
// 主题: C++23 — std::print / std::println 格式化输出

#include "cpplings.h"
#include <string>
#include <cstdio>

std::string format_int(int value) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", value);
    return std::string(buf);
}

std::string format_float(double value, int precision) {
    char buf[64];
    char fmt[16];
    std::snprintf(fmt, sizeof(fmt), "%%.%df", precision);
    std::snprintf(buf, sizeof(buf), fmt, value);
    return std::string(buf);
}

std::string format_string(const std::string& s) {
    return s;
}

std::string print_int_string(const char* name, int value) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s: %d", name, value);
    return std::string(buf);
}

std::string print_all(const std::string& s, int i, double d) {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "%s %d %.2f", s.c_str(), i, d);
    return std::string(buf);
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
