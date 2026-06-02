// cpplings: typelist1
// 主题: 模板元编程 — type_list 基础操作
//
// TODO: 实现 type_list 的 push_front, size, at 操作
//
// 提示: type_list 是编译期类型容器，所有操作都是类型级的

#include "cpplings.h"
#include <type_traits>
#include <cstddef>

// type_list 定义
template <typename... Ts>
struct type_list {};

// TODO: push_front — 在 type_list 前面加一个类型
template <typename List, typename T>
struct push_front;

// 特化: push_front<type_list<Ts...>, T> = type_list<T, Ts...>
// 请实现
template <typename... Ts, typename T>
struct push_front<type_list<Ts...>, T> { using type = type_list<T, Ts...>; };

// TODO: size — 返回 type_list 中类型的数量
template <typename List>
struct size;

// 特化: size<type_list<Ts...>> = sizeof...(Ts)
// 请实现
template <typename... Ts>
struct size<type_list<Ts...>> : std::integral_constant<std::size_t, sizeof...(Ts)> {};

// TODO: at — 获取第 N 个类型
template <typename List, std::size_t N>
struct at;

// 提示: 递归特化 + 继承
// at<type_list<T0, T1, ...>, 0> = T0
// at<type_list<T0, T1, ...>, N> = at<type_list<T1, ...>, N-1>
// 请实现
template <typename T0, typename... Ts>
struct at<type_list<T0, Ts...>, 0> { using type = T0; };

template <typename T0, typename... Ts, std::size_t N>
struct at<type_list<T0, Ts...>, N> { using type = typename at<type_list<Ts...>, N-1>::type; };

TEST("push_front") {
    using List = type_list<int, double>;
    using Result = push_front<List, char>::type;
    static_assert(std::is_same<Result, type_list<char, int, double>>::value, "");
}

TEST("size") {
    using List = type_list<int, double, char, float>;
    static_assert(size<List>::value == 4, "");
}

TEST("at — 第 0 个") {
    using List = type_list<int, double, char>;
    static_assert(std::is_same<at<List, 0>::type, int>::value, "");
}

TEST("at — 第 1 个") {
    using List = type_list<int, double, char>;
    static_assert(std::is_same<at<List, 1>::type, double>::value, "");
}

TEST("at — 第 2 个") {
    using List = type_list<int, double, char>;
    static_assert(std::is_same<at<List, 2>::type, char>::value, "");
}

CPPLINGS_MAIN
