// Solution: auto 类型推导
// 使用 auto 关键字让编译器推导正确的变量类型

#include "cpplings.h"
#include <vector>
#include <type_traits>

TEST("auto 推导基本类型") {
    auto a = 42;         // int
    auto b = 3.14;       // double
    auto c = true;       // bool
    auto d = 'x';        // char

    ASSERT_EQ(a, 42);
    ASSERT_EQ(b, 3.14);
    ASSERT_EQ(c, true);
    ASSERT_EQ(d, 'x');
}

TEST("auto 与 const") {
    const int x = 10;

    const auto copy = x;   // const auto 保留顶层 const
    const auto cref = x;

    ASSERT_EQ(copy, 10);
    ASSERT_EQ(cref, 10);
    static_assert(std::is_same<decltype(cref), const int>::value,
                  "cref 应该是 const int");
}

TEST("auto 与引用") {
    int val = 100;
    int& ref = val;

    auto copy = ref;    // auto 去掉引用，copy 是 int

    ASSERT_EQ(copy, 100);
    copy = 200;
    ASSERT_EQ(val, 100);  // 修改 copy 不应影响 val
}

TEST("auto 推导容器迭代器") {
    std::vector<int> nums = {10, 20, 30, 40, 50};

    auto it = nums.begin();

    ASSERT_EQ(*it, 10);
    ++it;
    ASSERT_EQ(*it, 20);

    // 使用 auto 配合 range-based for
    int sum = 0;
    for (auto n : nums) {
        sum += n;
    }
    ASSERT_EQ(sum, 150);
}

CPPLINGS_MAIN
