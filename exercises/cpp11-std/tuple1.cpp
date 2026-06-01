// Exercise: std::tuple
// std::tuple 是 C++11 引入的固定大小异构容器，
// 可以存储不同类型的值。
//
// 任务:
//   1. 用 make_tuple 创建 tuple
//   2. 用 get<N> 按索引访问元素
//   3. 用 tie 解包 tuple 到变量
//   4. 用 tuple_size 获取 tuple 元素个数
//
// 提示: #include <tuple>
//       auto t = std::make_tuple(1, "hello", 3.14);
//       std::get<0>(t)  // 第一个元素
//       int a; std::string b; std::tie(a, b) = some_tuple;
//       std::tuple_size<decltype(t)>::value  // 元素个数

#include "cpplings.h"
#include <tuple>
#include <string>

TEST("make_tuple 创建") {
    // TODO: 用 make_tuple 创建 t = (1, 3.14, "hello")
    //       类型应为 tuple<int, double, const char*>

    int _todo_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    ASSERT_EQ(std::get<0>(t), 1);
    ASSERT_EQ(std::get<1>(t), 3.14);
    ASSERT_EQ(std::string(std::get<2>(t)), std::string("hello"));
}

TEST("get 按索引访问") {
    auto t = std::make_tuple(10, std::string("world"), true);

    // TODO: 用 get<0>, get<1>, get<2> 分别取出元素到 first, second, third

    int _todo2_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    ASSERT_EQ(first, 10);
    ASSERT_EQ(second, std::string("world"));
    ASSERT_TRUE(third);
}

TEST("tie 解包") {
    auto t = std::make_tuple(42, std::string("answer"), 99.9);

    int a;
    std::string b;
    double c;

    // TODO: 用 std::tie 将 t 解包到 a, b, c

    int _todo3_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    ASSERT_EQ(a, 42);
    ASSERT_EQ(b, std::string("answer"));
    ASSERT_EQ(c, 99.9);
}

TEST("tuple_size 获取大小") {
    auto t = std::make_tuple(1, 2.0, 'c', std::string("hi"));

    // TODO: 用变量 sz 获取 t 的元素个数
    // 提示: std::tuple_size<decltype(t)>::value

    int _todo4_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    ASSERT_EQ(sz, 4u);
}

TEST("tie 和 ignore") {
    auto t = std::make_tuple(1, 2, 3);

    int first = 0;
    int third = 0;

    // TODO: 用 std::tie 和 std::ignore 只解包第一个和第三个元素
    // 提示: std::tie(first, std::ignore, third) = t;

    int _todo5_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    ASSERT_EQ(first, 1);
    ASSERT_EQ(third, 3);
}

CPPLINGS_MAIN
