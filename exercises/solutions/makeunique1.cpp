// Solution: std::make_unique

#include "cpplings.h"
#include <memory>
#include <string>

TEST("make_unique for single object") {
    auto ptr = std::make_unique<int>(42);
    ASSERT_EQ(*ptr, 42);
    ASSERT_TRUE(ptr != nullptr);
}

TEST("make_unique for string") {
    auto ptr = std::make_unique<std::string>("hello");
    ASSERT_EQ(*ptr, "hello");
    ASSERT_EQ(ptr->size(), 5u);
}

TEST("make_unique for array") {
    auto arr = std::make_unique<int[]>(5);
    arr[0] = 10;
    arr[1] = 20;
    arr[2] = 30;
    ASSERT_EQ(arr[0], 10);
    ASSERT_EQ(arr[1], 20);
    ASSERT_EQ(arr[2], 30);
}

TEST("ownership transfer with make_unique") {
    auto ptr1 = std::make_unique<int>(99);
    ASSERT_EQ(*ptr1, 99);

    auto ptr2 = std::move(ptr1);
    ASSERT_TRUE(ptr1 == nullptr);
    ASSERT_EQ(*ptr2, 99);
}

TEST("make_unique with multiple arguments") {
    auto ptr = std::make_unique<std::string>(5, 'x');
    ASSERT_EQ(*ptr, "xxxxx");
}

CPPLINGS_MAIN
