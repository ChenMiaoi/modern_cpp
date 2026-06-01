// cpplings: compiletime1
// 主题: 编译期计算 — constexpr/consteval 数据结构与算法
//
// TODO: 使用 constexpr 实现编译期计算
//
// 提示: constexpr 函数可以在编译期和运行期使用
//       constexpr 函数在编译期可以常量求值，也可以在运行期使用
//       在 constexpr 上下文中使用 static_assert 验证编译期计算

#include "cpplings.h"
#include <array>
#include <string_view>
#include <cstddef>

// TODO: constexpr_array — 编译期创建并填充数组
// 创建包含 N 个元素的 array，元素值为 func(0), func(1), ..., func(N-1)
// 提示: N 作为模板参数传递，如 constexpr_map<N>(func)
template <std::size_t N, typename F>
constexpr std::array<int, N> constexpr_map(F func) {
    int _todo_ = "FILL IN THE TODO";
    return {};
}

// TODO: compile_time_hash — 编译期字符串哈希 (FNV-1a)
// 对 string_view 计算哈希值
constexpr std::size_t fnv_hash(std::string_view sv) {
    int _todo_ = "FILL IN THE TODO";
    return 0;
}

// TODO: compile_time_contains — 编译期检查字符串是否包含某字符
constexpr bool str_contains(std::string_view sv, char c) {
    int _todo_ = "FILL IN THE TODO";
    return false;
}

// TODO: compile_time_factorial — 用 constexpr 实现阶乘
constexpr int consteval_factorial(int n) {
    int _todo_ = "FILL IN THE TODO";
    return 0;
}

// TODO: constexpr_count_if — 编译期统计满足条件的元素个数
template <typename T, std::size_t N, typename Pred>
constexpr int constexpr_count_if(const std::array<T, N>& arr, Pred pred) {
    int _todo_ = "FILL IN THE TODO";
    return 0;
}

TEST("constexpr_map 创建编译期数组") {
    constexpr auto squares = constexpr_map<5>([](int i) { return i * i; });
    static_assert(squares[0] == 0);
    static_assert(squares[1] == 1);
    static_assert(squares[4] == 16);
    ASSERT_EQ(squares[0], 0);
    ASSERT_EQ(squares[3], 9);
}

TEST("fnv_hash 编译期哈希") {
    constexpr auto h1 = fnv_hash("hello");
    constexpr auto h2 = fnv_hash("hello");
    constexpr auto h3 = fnv_hash("world");
    static_assert(h1 == h2, "相同字符串哈希相同");
    static_assert(h1 != h3, "不同字符串哈希不同");
    ASSERT_EQ(h1, h2);
    ASSERT_TRUE(h1 != h3);
}

TEST("str_contains 编译期查找字符") {
    constexpr bool has_o = str_contains("hello", 'o');
    constexpr bool has_z = str_contains("hello", 'z');
    static_assert(has_o == true);
    static_assert(has_z == false);
    ASSERT_TRUE(has_o);
    ASSERT_FALSE(has_z);
}

TEST("consteval_factorial 编译期阶乘") {
    static_assert(consteval_factorial(0) == 1);
    static_assert(consteval_factorial(1) == 1);
    static_assert(consteval_factorial(5) == 120);
    static_assert(consteval_factorial(10) == 3628800);
    ASSERT_EQ(consteval_factorial(6), 720);
}

TEST("constexpr_count_if 统计偶数") {
    constexpr std::array<int, 6> arr = {1, 2, 3, 4, 5, 6};
    constexpr int evens = constexpr_count_if(arr, [](int x) { return x % 2 == 0; });
    static_assert(evens == 3);
    ASSERT_EQ(evens, 3);
}

TEST("constexpr_count_if 统计大于阈值") {
    constexpr std::array<int, 5> arr = {10, 20, 30, 40, 50};
    constexpr int big = constexpr_count_if(arr, [](int x) { return x > 25; });
    static_assert(big == 3);
    ASSERT_EQ(big, 3);
}

CPPLINGS_MAIN
