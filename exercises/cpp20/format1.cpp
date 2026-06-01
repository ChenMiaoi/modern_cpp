// cpplings: format1
// Title: std::format 格式化
// Description: Use std::format for type-safe string formatting.
//   Format integers with padding, floats with precision, strings
//   with alignment, and combine multiple values.
//
// Instructions:
//   1. Format an integer with zero-padding.
//   2. Format a floating-point number with fixed precision.
//   3. Format a string with width and alignment.
//   4. Combine multiple values in one format call.
//   5. Delete each _todo_ guard after filling in the TODO block.
//
// Hint: std::format("{:05}", 42)    → "00042"
//       std::format("{:.2f}", 3.14) → "3.14"
//       std::format("{:>10}", "hi") → "        hi"

#include "cpplings.h"
#include <format>
#include <string>

// TODO: Implement pad_int — format an integer with zero-padding to width 5.
//   std::string pad_int(int n) {
//       return std::format("{:05}", n);
//   }

TEST("format — zero-padded integer") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // ASSERT_EQ(pad_int(42), "00042");
    // ASSERT_EQ(pad_int(0), "00000");
    // ASSERT_EQ(pad_int(12345), "12345");
}

// TODO: Implement format_float — format with 2 decimal places.
//   std::string format_float(double val) {
//       return std::format("{:.2f}", val);
//   }

TEST("format — float precision") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // ASSERT_EQ(format_float(3.14159), "3.14");
    // ASSERT_EQ(format_float(2.0), "2.00");
    // ASSERT_EQ(format_float(0.1 + 0.2), "0.30");
}

// TODO: Implement right_align — format string right-aligned in width 10.
//   std::string right_align(const std::string& s) {
//       return std::format("{:>10}", s);
//   }

TEST("format — string alignment") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // ASSERT_EQ(right_align("hi"), "        hi");
    // ASSERT_EQ(right_align("hello"), "     hello");
    // ASSERT_EQ(right_align("1234567890"), "1234567890");
}

// TODO: Implement format_pair — format a name and age as "name (age)".
//   std::string format_pair(const std::string& name, int age) {
//       return std::format("{} ({})", name, age);
//   }

TEST("format — combined values") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // ASSERT_EQ(format_pair("Alice", 30), "Alice (30)");
    // ASSERT_EQ(format_pair("Bob", 25), "Bob (25)");
}

CPPLINGS_MAIN
