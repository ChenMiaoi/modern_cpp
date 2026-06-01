// Solution — ranges1: Ranges 基础
#include "cpplings.h"
#include <ranges>
#include <vector>
#include <algorithm>
#include <numeric>

std::vector<int> filter_even(const std::vector<int>& v) {
    std::vector<int> out;
    for (int x : v | std::views::filter([](int x) { return x % 2 == 0; }))
        out.push_back(x);
    return out;
}

TEST("filter — even numbers") {
    std::vector<int> input = {1, 2, 3, 4, 5, 6, 7, 8};
    auto result = filter_even(input);
    ASSERT_EQ(result.size(), 4u);
    ASSERT_EQ(result[0], 2);
    ASSERT_EQ(result[1], 4);
    ASSERT_EQ(result[2], 6);
    ASSERT_EQ(result[3], 8);
}

std::vector<int> double_all(const std::vector<int>& v) {
    std::vector<int> out;
    for (int x : v | std::views::transform([](int x) { return x * 2; }))
        out.push_back(x);
    return out;
}

TEST("transform — double all") {
    std::vector<int> input = {1, 2, 3, 4};
    auto result = double_all(input);
    ASSERT_EQ(result.size(), 4u);
    ASSERT_EQ(result[0], 2);
    ASSERT_EQ(result[1], 4);
    ASSERT_EQ(result[2], 6);
    ASSERT_EQ(result[3], 8);
}

std::vector<int> take_n(const std::vector<int>& v, std::size_t n) {
    std::vector<int> out;
    for (int x : v | std::views::take(n))
        out.push_back(x);
    return out;
}

TEST("take — first N elements") {
    std::vector<int> input = {10, 20, 30, 40, 50};
    auto result = take_n(input, 3);
    ASSERT_EQ(result.size(), 3u);
    ASSERT_EQ(result[0], 10);
    ASSERT_EQ(result[2], 30);
}

std::vector<int> sort_copy(std::vector<int> v) {
    std::ranges::sort(v);
    return v;
}

TEST("ranges::sort") {
    std::vector<int> input = {5, 3, 1, 4, 2};
    auto result = sort_copy(input);
    ASSERT_EQ(result[0], 1);
    ASSERT_EQ(result[1], 2);
    ASSERT_EQ(result[2], 3);
    ASSERT_EQ(result[3], 4);
    ASSERT_EQ(result[4], 5);
}

CPPLINGS_MAIN
