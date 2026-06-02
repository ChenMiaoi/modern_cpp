// Solution: any1 — std::any
#include "cpplings.h"
#include <any>
#include <string>

TEST("any — 存储和取回 int") {
    std::any a = 42;
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(a.type() == typeid(int));

    int val = std::any_cast<int>(a);
    ASSERT_EQ(val, 42);
}

TEST("any — 存储和取回 string") {
    std::any a = std::string("hello");
    ASSERT_TRUE(a.has_value());

    auto s = std::any_cast<std::string>(a);
    ASSERT_EQ(s, "hello");
}

TEST("any — bad_any_cast") {
    std::any a = 42;
    ASSERT_THROWS(std::any_cast<std::string>(a), std::bad_any_cast);
}

TEST("any — reset 和 empty") {
    std::any a = 100;
    ASSERT_TRUE(a.has_value());

    a.reset();
    ASSERT_FALSE(a.has_value());

    std::any empty;
    ASSERT_FALSE(empty.has_value());

    ASSERT_THROWS(std::any_cast<int>(empty), std::bad_any_cast);
}

TEST("any — 指针形式 cast") {
    std::any a = 99;

    auto* p = std::any_cast<int>(&a);
    ASSERT_TRUE(p != nullptr);
    ASSERT_EQ(*p, 99);

    auto* wrong = std::any_cast<double>(&a);
    ASSERT_TRUE(wrong == nullptr);
}

CPPLINGS_MAIN
