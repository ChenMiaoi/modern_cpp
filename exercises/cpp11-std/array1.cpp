// Exercise: std::array
// std::array 是 C++11 引入的固定大小数组容器，
// 比原生数组更安全，支持 STL 接口。
//
// 任务:
//   1. 用 std::array<int, 5> 创建并初始化数组 {10, 20, 30, 40, 50}
//   2. 用 at() 和 operator[] 访问元素
//   3. 用 fill() 将所有元素设为同一个值
//   4. 用 range-based for 计算元素之和
//   5. 使用 size() 获取数组大小
//
// 提示: #include <array>
//       std::array<int, N> a = {v1, v2, ...};
//       a.at(i) 带越界检查，a[i] 不带
//       a.fill(val) 将所有元素设为 val

#include "cpplings.h"
#include <array>

TEST("创建和初始化 std::array") {
    // TODO: 创建 arr，类型为 std::array<int, 5>，初始化为 {10, 20, 30, 40, 50}

    int _todo_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    ASSERT_EQ(arr.size(), 5u);
    ASSERT_EQ(arr[0], 10);
    ASSERT_EQ(arr[4], 50);
}

TEST("用 at() 和 operator[] 访问元素") {
    std::array<int, 3> arr = {100, 200, 300};

    // TODO: 用变量 first 和 last 分别获取 arr 的第一个和最后一个元素
    // 提示: 使用 at() 或 operator[]

    int _todo2_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    ASSERT_EQ(first, 100);
    ASSERT_EQ(last, 300);
}

TEST("fill 将所有元素设为同一值") {
    // TODO: 创建 arr，类型为 std::array<int, 4>，然后用 fill(7) 填充

    int _todo3_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    ASSERT_EQ(arr[0], 7);
    ASSERT_EQ(arr[1], 7);
    ASSERT_EQ(arr[2], 7);
    ASSERT_EQ(arr[3], 7);
}

TEST("range-based for 计算和") {
    std::array<int, 5> arr = {1, 2, 3, 4, 5};

    // TODO: 用 range-based for 循环计算 arr 中所有元素的和，存入变量 sum

    int _todo4_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    ASSERT_EQ(sum, 15);
}

TEST("size 和 empty") {
    std::array<int, 3> arr = {1, 2, 3};
    std::array<int, 0> empty_arr = {};

    // TODO: 用变量 sz 获取 arr 的大小，用变量 is_empty 判断 empty_arr 是否为空
    // 提示: size() 返回元素个数，empty() 返回是否为空

    int _todo5_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    ASSERT_EQ(sz, 3u);
    ASSERT_TRUE(is_empty);
}

CPPLINGS_MAIN
