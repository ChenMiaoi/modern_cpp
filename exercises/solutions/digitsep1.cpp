// Solution: 数字分隔符 (Digit Separators)

#include "cpplings.h"

TEST("decimal with separators") {
    int million = 1'000'000;
    ASSERT_EQ(million, 1000000);
}

TEST("large decimal with separators") {
    int big = 1'234'567'890;
    ASSERT_EQ(big, 1234567890);
}

TEST("hex with separators") {
    unsigned hex_val = 0xDEAD'BEEF;
    ASSERT_EQ(hex_val, 0xDEADBEEF);
}

TEST("binary with separators") {
    int bin_val = 0b1010'1010'1010'1010;
    ASSERT_EQ(bin_val, 0xAAAA);
}

TEST("floating point with separators") {
    double pi = 3.141'592'653'589'793;
    ASSERT_TRUE(pi > 3.141);
    ASSERT_TRUE(pi < 3.142);
}

TEST("octal with separators") {
    int oct_val = 01'777'777'777;
    ASSERT_EQ(oct_val, 01777777777);
}

CPPLINGS_MAIN
