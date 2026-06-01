// Exercise: ctad1 — 类模板参数推导 (CTAD)
// 使用 C++17 的类模板参数推导，避免显式指定模板参数。
//
// 任务:
//   1. 使用 CTAD 创建 pair 和 tuple，不写模板参数
//   2. 为自定义类编写 deduction guide
//   3. 理解 CTAD 如何从构造函数参数推导类型
// 提示: C++17 允许 std::pair(1, "hi") 而非 std::pair<int, const char*>(1, "hi")

#include "cpplings.h"
#include <utility>
#include <tuple>
#include <string>
#include <vector>

// TODO: 实现一个简单的 Range 类，有 begin_ 和 end_ 成员
// 提示:
//   template<typename T>
//   struct Range {
//       T begin_, end_;
//       Range(T b, T e) : begin_(b), end_(e) {}
//   };
// 然后写 deduction guide:
//   template<typename T> Range(T, T) -> Range<T>;

TEST("CTAD — pair 不写模板参数") {
    // C++17: 可以从参数推导 pair 的类型
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
    // TODO: 使用你实现的 Range 类，用 CTAD 创建 Range(1, 10)
    int _todo_ = "请删除此行，实现上面的 TODO";
    // Range r(1, 10);
    // ASSERT_EQ(r.begin_, 1);
    // ASSERT_EQ(r.end_, 10);
    // static_assert(std::is_same_v<decltype(r.begin_), int>);
}

TEST("CTAD — vector 的 CTAD") {
    // C++17: std::vector 可以从 initializer_list 推导
    std::vector v{1, 2, 3, 4, 5};
    ASSERT_EQ(v.size(), 5u);
    ASSERT_EQ(v[0], 1);
    ASSERT_EQ(v[4], 5);
}

TEST("CTAD — 嵌套 CTAD") {
    // TODO: 使用 CTAD 创建嵌套结构: pair of pair
    // 提示: std::pair outer(std::pair(1, 2), std::string("nested"));
    int _todo_ = "请删除此行，实现上面的 TODO";
    // auto outer = std::pair(std::pair(1, 2), std::string("nested"));
    // ASSERT_EQ(outer.first.first, 1);
    // ASSERT_EQ(outer.first.second, 2);
    // ASSERT_EQ(outer.second, "nested");
}

CPPLINGS_MAIN
