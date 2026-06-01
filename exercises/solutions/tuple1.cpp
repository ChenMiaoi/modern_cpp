// Solution: std::tuple
// std::tuple 是 C++11 引入的固定大小异构容器，
// 可以存储不同类型的值。

#include "cpplings.h"
#include <tuple>
#include <string>

TEST("make_tuple 创建") {
    auto t = std::make_tuple(1, 3.14, "hello");

    ASSERT_EQ(std::get<0>(t), 1);
    ASSERT_EQ(std::get<1>(t), 3.14);
    ASSERT_EQ(std::string(std::get<2>(t)), std::string("hello"));
}

TEST("get 按索引访问") {
    auto t = std::make_tuple(10, std::string("world"), true);

    int first = std::get<0>(t);
    std::string second = std::get<1>(t);
    bool third = std::get<2>(t);

    ASSERT_EQ(first, 10);
    ASSERT_EQ(second, std::string("world"));
    ASSERT_TRUE(third);
}

TEST("tie 解包") {
    auto t = std::make_tuple(42, std::string("answer"), 99.9);

    int a;
    std::string b;
    double c;
    std::tie(a, b, c) = t;

    ASSERT_EQ(a, 42);
    ASSERT_EQ(b, std::string("answer"));
    ASSERT_EQ(c, 99.9);
}

TEST("tuple_size 获取大小") {
    auto t = std::make_tuple(1, 2.0, 'c', std::string("hi"));

    auto sz = std::tuple_size<decltype(t)>::value;

    ASSERT_EQ(sz, 4u);
}

TEST("tie 和 ignore") {
    auto t = std::make_tuple(1, 2, 3);

    int first = 0;
    int third = 0;
    std::tie(first, std::ignore, third) = t;

    ASSERT_EQ(first, 1);
    ASSERT_EQ(third, 3);
}

CPPLINGS_MAIN
