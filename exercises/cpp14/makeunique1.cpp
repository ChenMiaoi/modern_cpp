// Exercise: std::make_unique
// C++14 引入了 std::make_unique，是创建 unique_ptr 的推荐方式。
// 相比直接使用 new，make_unique 更安全（异常安全）且更简洁。
//
// 任务:
//   1. 使用 std::make_unique 创建单个对象
//   2. 使用 std::make_unique<T[]> 创建数组
//   3. 展示 unique_ptr 的所有权转移 (std::move)
// 提示: #include <memory> 提供 make_unique 和 unique_ptr

#include "cpplings.h"
#include <memory>
#include <string>

TEST("make_unique for single object") {
    // TODO: 使用 std::make_unique<int> 创建一个值为 42 的 int
    int _todo_ = "请删除此行，实现上面的 TODO";
    auto ptr = std::make_unique<int>(42);
    ASSERT_EQ(*ptr, 42);
    ASSERT_TRUE(ptr != nullptr);
}

TEST("make_unique for string") {
    // TODO: 使用 std::make_unique<std::string> 创建字符串 "hello"
    int _todo_ = "请删除此行，实现上面的 TODO";
    auto ptr = std::make_unique<std::string>("hello");
    ASSERT_EQ(*ptr, "hello");
    ASSERT_EQ(ptr->size(), 5u);
}

TEST("make_unique for array") {
    // TODO: 使用 std::make_unique<int[]> 创建大小为 5 的数组
    int _todo_ = "请删除此行，实现上面的 TODO";
    auto arr = std::make_unique<int[]>(5);
    arr[0] = 10;
    arr[1] = 20;
    arr[2] = 30;
    ASSERT_EQ(arr[0], 10);
    ASSERT_EQ(arr[1], 20);
    ASSERT_EQ(arr[2], 30);
}

TEST("ownership transfer with make_unique") {
    // TODO: 创建一个 unique_ptr，然后用 std::move 转移所有权
    int _todo_ = "请删除此行，实现上面的 TODO";
    auto ptr1 = std::make_unique<int>(99);
    ASSERT_EQ(*ptr1, 99);

    auto ptr2 = std::move(ptr1);
    ASSERT_TRUE(ptr1 == nullptr);
    ASSERT_EQ(*ptr2, 99);
}

TEST("make_unique with multiple arguments") {
    // TODO: 使用 make_unique 创建一个 std::string，传入重复字符构造参数
    int _todo_ = "请删除此行，实现上面的 TODO";
    auto ptr = std::make_unique<std::string>(5, 'x');
    ASSERT_EQ(*ptr, "xxxxx");
}

CPPLINGS_MAIN
