// Solution: 二进制字面量与数字分隔符 (Binary Literals & Digit Separators)

#include "cpplings.h"

TEST("binary literal 0b1010") {
    int x = 0b1010;
    ASSERT_EQ(x, 10);
}

TEST("binary literal 0b11111111") {
    int x = 0b11111111;
    ASSERT_EQ(x, 255);
}

TEST("digit separator with large number") {
    int x = 1'000'000;
    ASSERT_EQ(x, 1000000);
}

TEST("digit separator with hex") {
    int x = 0xFF'FF;
    ASSERT_EQ(x, 65535);
}

TEST("binary with digit separator") {
    int x = 0b1111'1111'1111'1111;
    ASSERT_EQ(x, 65535);
}

CPPLINGS_MAIN
