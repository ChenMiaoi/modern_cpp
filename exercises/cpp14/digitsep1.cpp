// Exercise: 数字分隔符 (Digit Separators)
// C++14 引入了单引号 ' 作为数字分隔符，让大数字更易读。
// 分隔符可以用于任何进制的数字：十进制、十六进制、八进制、二进制。
//
// 任务:
//   1. 使用 ' 分隔符定义大数字
//   2. 在不同进制中使用分隔符
//   3. 使用分隔符表示浮点数
// 提示: 1'000'000 等价于 1000000，0xFF'FF 等价于 0xFFFF

#include "cpplings.h"

TEST("decimal with separators") {
    // TODO: 用数字分隔符定义一百万
    int _todo_ = "请删除此行，实现上面的 TODO";
    int million = 1'000'000;
    ASSERT_EQ(million, 1000000);
}

TEST("large decimal with separators") {
    // TODO: 用数字分隔符定义 1'234'567'890
    int _todo_ = "请删除此行，实现上面的 TODO";
    int big = 1'234'567'890;
    ASSERT_EQ(big, 1234567890);
}

TEST("hex with separators") {
    // TODO: 用数字分隔符定义 0xDEAD'BEEF
    int _todo_ = "请删除此行，实现上面的 TODO";
    unsigned hex_val = 0xDEAD'BEEF;
    ASSERT_EQ(hex_val, 0xDEADBEEF);
}

TEST("binary with separators") {
    // TODO: 用数字分隔符定义 0b1010'1010'1010'1010
    int _todo_ = "请删除此行，实现上面的 TODO";
    int bin_val = 0b1010'1010'1010'1010;
    ASSERT_EQ(bin_val, 0xAAAA);
}

TEST("floating point with separators") {
    // TODO: 用数字分隔符定义 3.141'592'653'589'793
    int _todo_ = "请删除此行，实现上面的 TODO";
    double pi = 3.141'592'653'589'793;
    ASSERT_TRUE(pi > 3.141);
    ASSERT_TRUE(pi < 3.142);
}

TEST("octal with separators") {
    // TODO: 用数字分隔符定义 01'777'777'777
    int _todo_ = "请删除此行，实现上面的 TODO";
    int oct_val = 01'777'777'777;
    ASSERT_EQ(oct_val, 01777777777);
}

CPPLINGS_MAIN
