// Solution: std::array
// std::array 是 C++11 引入的固定大小数组容器，
// 比原生数组更安全，支持 STL 接口。

#include "cpplings.h"
#include <array>

TEST("创建和初始化 std::array") {
    std::array<int, 5> arr = {10, 20, 30, 40, 50};

    ASSERT_EQ(arr.size(), 5u);
    ASSERT_EQ(arr[0], 10);
    ASSERT_EQ(arr[4], 50);
}

TEST("用 at() 和 operator[] 访问元素") {
    std::array<int, 3> arr = {100, 200, 300};

    int first = arr.at(0);
    int last = arr[2];

    ASSERT_EQ(first, 100);
    ASSERT_EQ(last, 300);
}

TEST("fill 将所有元素设为同一值") {
    std::array<int, 4> arr;
    arr.fill(7);

    ASSERT_EQ(arr[0], 7);
    ASSERT_EQ(arr[1], 7);
    ASSERT_EQ(arr[2], 7);
    ASSERT_EQ(arr[3], 7);
}

TEST("range-based for 计算和") {
    std::array<int, 5> arr = {1, 2, 3, 4, 5};

    int sum = 0;
    for (int n : arr) {
        sum += n;
    }

    ASSERT_EQ(sum, 15);
}

TEST("size 和 empty") {
    std::array<int, 3> arr = {1, 2, 3};
    std::array<int, 0> empty_arr = {};

    auto sz = arr.size();
    bool is_empty = empty_arr.empty();

    ASSERT_EQ(sz, 3u);
    ASSERT_TRUE(is_empty);
}

CPPLINGS_MAIN
