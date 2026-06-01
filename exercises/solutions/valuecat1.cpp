// cpplings: valuecat1 — 解答
// 主题: 值类别 (Value Categories) — lvalue, prvalue, xvalue

#include "cpplings.h"
#include <type_traits>
#include <string>
#include <utility>

int& get_lvalue(int& x) {
    return x;
}

int get_prvalue(int x) {
    return x;
}

int&& get_xvalue(int& x) {
    return std::move(x);
}

std::string make_greeting(const std::string& name) {
    return "Hello, " + name + "!";
}

std::string&& move_string(std::string& s) {
    return std::move(s);
}

TEST("get_lvalue 返回 lvalue 引用") {
    int val = 42;
    decltype(auto) result = get_lvalue(val);
    static_assert(std::is_lvalue_reference_v<decltype(get_lvalue(val))>,
                  "get_lvalue 应返回 lvalue 引用");
    ASSERT_TRUE(std::is_lvalue_reference_v<decltype(get_lvalue(val))>);
    ASSERT_EQ(result, 42);
}

TEST("get_prvalue 返回 prvalue") {
    decltype(auto) result = get_prvalue(10);
    static_assert(std::is_same_v<decltype(get_prvalue(10)), int>,
                  "get_prvalue 应返回 int (prvalue)");
    ASSERT_FALSE(std::is_reference_v<decltype(get_prvalue(10))>);
    ASSERT_EQ(result, 10);
}

TEST("get_xvalue 返回 xvalue (rvalue 引用)") {
    int val = 7;
    decltype(auto) result = get_xvalue(val);
    static_assert(std::is_rvalue_reference_v<decltype(get_xvalue(val))>,
                  "get_xvalue 应返回 rvalue 引用 (xvalue)");
    ASSERT_TRUE(std::is_rvalue_reference_v<decltype(get_xvalue(val))>);
    ASSERT_EQ(result, 7);
}

TEST("std::move 产生 xvalue") {
    std::string s = "hello";
    auto&& moved = std::move(s);
    static_assert(std::is_rvalue_reference_v<decltype(std::move(s))>,
                  "std::move 返回 rvalue 引用");
    ASSERT_TRUE((std::is_same_v<decltype(std::move(s)), std::string&&>));
}

TEST("make_greeting 返回 prvalue，临时物化") {
    auto greeting = make_greeting("World");
    ASSERT_EQ(greeting, "Hello, World!");
    static_assert(std::is_same_v<decltype(make_greeting("x")), std::string>,
                  "make_greeting 返回 prvalue (std::string)");
}

TEST("move_string 转移所有权") {
    std::string original = "transfer me";
    std::string moved_to = move_string(original);
    ASSERT_EQ(moved_to, "transfer me");
    ASSERT_TRUE((std::is_rvalue_reference_v<decltype(move_string(original))>));
}

TEST("decltype 区分 lvalue 和 xvalue") {
    int x = 1;
    static_assert(std::is_same_v<decltype(x), int>);
    static_assert(std::is_same_v<decltype(std::move(x)), int&&>);
    static_assert(std::is_same_v<decltype((x)), int&>);
    ASSERT_TRUE(true);
}

CPPLINGS_MAIN
