// cpplings: ifconstexpr1
// Title: if constexpr
// Description: Use if constexpr to perform compile-time type dispatch,
//   constrain template instantiation, and terminate recursive expansion.
//
// Instructions:
//   1. Implement serialize() — dispatch on T at compile time.
//   2. Implement get_size_or_zero() — return size() if available, else 0.
//   3. Implement sum_all() — variadic pack fold with constexpr termination.
//   4. Delete each _todo_ guard after filling in the TODO block.
//
// Hint: if constexpr (condition) { … } else { … } discards the untaken
//       branch at compile time — SFINAE-like without overload sets.

#include "cpplings.h"
#include <string>
#include <sstream>
#include <type_traits>
#include <vector>

// TODO: Return a type-tagged string representation.
//   int     → "int:<value>"          (e.g. "int:42")
//   float   → "float:<value>"        (e.g. "float:3.140000")
//   string  → "str:\"<value>\""      (e.g. "str:\"hello\"")
//   bool    → "bool:true" or "bool:false"
//   other   → "unknown"
template <typename T>
std::string serialize(const T& value) {
    int _todo_ = "FILL IN"; (void)_todo_;
    // Hint: use std::is_same_v<T, bool>, std::is_integral_v<T>,
    //       std::is_floating_point_v<T>, std::is_same_v<T, std::string>
    //       WARNING: bool is integral — check it FIRST!
    return "";  // replace this
}

TEST("serialize — int") {
    ASSERT_EQ(serialize(42), "int:42");
    ASSERT_EQ(serialize(-1), "int:-1");
    ASSERT_EQ(serialize(0), "int:0");
}

TEST("serialize — float") {
    auto result = serialize(3.14);
    ASSERT_TRUE(result.find("float:") == 0);
}

TEST("serialize — string") {
    ASSERT_EQ(serialize(std::string("hello")), "str:\"hello\"");
}

TEST("serialize — bool") {
    ASSERT_EQ(serialize(true), "bool:true");
    ASSERT_EQ(serialize(false), "bool:false");
}

// TODO: Return c.size() if the type supports .size(), otherwise return 0.
//   Hint: use if constexpr with a SFINAE type trait (std::void_t + decltype)
//         to detect whether .size() exists at compile time.
template <typename Container>
std::size_t get_size_or_zero(const Container& c) {
    int _todo_ = "FILL IN"; (void)_todo_;
    return 0;  // replace this
}

TEST("get_size_or_zero — with size()") {
    std::vector<int> v = {1, 2, 3};
    ASSERT_EQ(get_size_or_zero(v), 3u);
}

TEST("get_size_or_zero — without size()") {
    int x = 42;
    ASSERT_EQ(get_size_or_zero(x), 0u);
}

// TODO: Sum all arguments. Use if constexpr to stop recursive expansion.
//   Base case: single argument → return it.
//   Recursive: sizeof...(rest) == 0 → return first; else → first + sum_all(rest...)
template <typename T>
T sum_all(T val) {
    int _todo_ = "FILL IN"; (void)_todo_;
    return val;  // replace this — but the _todo_ line must go
}

template <typename T, typename... Rest>
auto sum_all(T first, Rest... rest) {
    int _todo_ = "FILL IN"; (void)_todo_;
    // if constexpr (sizeof...(rest) == 0) { return first; }
    // else { return first + sum_all(rest...); }
    return first;  // replace this
}

TEST("sum_all") {
    ASSERT_EQ(sum_all(1, 2, 3), 6);
    ASSERT_EQ(sum_all(100), 100);
    ASSERT_EQ(sum_all(1, 2, 3, 4, 5), 15);
}

CPPLINGS_MAIN
