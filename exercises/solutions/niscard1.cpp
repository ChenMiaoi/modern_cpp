// Solution: nodiscard1 — [[nodiscard]] 等属性
#include "cpplings.h"
#include <string>

[[nodiscard]] int compute(int x) {
    return x * 2;
}

[[nodiscard("use the result")]] int allocate(int size) {
    return size * 8;
}

TEST("nodiscard — 函数返回值不可忽略") {
    int result = compute(21);
    ASSERT_EQ(result, 42);
}

TEST("nodiscard — 带消息的 nodiscard") {
    int bytes = allocate(100);
    ASSERT_EQ(bytes, 800);
}

TEST("maybe_unused — 抑制未使用警告") {
    [[maybe_unused]] int debug_val = 42;
    ASSERT_TRUE(true);
}

TEST("fallthrough — switch 中的有意穿透") {
    int value = 1;
    int result = 0;

    switch (value) {
    case 1:
        result += 10;
        [[fallthrough]];
    case 2:
        result += 20;
        break;
    case 3:
        result += 30;
        break;
    }

    ASSERT_EQ(result, 30);
}

TEST("fallthrough — 无穿透时 case 独立") {
    int value = 2;
    int result = 0;

    switch (value) {
    case 1:
        result += 10;
        break;
    case 2:
        result += 20;
        break;
    case 3:
        result += 30;
        break;
    }

    ASSERT_EQ(result, 20);
}

CPPLINGS_MAIN
