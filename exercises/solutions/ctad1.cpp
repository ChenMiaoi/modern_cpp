// Solution: ctad1 — 类模板参数推导 (CTAD)
#include "cpplings.h"
#include <utility>
#include <tuple>
#include <string>
#include <vector>

template<typename T>
struct Range {
    T begin_, end_;
    Range(T b, T e) : begin_(b), end_(e) {}
};

template<typename T>
Range(T, T) -> Range<T>;

TEST("CTAD — pair 不写模板参数") {
    std::pair p(42, std::string("hello"));
    ASSERT_EQ(p.first, 42);
    ASSERT_EQ(p.second, "hello");

    std::pair p2(3.14, true);
    static_assert(std::is_same_v<decltype(p2.first), double>);
    ASSERT_TRUE(p2.second);
}

TEST("CTAD — tuple 不写模板参数") {
    std::tuple t(1, 2.0f, std::string("world"));
    ASSERT_EQ(std::get<0>(t), 1);
    ASSERT_EQ(std::get<2>(t), "world");
}

TEST("CTAD — 自定义 deduction guide") {
    Range r(1, 10);
    ASSERT_EQ(r.begin_, 1);
    ASSERT_EQ(r.end_, 10);
    static_assert(std::is_same_v<decltype(r.begin_), int>);

    Range r2(1.5, 3.5);
    ASSERT_EQ(r2.begin_, 1.5);
    ASSERT_EQ(r2.end_, 3.5);
    static_assert(std::is_same_v<decltype(r2.begin_), double>);
}

TEST("CTAD — vector 的 CTAD") {
    std::vector v{1, 2, 3, 4, 5};
    ASSERT_EQ(v.size(), 5u);
    ASSERT_EQ(v[0], 1);
    ASSERT_EQ(v[4], 5);
}

TEST("CTAD — 嵌套 CTAD") {
    auto outer = std::pair(std::pair(1, 2), std::string("nested"));
    ASSERT_EQ(outer.first.first, 1);
    ASSERT_EQ(outer.first.second, 2);
    ASSERT_EQ(outer.second, "nested");
}

CPPLINGS_MAIN
