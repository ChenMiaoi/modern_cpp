// Solution: std::exchange

#include "cpplings.h"
#include <utility>
#include <memory>

TEST("exchange with integers") {
    int a = 10;
    int old = std::exchange(a, 20);
    ASSERT_EQ(old, 10);
    ASSERT_EQ(a, 20);
}

TEST("exchange with zero") {
    int x = 42;
    int old = std::exchange(x, 0);
    ASSERT_EQ(old, 42);
    ASSERT_EQ(x, 0);
}

TEST("exchange with pointer") {
    int value = 100;
    int* raw_ptr = &value;
    int* old_ptr = std::exchange(raw_ptr, nullptr);
    ASSERT_TRUE(old_ptr == &value);
    ASSERT_TRUE(raw_ptr == nullptr);
    ASSERT_EQ(*old_ptr, 100);
}

TEST("exchange in move constructor") {
    struct Buffer {
        int* data;
        int size;

        Buffer(int n) : data(new int[n]), size(n) {
            for (int i = 0; i < n; ++i) data[i] = i;
        }

        Buffer(Buffer&& other)
            : data(std::exchange(other.data, nullptr))
            , size(std::exchange(other.size, 0))
        {}

        ~Buffer() { delete[] data; }

        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;
    };

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
    struct Point { int x; int y; };
    Point p = {1, 2};
    Point old = std::exchange(p, {3, 4});
    ASSERT_EQ(old.x, 1);
    ASSERT_EQ(old.y, 2);
    ASSERT_EQ(p.x, 3);
    ASSERT_EQ(p.y, 4);
}

CPPLINGS_MAIN
