// cpplings: requires1
// Title: requires 表达式与约束
// Description: Use requires expressions to check type properties,
//   write compound and nested requirements, and constrain templates.
//
// Instructions:
//   1. Write a simple requires expression to check for addition.
//   2. Write a type requirement checking for a nested type.
//   3. Write a compound requirement checking expression validity and type.
//   4. Write a nested requirement using static_assert inside requires.
//   5. Delete each _todo_ guard after filling in the TODO block.
//
// Hint: requires (T a, T b) { a + b; } — simple requirement.
//       requires { typename T::value_type; } — type requirement.
//       requires (T a) { { a.size() } -> std::convertible_to<int>; } — compound.
//       requires (T a) { { a + a } -> std::same_as<T>; } — also compound.

#include "cpplings.h"
#include <concepts>
#include <string>
#include <vector>
#include <type_traits>

// TODO: Define Addable concept using a simple requires expression.
//   template <typename T>
//   concept Addable = requires(T a, T b) {
//       a + b;
//   };

TEST("simple requires — Addable") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // ASSERT_TRUE(Addable<int>);
    // ASSERT_TRUE(Addable<std::string>);
    // ASSERT_FALSE(Addable<std::vector<int>>);
}

// TODO: Define HasValueType concept — a type requirement.
//   template <typename T>
//   concept HasValueType = requires {
//       typename T::value_type;
//   };

TEST("type requirement — HasValueType") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // ASSERT_TRUE(HasValueType<std::vector<int>>);
    // ASSERT_TRUE(HasValueType<std::string>);
    // ASSERT_FALSE(HasValueType<int>);
}

// TODO: Define Sized concept — a compound requirement.
//   template <typename T>
//   concept Sized = requires(T t) {
//       { t.size() } -> std::convertible_to<std::size_t>;
//   };

TEST("compound requirement — Sized") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // ASSERT_TRUE(Sized<std::string>);
    // ASSERT_TRUE(Sized<std::vector<int>>);
    // ASSERT_FALSE(Sized<int>);
}

// TODO: Define SameSizeAddable — a nested requirement.
//   template <typename T>
//   concept SameSizeAddable = requires(T a, T b) {
//       { a + b } -> std::same_as<T>;
//   };

TEST("nested requirement — SameSizeAddable") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // ASSERT_TRUE(SameSizeAddable<int>);
    // ASSERT_TRUE(SameSizeAddable<std::string>);
    // ASSERT_FALSE(SameSizeAddable<short>);  // short + short promotes to int, not short
}

// TODO: Write a function constrained by requires clause inline.
//   auto describe_size(const auto& c) requires requires { c.size(); } {
//       return static_cast<int>(c.size());
//   }

TEST("inline requires clause") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // std::string s = "hello";
    // ASSERT_EQ(describe_size(s), 5);
    // std::vector<int> v = {1, 2, 3};
    // ASSERT_EQ(describe_size(v), 3);
}

CPPLINGS_MAIN
