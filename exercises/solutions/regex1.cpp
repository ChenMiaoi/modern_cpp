// Solution: 正则表达式基础 (std::regex)
// C++11 引入了标准正则表达式库 <regex>，
// 支持匹配、搜索和替换操作。

#include "cpplings.h"
#include <regex>
#include <string>

TEST("regex_match 全文匹配") {
    std::regex digits_only("\\d+");

    bool result1 = std::regex_match("12345", digits_only);
    bool result2 = std::regex_match("123ab", digits_only);

    ASSERT_TRUE(result1);
    ASSERT_FALSE(result2);
}

TEST("regex_search 搜索子串") {
    std::regex email_pattern("[a-zA-Z]+@[a-zA-Z]+\\.[a-zA-Z]+");
    std::string text = "联系我: alice@example.com 或 bob@test.org";

    bool found = std::regex_search(text, email_pattern);

    ASSERT_TRUE(found);
}

TEST("regex_search 提取匹配") {
    std::regex year_pattern("(\\d{4})-(\\d{2})-(\\d{2})");
    std::string date = "今天是 2025-06-15，好日子";

    std::smatch match;
    bool search_ok = std::regex_search(date, match, year_pattern);

    ASSERT_TRUE(search_ok);
    ASSERT_EQ(match[1].str(), "2025");
    ASSERT_EQ(match[2].str(), "06");
    ASSERT_EQ(match[3].str(), "15");
}

TEST("regex_replace 替换") {
    std::regex spaces("\\s+");
    std::string input = "hello   world   cpp";

    std::string replaced = std::regex_replace(input, spaces, " ");

    ASSERT_EQ(replaced, std::string("hello world cpp"));
}

TEST("regex_replace 删除匹配") {
    std::regex non_digits("[^0-9]");
    std::string phone = "电话: 138-0000-1234";

    std::string cleaned = std::regex_replace(phone, non_digits, "");

    ASSERT_EQ(cleaned, std::string("13800001234"));
}

CPPLINGS_MAIN
