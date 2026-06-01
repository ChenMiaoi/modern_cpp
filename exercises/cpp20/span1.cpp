// cpplings: span1
// Title: std::span 非拥有视图
// Description: Use std::span to create non-owning views over
//   contiguous data. Construct from arrays and vectors, use
//   subspan for slicing, and query size.
//
// Instructions:
//   1. Implement sum_span() to sum elements through a span.
//   2. Implement first_n() to return a subspan of the first n elements.
//   3. Implement last_n() to return a subspan of the last n elements.
//   4. Implement modify_span() that modifies data through a span.
//   5. Delete each _todo_ guard after filling in the TODO block.
//
// Hint: std::span<int> s(arr);  — from C array
//       std::span<int> s(vec.data(), vec.size()); — from vector
//       s.subspan(offset, count) — slicing

#include "cpplings.h"
#include <span>
#include <vector>
#include <numeric>

// TODO: Implement sum_span — sum all elements via a span.
//   int sum_span(std::span<const int> s) {
//       int total = 0;
//       for (int x : s) total += x;
//       return total;
//   }

TEST("span — sum elements") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // int arr[] = {1, 2, 3, 4, 5};
    // ASSERT_EQ(sum_span(arr), 15);
    // std::vector<int> v = {10, 20, 30};
    // ASSERT_EQ(sum_span(v), 60);
}

// TODO: Implement first_n — return a span of the first n elements.
//   std::span<const int> first_n(std::span<const int> s, std::size_t n) {
//       return s.subspan(0, n);
//   }

TEST("subspan — first N") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // int arr[] = {10, 20, 30, 40, 50};
    // auto sub = first_n(arr, 3);
    // ASSERT_EQ(sub.size(), 3u);
    // ASSERT_EQ(sub[0], 10);
    // ASSERT_EQ(sub[2], 30);
}

// TODO: Implement last_n — return a span of the last n elements.
//   std::span<const int> last_n(std::span<const int> s, std::size_t n) {
//       return s.subspan(s.size() - n, n);
//   }

TEST("subspan — last N") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // int arr[] = {10, 20, 30, 40, 50};
    // auto sub = last_n(arr, 2);
    // ASSERT_EQ(sub.size(), 2u);
    // ASSERT_EQ(sub[0], 40);
    // ASSERT_EQ(sub[1], 50);
}

// TODO: Implement modify_span — double each element through a span.
//   void modify_span(std::span<int> s) {
//       for (auto& x : s) x *= 2;
//   }

TEST("span — modify through span") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // int arr[] = {1, 2, 3};
    // modify_span(arr);
    // ASSERT_EQ(arr[0], 2);
    // ASSERT_EQ(arr[1], 4);
    // ASSERT_EQ(arr[2], 6);
    // std::vector<int> v = {10, 20};
    // modify_span(v);
    // ASSERT_EQ(v[0], 20);
    // ASSERT_EQ(v[1], 40);
}

CPPLINGS_MAIN
