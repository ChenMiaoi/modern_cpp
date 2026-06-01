// cpplings: concepts1
// Title: C++20 Concepts
// Description: Define named concepts and use them to constrain function
//   templates via requires-clauses and shorthand syntax.
//
// Instructions:
//   1. Define a Printable concept (operator<< to ostream).
//   2. Define a Numeric concept (arithmetic but not bool).
//   3. Define a Container concept (begin/end/size).
//   4. Write a contains() function constrained with requires.
//   5. Use abbreviated function template syntax (auto params).
//   6. Delete each _todo_ guard after filling in the TODO block.
//
// Hint: concept Name = requires(T t) { { expr } -> constraint; };

#include "cpplings.h"
#include <concepts>
#include <sstream>
#include <string>
#include <vector>
#include <type_traits>
#include <stdexcept>

// TODO: Define Printable — T must be streamable to std::ostream.
//   template <typename T>
//   concept Printable = requires(std::ostream& os, T val) { ... };

TEST("Printable — accepted types") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // Once Printable is defined, uncomment:
    // template <Printable T> std::string to_string_rep(const T& v) { ... }
    // ASSERT_EQ(to_string_rep(42), "42");
    // ASSERT_EQ(to_string_rep(3.14), "3.14");
    // ASSERT_EQ(to_string_rep(std::string("hello")), "hello");
}

// TODO: Define Numeric — arithmetic type, excluding bool.
//   template <typename T>
//   concept Numeric = std::is_arithmetic_v<T> && !std::is_same_v<T, bool>;

TEST("Numeric — accepted types") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // Once Numeric is defined, uncomment:
    // template <Numeric T> T clamp_value(T val, T lo, T hi) { ... }
    // ASSERT_EQ(clamp_value(5, 1, 10), 5);
    // ASSERT_EQ(clamp_value(-1, 0, 100), 0);
    // ASSERT_EQ(clamp_value(150, 0, 100), 100);
    // ASSERT_EQ(clamp_value(3.14, 0.0, 10.0), 3.14);
}

// TODO: Define Container — has begin(), end(), and size().
//   template <typename T>
//   concept Container = requires(T t) { ... };

TEST("Container — accepted types") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // Once Container is defined, uncomment:
    // template <Container C> auto first_or_default(const C& c) -> decltype(*c.begin()) { ... }
    // std::vector<int> v = {10, 20, 30};
    // ASSERT_EQ(first_or_default(v), 10);
    // std::string s = "hello";
    // ASSERT_EQ(first_or_default(s), 'h');
}

// TODO: Write contains() using a requires-clause (not a named concept).
//   template <typename T>
//   requires std::equality_comparable<T> && std::copyable<T>
//   bool contains(const std::vector<T>& vec, const T& val) { ... }

TEST("requires clause") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // std::vector<int> nums = {1, 2, 3, 4, 5};
    // ASSERT_TRUE(contains(nums, 3));
    // ASSERT_FALSE(contains(nums, 99));
}

// TODO: Use abbreviated template syntax with auto.
//   void process_number(std::integral auto val) { (void)val; }

TEST("abbreviated syntax") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // process_number(42);
    // process_number(3L);
    // ASSERT_TRUE(true);
}

CPPLINGS_MAIN
