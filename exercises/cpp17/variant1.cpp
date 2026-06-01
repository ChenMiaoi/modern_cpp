// Exercise: variant1 — std::variant
// 使用 std::variant 存储不同类型的值，并学会访问和检查 variant 中的值。
//
// 任务:
//   1. 创建一个可以持有 int、double、string 的 variant
//   2. 使用 std::visit 和 overloaded lambda 访问 variant
//   3. 使用 std::get、std::get_if 和 std::holds_alternative
// 提示: std::variant 是类型安全的 union，index() 返回当前持有的类型下标

#include "cpplings.h"
#include <variant>
#include <string>
#include <sstream>

// TODO: 实现这个 Visitor 辅助结构体（类似 C++17 常见的 overloaded 模式）
// 提示: 使用模板继承，如 struct Overloaded : Ts... { using Ts::operator()...; };
// 然后提供一个推导指引。

TEST("variant — 创建和 index") {
    std::variant<int, double, std::string> v = 42;
    ASSERT_EQ(v.index(), 0u);
    ASSERT_TRUE(std::holds_alternative<int>(v));

    v = 3.14;
    ASSERT_EQ(v.index(), 1u);
    ASSERT_TRUE(std::holds_alternative<double>(v));

    v = std::string("hello");
    ASSERT_EQ(v.index(), 2u);
    ASSERT_TRUE(std::holds_alternative<std::string>(v));
}

TEST("variant — std::get 获取值") {
    std::variant<int, double, std::string> v = 100;
    ASSERT_EQ(std::get<int>(v), 100);
    ASSERT_EQ(std::get<0>(v), 100);

    v = std::string("world");
    ASSERT_EQ(std::get<std::string>(v), "world");
}

TEST("variant — std::get_if") {
    std::variant<int, double, std::string> v = 3.14;

    // TODO: 使用 std::get_if<double> 获取值指针
    // 提示: auto* p = std::get_if<double>(&v);
    int _todo_ = "请删除此行，实现上面的 TODO";
    // ASSERT_TRUE(p != nullptr);
    // ASSERT_EQ(*p, 3.14);

    // TODO: 对 int 类型，get_if 应返回 nullptr
    // auto* wrong = std::get_if<int>(&v);
    // ASSERT_TRUE(wrong == nullptr);
}

TEST("variant — std::visit") {
    std::variant<int, double, std::string> v = std::string("C++17");

    // TODO: 使用 std::visit 访问 variant 并计算结果
    // 提示: std::visit 接受一个可调用对象，可以用 overloaded lambda 模式
    // 结果应该是所有类型都转成 string:
    //   int -> "int:42", double -> "double:3.14", string -> "string:hello"
    int _todo_ = "请删除此行，实现上面的 TODO";

    // 一种实现方式:
    // struct Overloaded : Ts... { using Ts::operator()...; };
    // template<class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;
    // std::string result = std::visit(Overloaded{
    //     [](int x) { return "int:" + std::to_string(x); },
    //     [](double x) { /* ... */ },
    //     [](const std::string& s) { return "string:" + s; }
    // }, v);
    // ASSERT_EQ(result, "string:C++17");
}

TEST("variant — get 异常") {
    std::variant<int, double, std::string> v = 42;
    // std::get<double> 对 int 类型 variant 应抛出 std::bad_variant_access
    ASSERT_THROWS(std::get<double>(v), std::bad_variant_access);
}

CPPLINGS_MAIN
