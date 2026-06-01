// Exercise: auto 类型推导
// 使用 auto 关键字让编译器推导正确的变量类型
//
// 任务:
//   1. 用 auto 声明变量，使其类型正确
//   2. 理解 auto 与 const、引用的交互
//   3. 用 auto 简化迭代器声明
//
// 提示: auto 让编译器从初始化表达式推导类型。
//       auto 会去掉顶层 const，需要显式写 const auto 保留。
//       auto 会去掉引用，需要 auto& 保留引用。

#include "cpplings.h"
#include <vector>
#include <type_traits>

TEST("auto 推导基本类型") {
    // TODO: 使用 auto 声明以下变量，使其类型正确
    // 例如: auto a = 42;  // a 的类型是 int
    //
    // 完成以下四行声明:
    //   auto a = 42;        // int
    //   auto b = 3.14;      // double
    //   auto c = true;      // bool
    //   auto d = 'x';       // char

    int _todo_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    auto a = 42;
    auto b = 3.14;
    auto c = true;
    auto d = 'x';

    ASSERT_EQ(a, 42);
    ASSERT_EQ(b, 3.14);
    ASSERT_EQ(c, true);
    ASSERT_EQ(d, 'x');
}

TEST("auto 与 const") {
    const int x = 10;

    // TODO: auto 会去掉顶层 const。
    // 将下面的 auto 改为 const auto，使 copy 的类型为 const int

    int _todo2_ = "请删除此行，修改 auto 为 const auto";  // 编译错误

    auto copy = x;      // ← 修改此行：让 copy 是 const int
    const auto cref = x;

    ASSERT_EQ(copy, 10);
    ASSERT_EQ(cref, 10);
    static_assert(std::is_same_v<decltype(cref), const int>,
                  "cref 应该是 const int");
}

TEST("auto 与引用") {
    int val = 100;
    int& ref = val;

    // TODO: 用 auto 声明 copy，使其是 ref 的值拷贝（不是引用）
    // auto 会自动去掉引用，所以 auto copy = ref; 就行

    int _todo3_ = "请删除此行，声明 auto copy";  // 编译错误

    auto copy = ref;

    ASSERT_EQ(copy, 100);
    copy = 200;
    ASSERT_EQ(val, 100);  // 修改 copy 不应影响 val
}

TEST("auto 推导容器迭代器") {
    std::vector<int> nums = {10, 20, 30, 40, 50};

    // TODO: 用 auto 声明迭代器 it，指向 nums.begin()

    int _todo4_ = "请删除此行，声明 auto it";  // 编译错误

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
