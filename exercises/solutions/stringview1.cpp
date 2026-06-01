// Solution: stringview1 — std::string_view
#include "cpplings.h"
#include <string>
#include <string_view>

TEST("string_view — 创建") {
    std::string s = "Hello, World!";
    std::string_view sv = s;
    ASSERT_EQ(sv.size(), 13u);
    ASSERT_EQ(sv[0], 'H');

    std::string_view sv2 = "Literal";
    ASSERT_EQ(sv2.size(), 7u);
    ASSERT_EQ(sv2, "Literal");
}

TEST("string_view — substr") {
    std::string_view sv = "Hello, World!";
    auto sub = sv.substr(7, 5);
    ASSERT_EQ(sub, "World");
}

TEST("string_view — find") {
    std::string_view sv = "the quick brown fox";

    auto pos = sv.find("quick");
    ASSERT_EQ(pos, 4u);

    auto fox_pos = sv.find("fox");
    auto fox = sv.substr(fox_pos);
    ASSERT_EQ(fox, "fox");
}

TEST("string_view — remove_prefix / remove_suffix") {
    std::string_view sv = "  Hello  ";
    sv.remove_prefix(2);
    sv.remove_suffix(2);
    ASSERT_EQ(sv, "Hello");
    ASSERT_EQ(sv.size(), 5u);
}

TEST("string_view — 零拷贝验证") {
    std::string s = "shared data";
    std::string_view sv = s;
    ASSERT_TRUE(sv.data() == s.c_str());
    ASSERT_EQ(sv.size(), s.size());
}

CPPLINGS_MAIN
