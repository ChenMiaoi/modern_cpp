// cpplings: compiletime1 — 解答
// 主题: 编译期计算 — constexpr/consteval 数据结构与算法

#include "cpplings.h"
#include <array>
#include <string_view>
#include <cstddef>

template <std::size_t N, typename F>
constexpr std::array<int, N> constexpr_map(F func) {
    std::array<int, N> arr{};
    for (std::size_t i = 0; i < N; ++i) {
        arr[i] = func(static_cast<int>(i));
    }
    return arr;
}

constexpr std::size_t fnv_hash(std::string_view sv) {
    constexpr std::size_t basis = 14695981039346656037ULL;
    constexpr std::size_t prime = 1099511628211ULL;
    std::size_t hash = basis;
    for (char c : sv) {
        hash ^= static_cast<std::size_t>(static_cast<unsigned char>(c));
        hash *= prime;
    }
    return hash;
}

constexpr bool str_contains(std::string_view sv, char c) {
    for (char ch : sv) {
        if (ch == c) return true;
    }
    return false;
}

constexpr int consteval_factorial(int n) {
    int result = 1;
    for (int i = 2; i <= n; ++i) {
        result *= i;
    }
    return result;
}

template <typename T, std::size_t N, typename Pred>
constexpr int constexpr_count_if(const std::array<T, N>& arr, Pred pred) {
    int count = 0;
    for (std::size_t i = 0; i < N; ++i) {
        if (pred(arr[i])) ++count;
    }
    return count;
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
