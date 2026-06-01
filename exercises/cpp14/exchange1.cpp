// Exercise: std::exchange
// C++14 引入了 std::exchange，它将一个变量设置为新值并返回旧值。
// 这在实现移动语义时特别有用。
//
// 任务:
//   1. 使用 std::exchange 交换整数值
//   2. 在移动构造函数中使用 std::exchange
//   3. 使用 std::exchange 重置指针并获取旧值
// 提示: std::exchange(target, new_value) 返回 target 的旧值

#include "cpplings.h"
#include <utility>
#include <memory>

TEST("exchange with integers") {
    // TODO: 使用 std::exchange 将 a 从 10 变为 20，并获取旧值
    int _todo_ = "请删除此行，实现上面的 TODO";
    int a = 10;
    int old = std::exchange(a, 20);
    ASSERT_EQ(old, 10);
    ASSERT_EQ(a, 20);
}

TEST("exchange with zero") {
    // TODO: 使用 std::exchange 将 x 设为 0，返回旧值
    int _todo_ = "请删除此行，实现上面的 TODO";
    int x = 42;
    int old = std::exchange(x, 0);
    ASSERT_EQ(old, 42);
    ASSERT_EQ(x, 0);
}

TEST("exchange with pointer") {
    // TODO: 使用 std::exchange 将 raw_ptr 设为 nullptr，获取旧指针
    int _todo_ = "请删除此行，实现上面的 TODO";
    int value = 100;
    int* raw_ptr = &value;
    int* old_ptr = std::exchange(raw_ptr, nullptr);
    ASSERT_TRUE(old_ptr == &value);
    ASSERT_TRUE(raw_ptr == nullptr);
    ASSERT_EQ(*old_ptr, 100);
}

TEST("exchange in move constructor") {
    // 一个简单的资源持有类，展示 exchange 在移动构造中的使用
    struct Buffer {
        int* data;
        int size;

        Buffer(int n) : data(new int[n]), size(n) {
            for (int i = 0; i < n; ++i) data[i] = i;
        }

        // TODO: 使用 std::exchange 实现移动构造函数
        // 将 other.data 设为 nullptr，other.size 设为 0
        Buffer(Buffer&& other)
            : data(std::exchange(other.data, nullptr))
            , size(std::exchange(other.size, 0))
        {}

        ~Buffer() { delete[] data; }

        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;
    };

    int _todo_ = "请删除此行，实现上面的 TODO";
    Buffer b1(5);
    ASSERT_EQ(b1.size, 5);
    ASSERT_TRUE(b1.data != nullptr);

    Buffer b2(std::move(b1));
    ASSERT_EQ(b2.size, 5);
    ASSERT_EQ(b2.data[3], 3);
    ASSERT_TRUE(b1.data == nullptr);
    ASSERT_EQ(b1.size, 0);
}

TEST("exchange with custom type") {
    // TODO: 使用 exchange 交换一个 struct 的值
    struct Point { int x; int y; };
    int _todo_ = "请删除此行，实现上面的 TODO";
    Point p = {1, 2};
    Point old = std::exchange(p, {3, 4});
    ASSERT_EQ(old.x, 1);
    ASSERT_EQ(old.y, 2);
    ASSERT_EQ(p.x, 3);
    ASSERT_EQ(p.y, 4);
}

CPPLINGS_MAIN
