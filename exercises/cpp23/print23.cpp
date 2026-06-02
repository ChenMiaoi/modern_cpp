// cpplings: print23
// 主题: C++23 — std::print / std::println 格式化输出
//
// TODO: 使用 C++23 <print> 进行格式化输出
//   - std::print 到 stdout
//   - std::println 自动换行
//   - std::format 与 std::print 的关系
//   - 自定义 std::formatter<T>
//
// 提示: std::print("{} {}", a, b) 类似 Python print(f"{a} {b}")
//       如果编译器不支持 <print>，走 fallback 路径用 snprintf 模拟

#include "cpplings.h"
#include <version>
#include <string>

#if __has_include(<print>) && defined(__cpp_lib_print) && __cpp_lib_print >= 202207L
#include <print>
#include <format>
#include <cstdio>

// === 真实 std::print 路径 ===

// TODO: format_to_string — 使用 std::format / std::vformat 格式化为字符串
std::string format_to_string(const char* fmt, int value) {
    int _todo_ = "FILL IN THE TODO";
    return "";
}

// TODO: format_to_string_fp — 浮点数格式化
std::string format_to_string_fp(const char* fmt, double value) {
    int _todo_ = "FILL IN THE TODO";
    return "";
}

// TODO: demonstrate_print — 用 std::print 输出到 stdout
// 调用 std::print("Hello, {}! You are {} years old.\n", name, age);
void demonstrate_print(const std::string& name, int age) {
    int _todo_ = "FILL IN THE TODO";
}

// TODO: demonstrate_println — 使用 std::println
void demonstrate_println(const std::string& msg) {
    int _todo_ = "FILL IN THE TODO";
}

// === 自定义 formatter 示例 ===
struct Point {
    int x, y;
};

namespace std {
// TODO: 特化 std::formatter<Point> 格式化为 "(x, y)"
// 输出示例: Point{3, 4} => "(3, 4)"
template <>
struct formatter<Point> {
    constexpr auto parse(format_parse_context& ctx) {
        int _todo_ = "FILL IN THE TODO";
        return ctx.begin();
    }

    auto format(const Point& point, format_context& ctx) const {
        int _todo_ = "FILL IN THE TODO";
        return ctx.out();
    }
};
}  // namespace std

namespace {

std::string read_file_contents(std::FILE* file) {
    std::fflush(file);
    std::rewind(file);

    std::string result;
    char buffer[256];
    while (std::size_t n = std::fread(buffer, 1, sizeof(buffer), file)) {
        result.append(buffer, n);
    }
    return result;
}

template <typename Writer>
std::string capture_output(Writer writer) {
    std::FILE* file = std::tmpfile();
    if (file == nullptr) {
        return {};
    }

    writer(file);
    std::string output = read_file_contents(file);
    std::fclose(file);
    return output;
}

}  // namespace

TEST("std::format 格式化整数") {
    std::string s = std::format("{}", 42);
    ASSERT_EQ(s, "42");
}

TEST("std::format 格式化浮点") {
    std::string s = std::format("{:.2f}", 3.14159);
    ASSERT_EQ(s, "3.14");
}

TEST("std::format 多值格式化") {
    std::string s = std::format("Hello, {}! Age: {}", "Alice", 30);
    ASSERT_EQ(s, "Hello, Alice! Age: 30");
}

TEST("format_to_string 整数") {
    ASSERT_EQ(format_to_string("{}", 42), "42");
    ASSERT_EQ(format_to_string("{}", -7), "-7");
}

TEST("format_to_string_fp 浮点") {
    ASSERT_EQ(format_to_string_fp("{:.2f}", 3.14159), "3.14");
}

TEST("std::print 输出格式化内容") {
    std::string output = capture_output([](std::FILE* file) {
        std::print(file, "Hello, {}! You are {} years old.\n", "Alice", 30);
    });
    ASSERT_EQ(output, "Hello, Alice! You are 30 years old.\n");
}

TEST("std::println 自动追加换行") {
    std::string output = capture_output([](std::FILE* file) {
        std::println(file, "{}", "ready");
    });
    ASSERT_EQ(output, "ready\n");
}

TEST("std::format 返回字符串 vs std::print 输出") {
    std::string formatted = std::format("x={}, y={}", 10, 20);
    std::string printed = capture_output([](std::FILE* file) {
        std::print(file, "x={}, y={}", 10, 20);
    });
    ASSERT_EQ(formatted, "x=10, y=20");
    ASSERT_EQ(printed, formatted);
}

TEST("自定义 std::formatter<Point>") {
    Point p{3, 4};
    ASSERT_EQ(std::format("{}", p), "(3, 4)");
}

TEST("std::print 支持自定义 formatter") {
    Point p{3, 4};
    std::string output = capture_output([&](std::FILE* file) {
        std::print(file, "{}", p);
    });
    ASSERT_EQ(output, "(3, 4)");
}

#else  // Fallback: no <print> support

#include <cstdio>

// === Fallback 路径 ===
// 注意: 以下使用 snprintf 模拟，不是真正的 std::print
// 标准 std::print 直接输出到 stdout / FILE*，这里为了可验证性返回字符串

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

TEST("format_int 格式化正整数 (fallback)") {
    ASSERT_EQ(format_int(42), "42");
}

TEST("format_int 格式化负整数 (fallback)") {
    ASSERT_EQ(format_int(-7), "-7");
}

TEST("format_int 格式化零 (fallback)") {
    ASSERT_EQ(format_int(0), "0");
}

TEST("format_float 指定精度 (fallback)") {
    ASSERT_EQ(format_float(3.14159, 2), "3.14");
}

TEST("format_float 整数精度 (fallback)") {
    ASSERT_EQ(format_float(2.0, 0), "2");
}

TEST("format_string 原样返回 (fallback)") {
    ASSERT_EQ(format_string("hello"), "hello");
}

TEST("print_int_string 组合输出 (fallback)") {
    ASSERT_EQ(print_int_string("age", 25), "age: 25");
}

TEST("print_all 多值格式化 (fallback)") {
    std::string result = print_all("Alice", 30, 3.14);
    ASSERT_TRUE(result.find("Alice") != std::string::npos);
    ASSERT_TRUE(result.find("30") != std::string::npos);
}

#endif

CPPLINGS_MAIN
