// cpplings: valuecat1
// 主题: 值类别 (Value Categories) — lvalue, prvalue, xvalue
//
// TODO: 实现函数并理解 C++17 值类别系统
//
// 提示: lvalue — 有身份（可取地址），不可移动
//       prvalue — 纯右值，无身份，初始化对象
//       xvalue — 即将消亡的值，有身份，可移动
//       decltype(expr) 对 lvalue 返回 T&，对 prvalue 返回 T，对 xvalue 返回 T&&

#include "cpplings.h"
#include <type_traits>
#include <string>
#include <utility>

int _todo_ = "请删除此行，实现所有 TODO";  // 编译错误：类型不匹配

// TODO: 返回一个 lvalue 引用（左值引用）
// 该函数接收一个 int 引用参数并原样返回
// 提示: 返回类型应为 int&，参数类型应为 int&
int& get_lvalue(int& x) {
    // TODO: 返回 x 的引用
}

// TODO: 返回一个 prvalue（纯右值）
// 该函数接收一个 int 值并返回其副本
// 提示: 返回类型应为 int（非引用）
int get_prvalue(int x) {
    // TODO: 返回 x 的值（不是引用）
}

// TODO: 返回一个 xvalue（亡值）
// 该函数使用 std::move 将左值转为 xvalue
// 提示: std::move 的返回类型是 T&&
int&& get_xvalue(int& x) {
    // TODO: 使用 std::move 将 x 转换为 xvalue
}

// TODO: 返回一个临时字符串（prvalue），用于演示临时物化
std::string make_greeting(const std::string& name) {
    // TODO: 返回 "Hello, " + name + "!"
}

// TODO: move_string — 使用 std::move 转移字符串所有权
// 接收一个 string 引用，返回其右值引用
std::string&& move_string(std::string& s) {
    // TODO: 使用 std::move
}

TEST("get_lvalue 返回 lvalue 引用") {
    int val = 42;
    decltype(auto) result = get_lvalue(val);
    // decltype 对 lvalue 返回 int&
    static_assert(std::is_lvalue_reference_v<decltype(get_lvalue(val))>,
                  "get_lvalue 应返回 lvalue 引用");
    ASSERT_TRUE(std::is_lvalue_reference_v<decltype(get_lvalue(val))>);
    ASSERT_EQ(result, 42);
}

TEST("get_prvalue 返回 prvalue") {
    decltype(auto) result = get_prvalue(10);
    // decltype 对 prvalue 返回 int（非引用）
    static_assert(std::is_same_v<decltype(get_prvalue(10)), int>,
                  "get_prvalue 应返回 int (prvalue)");
    ASSERT_FALSE(std::is_reference_v<decltype(get_prvalue(10))>);
    ASSERT_EQ(result, 10);
}

TEST("get_xvalue 返回 xvalue (rvalue 引用)") {
    int val = 7;
    decltype(auto) result = get_xvalue(val);
    // decltype 对 xvalue 返回 T&&
    static_assert(std::is_rvalue_reference_v<decltype(get_xvalue(val))>,
                  "get_xvalue 应返回 rvalue 引用 (xvalue)");
    ASSERT_TRUE(std::is_rvalue_reference_v<decltype(get_xvalue(val))>);
    ASSERT_EQ(result, 7);
}

TEST("std::move 产生 xvalue") {
    std::string s = "hello";
    auto&& moved = std::move(s);  // std::move 返回 xvalue
    // decltype(std::move(s)) 应为 std::string&&
    static_assert(std::is_rvalue_reference_v<decltype(std::move(s))>,
                  "std::move 返回 rvalue 引用");
    ASSERT_TRUE((std::is_same_v<decltype(std::move(s)), std::string&&>));
}

TEST("make_greeting 返回 prvalue，临时物化") {
    auto greeting = make_greeting("World");
    // prvalue 被 materialized 为 glvalue 来初始化 greeting
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
    // x 是 lvalue: decltype(x) = int
    static_assert(std::is_same_v<decltype(x), int>);
    // std::move(x) 是 xvalue: decltype(std::move(x)) = int&&
    static_assert(std::is_same_v<decltype(std::move(x)), int&&>);
    // (x) 是 lvalue: decltype((x)) = int&  — 注意双括号!
    static_assert(std::is_same_v<decltype((x)), int&>);
    ASSERT_TRUE(true);
}

CPPLINGS_MAIN
