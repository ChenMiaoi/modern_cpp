// Solution: foldexpr1 — 折叠表达式
#include "cpplings.h"

template<typename... Args>
auto sum(Args... args) {
    return (args + ... + 0);
}

template<typename... Args>
auto product(Args... args) {
    return (args * ... * 1);
}

template<typename... Args>
bool all_true(Args... args) {
    return (... && args);
}

template<typename... Args>
int count_true(Args... args) {
    return (0 + ... + (args ? 1 : 0));
}

TEST("fold — sum (二元右折叠)") {
    ASSERT_EQ(sum(1, 2, 3), 6);
    ASSERT_EQ(sum(10), 10);
    ASSERT_EQ(sum(), 0);
}

TEST("fold — product (二元右折叠)") {
    ASSERT_EQ(product(2, 3, 4), 24);
    ASSERT_EQ(product(5), 5);
    ASSERT_EQ(product(), 1);
}

TEST("fold — all_true (一元左折叠)") {
    ASSERT_TRUE(all_true(true, true, true));
    ASSERT_FALSE(all_true(true, false, true));
    ASSERT_TRUE(all_true(true));
}

TEST("fold — count_true (带 lambda 的折叠)") {
    ASSERT_EQ(count_true(true, false, true, true), 3);
    ASSERT_EQ(count_true(false, false), 0);
    ASSERT_EQ(count_true(), 0);
}

CPPLINGS_MAIN
