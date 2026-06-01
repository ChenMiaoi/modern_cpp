// Exercise: 正则表达式基础 (std::regex)
// C++11 引入了标准正则表达式库 <regex>，
// 支持匹配、搜索和替换操作。
//
// 任务:
//   1. 用 regex_match 检查整个字符串是否匹配模式
//   2. 用 regex_search 在字符串中搜索匹配的子串
//   3. 用 regex_replace 替换匹配的部分
//
// 提示: #include <regex>
//       std::regex pattern("正则表达式");
//       std::regex_match(str, pattern) — 全文匹配
//       std::regex_search(str, match, pattern) — 搜索子串
//       std::regex_replace(str, pattern, "替换字符串") — 替换

#include "cpplings.h"
#include <regex>
#include <string>

TEST("regex_match 全文匹配") {
    std::regex digits_only("\\d+");

    // TODO: 用 result1 判断 "12345" 是否全部由数字组成
    //       用 result2 判断 "123ab" 是否全部由数字组成
    // 提示: std::regex_match(str, pattern) 返回 bool

    int _todo_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    ASSERT_TRUE(result1);
    ASSERT_FALSE(result2);
}

TEST("regex_search 搜索子串") {
    std::regex email_pattern("[a-zA-Z]+@[a-zA-Z]+\\.[a-zA-Z]+");
    std::string text = "联系我: alice@example.com 或 bob@test.org";

    // TODO: 用 found 判断 text 中是否包含邮箱地址
    // 提示: std::regex_search(text, pattern) 返回 bool

    int _todo2_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    ASSERT_TRUE(found);
}

TEST("regex_search 提取匹配") {
    std::regex year_pattern("(\\d{4})-(\\d{2})-(\\d{2})");
    std::string date = "今天是 2025-06-15，好日子";

    // TODO: 用 std::smatch match 保存匹配结果
    //       搜索 date 中的日期
    //       用 match[1] 获取第一个捕获组（年）

    int _todo3_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    ASSERT_TRUE(search_ok);
    ASSERT_EQ(match[1].str(), "2025");
    ASSERT_EQ(match[2].str(), "06");
    ASSERT_EQ(match[3].str(), "15");
}

TEST("regex_replace 替换") {
    std::regex spaces("\\s+");
    std::string input = "hello   world   cpp";

    // TODO: 用 replaced 将 input 中连续空格替换为单个空格
    // 提示: std::regex_replace(input, pattern, " ")

    int _todo4_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    ASSERT_EQ(replaced, std::string("hello world cpp"));
}

TEST("regex_replace 删除匹配") {
    std::regex non_digits("[^0-9]");
    std::string phone = "电话: 138-0000-1234";

    // TODO: 用 cleaned 删除 phone 中所有非数字字符
    // 提示: 替换为空字符串 ""

    int _todo5_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    ASSERT_EQ(cleaned, std::string("13800001234"));
}

CPPLINGS_MAIN
