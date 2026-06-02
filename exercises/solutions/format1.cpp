// Solution — format1: std::format 格式化
#include "cpplings.h"
#include <string>
#include <version>

#if __has_include(<format>)
#include <format>
#endif

#if defined(__cpp_lib_format) && __cpp_lib_format >= 201907L

std::string pad_int(int n) {
    return std::format("{:05}", n);
}

std::string format_float(double val) {
    return std::format("{:.2f}", val);
}

std::string right_align(const std::string& s) {
    return std::format("{:>10}", s);
}

std::string format_pair(const std::string& name, int age) {
    return std::format("{} ({})", name, age);
}

#else

#include <iomanip>
#include <sstream>

std::string pad_int(int n) {
    std::ostringstream out;
    out << std::setw(5) << std::setfill('0') << n;
    return out.str();
}

std::string format_float(double val) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << val;
    return out.str();
}

std::string right_align(const std::string& s) {
    std::ostringstream out;
    out << std::setw(10) << s;
    return out.str();
}

std::string format_pair(const std::string& name, int age) {
    std::ostringstream out;
    out << name << " (" << age << ")";
    return out.str();
}

#endif

TEST("format — zero-padded integer") {
    ASSERT_EQ(pad_int(42), "00042");
    ASSERT_EQ(pad_int(0), "00000");
    ASSERT_EQ(pad_int(12345), "12345");
}

TEST("format — float precision") {
    ASSERT_EQ(format_float(3.14159), "3.14");
    ASSERT_EQ(format_float(2.0), "2.00");
    ASSERT_EQ(format_float(0.1 + 0.2), "0.30");
}

TEST("format — string alignment") {
    ASSERT_EQ(right_align("hi"), "        hi");
    ASSERT_EQ(right_align("hello"), "     hello");
    ASSERT_EQ(right_align("1234567890"), "1234567890");
}

TEST("format — combined values") {
    ASSERT_EQ(format_pair("Alice", 30), "Alice (30)");
    ASSERT_EQ(format_pair("Bob", 25), "Bob (25)");
}

CPPLINGS_MAIN
