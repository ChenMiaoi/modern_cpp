// Solution: C++20 Concepts

#include "cpplings.h"
#include <concepts>
#include <sstream>
#include <string>
#include <vector>
#include <type_traits>
#include <stdexcept>

template <typename T>
concept Printable = requires(std::ostream& os, T val) {
    { os << val } -> std::same_as<std::ostream&>;
};

template <typename T>
concept Numeric = std::is_arithmetic_v<T> && !std::is_same_v<T, bool>;

template <typename T>
concept Container = requires(T t) {
    { t.begin() };
    { t.end() };
    { t.size() } -> std::convertible_to<std::size_t>;
};

template <typename T>
requires std::equality_comparable<T> && std::copyable<T>
bool contains(const std::vector<T>& vec, const T& val) {
    for (const auto& elem : vec) {
        if (elem == val) return true;
    }
    return false;
}

void process_number(std::integral auto val) {
    (void)val;
}

template <Printable T>
std::string to_string_rep(const T& v) {
    std::ostringstream oss;
    oss << v;
    return oss.str();
}

template <Numeric T>
T clamp_value(T val, T lo, T hi) {
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

template <Container C>
auto first_or_default(const C& c) -> decltype(*c.begin()) {
    if (c.empty()) {
        throw std::runtime_error("empty container");
    }
    return *c.begin();
}

TEST("Printable — accepted types") {
    ASSERT_EQ(to_string_rep(42), "42");
    ASSERT_EQ(to_string_rep(3.14), "3.14");
    ASSERT_EQ(to_string_rep(std::string("hello")), "hello");
}

TEST("Numeric — accepted types") {
    ASSERT_EQ(clamp_value(5, 1, 10), 5);
    ASSERT_EQ(clamp_value(-1, 0, 100), 0);
    ASSERT_EQ(clamp_value(150, 0, 100), 100);
    ASSERT_EQ(clamp_value(3.14, 0.0, 10.0), 3.14);
}

TEST("Container — accepted types") {
    std::vector<int> v = {10, 20, 30};
    ASSERT_EQ(first_or_default(v), 10);
    std::string s = "hello";
    ASSERT_EQ(first_or_default(s), 'h');
}

TEST("requires clause") {
    std::vector<int> nums = {1, 2, 3, 4, 5};
    ASSERT_TRUE(contains(nums, 3));
    ASSERT_FALSE(contains(nums, 99));
}

TEST("abbreviated syntax") {
    process_number(42);
    process_number(3L);
    ASSERT_TRUE(true);
}

CPPLINGS_MAIN
