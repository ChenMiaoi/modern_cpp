// Solution: variant1 — std::variant
#include "cpplings.h"
#include <variant>
#include <string>
#include <sstream>

template<class... Ts>
struct Overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

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

    auto* p = std::get_if<double>(&v);
    ASSERT_TRUE(p != nullptr);
    ASSERT_EQ(*p, 3.14);

    auto* wrong = std::get_if<int>(&v);
    ASSERT_TRUE(wrong == nullptr);
}

TEST("variant — std::visit") {
    auto to_string_visitor = Overloaded{
        [](int x) -> std::string { return "int:" + std::to_string(x); },
        [](double x) -> std::string {
            std::ostringstream os;
            os << "double:" << x;
            return os.str();
        },
        [](const std::string& s) -> std::string { return "string:" + s; }
    };

    std::variant<int, double, std::string> v1 = 42;
    ASSERT_EQ(std::visit(to_string_visitor, v1), "int:42");

    std::variant<int, double, std::string> v2 = 3.14;
    std::string r2 = std::visit(to_string_visitor, v2);
    ASSERT_TRUE(r2.substr(0, 7) == "double:");

    std::variant<int, double, std::string> v3 = std::string("C++17");
    ASSERT_EQ(std::visit(to_string_visitor, v3), "string:C++17");
}

TEST("variant — get 异常") {
    std::variant<int, double, std::string> v = 42;
    ASSERT_THROWS(std::get<double>(v), std::bad_variant_access);
}

CPPLINGS_MAIN
