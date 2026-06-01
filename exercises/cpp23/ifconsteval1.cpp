// cpplings: ifconsteval1
// 主题: C++23 — if consteval 编译期/运行期分支
//
// TODO: 使用 if consteval 在同一函数中区分编译期和运行期执行路径
//
// 提示: if consteval { ... } 分支仅在常量求值上下文中执行
//       普通运行时调用走 else 分支
//       比 C++20 的 if constexpr 更适合此场景

#include "cpplings.h"
#include <string>
#include <array>

// TODO: safe_divide — 在编译期检查除零（编译期除零应为编译错误）
// 运行期用 assert 或返回特殊值
constexpr int safe_divide(int a, int b) {
    int _todo_ = "FILL IN THE TODO";
    return 0;
}

// TODO: constexpr_abs — 编译期和运行期都能工作的绝对值
// if consteval 分支中用编译期安全的方式
// else 分支中可以用更高效的运行期方式
constexpr int constexpr_abs(int x) {
    int _todo_ = "FILL IN THE TODO";
    return 0;
}

// TODO: compile_time_factorial — 使用 if consteval 区分实现
// 编译期: 用递归
// 运行期: 用循环（更高效）
constexpr int smart_factorial(int n) {
    int _todo_ = "FILL IN THE TODO";
    return 0;
}

// TODO: make_const_array — 在编译期创建一个数组
// 填充 0..N-1 的值
template <std::size_t N>
constexpr std::array<int, N> make_sequence() {
    int _todo_ = "FILL IN THE TODO";
    return {};
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
