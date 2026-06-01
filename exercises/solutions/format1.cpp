// Solution — format1: std::format 格式化
#include "cpplings.h"
#include <format>
#include <string>

std::string pad_int(int n) {
    return std::format("{:05}", n);
}

TEST("format — zero-padded integer") {
    ASSERT_EQ(pad_int(42), "00042");
    ASSERT_EQ(pad_int(0), "00000");
    ASSERT_EQ(pad_int(12345), "12345");
}

std::string format_float(double val) {
    return std::format("{:.2f}", val);
}

TEST("format — float precision") {
    ASSERT_EQ(format_float(3.14159), "3.14");
    ASSERT_EQ(format_float(2.0), "2.00");
    ASSERT_EQ(format_float(0.1 + 0.2), "0.30");
}

std::string right_align(const std::string& s) {
    return std::format("{:>10}", s);
}

TEST("format — string alignment") {
    ASSERT_EQ(right_align("hi"), "        hi");
    ASSERT_EQ(right_align("hello"), "     hello");
    ASSERT_EQ(right_align("1234567890"), "1234567890");
}

std::string format_pair(const std::string& name, int age) {
    return std::format("{} ({})", name, age);
}

TEST("format — combined values") {
    ASSERT_EQ(format_pair("Alice", 30), "Alice (30)");
    ASSERT_EQ(format_pair("Bob", 25), "Bob (25)");
}

CPPLINGS_MAIN
