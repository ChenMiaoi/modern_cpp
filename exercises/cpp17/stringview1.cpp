// Exercise: stringview1 — std::string_view
// 使用 std::string_view 进行零拷贝字符串操作。
//
// 任务:
//   1. 从 string 和字面量创建 string_view
//   2. 使用 substr 和 find
//   3. 使用 remove_prefix 和 remove_suffix
//   4. 理解 string_view 不拥有内存（零拷贝）
// 提示: string_view 只是 {指针, 长度}，修改底层 string 会导致悬垂

#include "cpplings.h"
#include <string>
#include <string_view>

TEST("string_view — 创建") {
    std::string s = "Hello, World!";
    std::string_view sv = s;
    ASSERT_EQ(sv.size(), 13u);
    ASSERT_EQ(sv[0], 'H');

    // 从字面量创建（零拷贝）
    std::string_view sv2 = "Literal";
    ASSERT_EQ(sv2.size(), 7u);
    ASSERT_EQ(sv2, "Literal");
}

TEST("string_view — substr") {
    std::string_view sv = "Hello, World!";
    // TODO: 使用 substr 提取 "World"
    // 提示: sv.substr(7, 5) 或 sv.substr(7)
    int _todo_ = "请删除此行，实现上面的 TODO";
    // auto sub = sv.substr(7, 5);
    // ASSERT_EQ(sub, "World");
}

TEST("string_view — find") {
    std::string_view sv = "the quick brown fox";

    // TODO: 使用 find 找到 "quick" 的位置
    int _todo_ = "请删除此行，实现上面的 TODO";
    // auto pos = sv.find("quick");
    // ASSERT_EQ(pos, 4u);

    // TODO: 使用 find 找到 "fox" 并提取
    // auto fox_pos = sv.find("fox");
    // auto fox = sv.substr(fox_pos);
    // ASSERT_EQ(fox, "fox");
}

TEST("string_view — remove_prefix / remove_suffix") {
    std::string_view sv = "  Hello  ";

    // TODO: 使用 remove_prefix 去掉前导空格，remove_suffix 去掉尾部空格
    // 提示: sv.remove_prefix(2) 去掉前2个字符
    int _todo_ = "请删除此行，实现上面的 TODO";
    // sv.remove_prefix(2);
    // sv.remove_suffix(2);
    // ASSERT_EQ(sv, "Hello");
    // ASSERT_EQ(sv.size(), 5u);
}

TEST("string_view — 零拷贝验证") {
    std::string s = "shared data";
    std::string_view sv = s;

    // TODO: 验证 string_view 和 string 指向相同的内存
    // 提示: sv.data() == s.c_str()
    int _todo_ = "请删除此行，实现上面的 TODO";
    // ASSERT_TRUE(sv.data() == s.c_str());
    // ASSERT_EQ(sv.size(), s.size());
}

CPPLINGS_MAIN
