// Exercise: 二进制字面量与数字分隔符 (Binary Literals & Digit Separators)
// C++14 引入了二进制字面量 (0b前缀) 和数字分隔符 (') 来提高代码可读性。
//
// 任务:
//   1. 使用 0b 前缀定义二进制字面量
//   2. 使用 ' 分隔符让大数字更易读
//   3. 组合使用二进制字面量和分隔符
// 提示: 0b1010 表示十进制的 10，1'000'000 表示一百万

#include "cpplings.h"

TEST("binary literal 0b1010") {
    // TODO: 定义一个变量 x，用二进制字面量赋值为 10
    int _todo_ = "请删除此行，实现上面的 TODO";
    int x = 0b1010;
    ASSERT_EQ(x, 10);
}

TEST("binary literal 0b11111111") {
    // TODO: 用二进制字面量表示 255 (即 0b11111111)
    int _todo_ = "请删除此行，实现上面的 TODO";
    int x = 0b11111111;
    ASSERT_EQ(x, 255);
}

TEST("digit separator with large number") {
    // TODO: 用数字分隔符定义一百万
    int _todo_ = "请删除此行，实现上面的 TODO";
    int x = 1'000'000;
    ASSERT_EQ(x, 1000000);
}

TEST("digit separator with hex") {
    // TODO: 用数字分隔符定义十六进制数 0xFF'FF
    int _todo_ = "请删除此行，实现上面的 TODO";
    int x = 0xFF'FF;
    ASSERT_EQ(x, 65535);
}

TEST("binary with digit separator") {
    // TODO: 用二进制字面量和分隔符定义 0b1111'1111'1111'1111 (即 65535)
    int _todo_ = "请删除此行，实现上面的 TODO";
    int x = 0b1111'1111'1111'1111;
    ASSERT_EQ(x, 65535);
}

CPPLINGS_MAIN
