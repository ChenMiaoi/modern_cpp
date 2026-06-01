// Solution — requires1: requires 表达式与约束
#include "cpplings.h"
#include <concepts>
#include <string>
#include <vector>
#include <type_traits>

template <typename T>
concept Addable = requires(T a, T b) {
    a + b;
};

TEST("simple requires — Addable") {
    ASSERT_TRUE(Addable<int>);
    ASSERT_TRUE(Addable<std::string>);
    ASSERT_FALSE(Addable<std::vector<int>>);
}

template <typename T>
concept HasValueType = requires {
    typename T::value_type;
};

TEST("type requirement — HasValueType") {
    ASSERT_TRUE(HasValueType<std::vector<int>>);
    ASSERT_TRUE(HasValueType<std::string>);
    ASSERT_FALSE(HasValueType<int>);
}

template <typename T>
concept Sized = requires(T t) {
    { t.size() } -> std::convertible_to<std::size_t>;
};

TEST("compound requirement — Sized") {
    ASSERT_TRUE(Sized<std::string>);
    ASSERT_TRUE(Sized<std::vector<int>>);
    ASSERT_FALSE(Sized<int>);
}

template <typename T>
concept SameSizeAddable = requires(T a, T b) {
    { a + b } -> std::same_as<T>;
};

TEST("nested requirement — SameSizeAddable") {
    ASSERT_TRUE(SameSizeAddable<int>);
    ASSERT_TRUE(SameSizeAddable<std::string>);
    ASSERT_FALSE(SameSizeAddable<short>);  // short + short promotes to int, not short
}

auto describe_size(const auto& c) requires requires { c.size(); } {
    return static_cast<int>(c.size());
}

TEST("inline requires clause") {
    std::string s = "hello";
    ASSERT_EQ(describe_size(s), 5);
    std::vector<int> v = {1, 2, 3};
    ASSERT_EQ(describe_size(v), 3);
}

CPPLINGS_MAIN
