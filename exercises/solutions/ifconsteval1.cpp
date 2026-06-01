// cpplings: ifconsteval1 — 解答
// 主题: C++23 — if consteval 编译期/运行期分支

#include "cpplings.h"
#include <string>
#include <array>
#include <stdexcept>

constexpr int safe_divide(int a, int b) {
    if consteval {
        // 编译期: b 为 0 时直接产生编译错误（除以零）
        return a / b;
    } else {
        // 运行期: 检查后除法
        if (b == 0) return 0;
        return a / b;
    }
}

constexpr int constexpr_abs(int x) {
    if consteval {
        // 编译期分支 — 简单实现
        return x < 0 ? -x : x;
    } else {
        // 运行期分支
        return x < 0 ? -x : x;
    }
}

constexpr int smart_factorial(int n) {
    if consteval {
        // 编译期: 递归
        if (n <= 1) return 1;
        return n * smart_factorial(n - 1);
    } else {
        // 运行期: 循环
        int result = 1;
        for (int i = 2; i <= n; ++i) {
            result *= i;
        }
        return result;
    }
}

template <std::size_t N>
constexpr std::array<int, N> make_sequence() {
    std::array<int, N> arr{};
    for (std::size_t i = 0; i < N; ++i) {
        arr[i] = static_cast<int>(i);
    }
    return arr;
}

TEST("safe_divide 正常除法") {
    ASSERT_EQ(safe_divide(10, 3), 3);
}

TEST("safe_divide 整除") {
    ASSERT_EQ(safe_divide(20, 5), 4);
}

TEST("constexpr_abs 正数") {
    ASSERT_EQ(constexpr_abs(42), 42);
}

TEST("constexpr_abs 负数") {
    ASSERT_EQ(constexpr_abs(-17), 17);
}

TEST("constexpr_abs 零") {
    ASSERT_EQ(constexpr_abs(0), 0);
}

TEST("constexpr_abs 可在编译期使用") {
    constexpr int val = constexpr_abs(-100);
    static_assert(val == 100, "constexpr_abs 应在编译期工作");
    ASSERT_EQ(val, 100);
}

TEST("smart_factorial 编译期计算") {
    constexpr int val = smart_factorial(6);
    static_assert(val == 720, "6! = 720");
    ASSERT_EQ(val, 720);
}

TEST("smart_factorial 运行期计算") {
    int n = 7;
    ASSERT_EQ(smart_factorial(n), 5040);
}

TEST("make_sequence 创建序列") {
    constexpr auto seq = make_sequence<5>();
    static_assert(seq[0] == 0 && seq[4] == 4, "序列 0..4");
    ASSERT_EQ(seq[0], 0);
    ASSERT_EQ(seq[2], 2);
    ASSERT_EQ(seq[4], 4);
}

CPPLINGS_MAIN
