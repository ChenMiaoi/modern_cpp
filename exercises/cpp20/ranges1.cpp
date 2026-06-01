// cpplings: ranges1
// Title: Ranges 基础 — Views and Algorithms
// Description: Use std::ranges and std::views to filter, transform,
//   take elements, and sort collections without writing raw loops.
//
// Instructions:
//   1. Implement filter_even() using views::filter.
//   2. Implement double_all() using views::transform.
//   3. Implement take_n() using views::take.
//   4. Implement sort_copy() using ranges::sort on a copied vector.
//   5. Delete each _todo_ guard after filling in the TODO block.
//
// Hint: auto result = vec | std::views::filter(pred) | std::views::transform(fn);

#include "cpplings.h"
#include <ranges>
#include <vector>
#include <algorithm>
#include <numeric>

// TODO: Implement filter_even — return a vector of even numbers from input.
//   std::vector<int> filter_even(const std::vector<int>& v) {
//       std::vector<int> out;
//       for (int x : v | std::views::filter([](int x) { return x % 2 == 0; }))
//           out.push_back(x);
//       return out;
//   }

TEST("filter — even numbers") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // std::vector<int> input = {1, 2, 3, 4, 5, 6, 7, 8};
    // auto result = filter_even(input);
    // ASSERT_EQ(result.size(), 4u);
    // ASSERT_EQ(result[0], 2);
    // ASSERT_EQ(result[1], 4);
    // ASSERT_EQ(result[2], 6);
    // ASSERT_EQ(result[3], 8);
}

// TODO: Implement double_all — return a vector where each element is doubled.
//   std::vector<int> double_all(const std::vector<int>& v) {
//       std::vector<int> out;
//       for (int x : v | std::views::transform([](int x) { return x * 2; }))
//           out.push_back(x);
//       return out;
//   }

TEST("transform — double all") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // std::vector<int> input = {1, 2, 3, 4};
    // auto result = double_all(input);
    // ASSERT_EQ(result.size(), 4u);
    // ASSERT_EQ(result[0], 2);
    // ASSERT_EQ(result[1], 4);
    // ASSERT_EQ(result[2], 6);
    // ASSERT_EQ(result[3], 8);
}

// TODO: Implement take_n — return a vector of the first n elements.
//   std::vector<int> take_n(const std::vector<int>& v, std::size_t n) {
//       std::vector<int> out;
//       for (int x : v | std::views::take(n))
//           out.push_back(x);
//       return out;
//   }

TEST("take — first N elements") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // std::vector<int> input = {10, 20, 30, 40, 50};
    // auto result = take_n(input, 3);
    // ASSERT_EQ(result.size(), 3u);
    // ASSERT_EQ(result[0], 10);
    // ASSERT_EQ(result[2], 30);
}

// TODO: Implement sort_copy — return a sorted copy of the input.
//   std::vector<int> sort_copy(std::vector<int> v) {
//       std::ranges::sort(v);
//       return v;
//   }

TEST("ranges::sort") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // std::vector<int> input = {5, 3, 1, 4, 2};
    // auto result = sort_copy(input);
    // ASSERT_EQ(result[0], 1);
    // ASSERT_EQ(result[1], 2);
    // ASSERT_EQ(result[2], 3);
    // ASSERT_EQ(result[3], 4);
    // ASSERT_EQ(result[4], 5);
}

CPPLINGS_MAIN
