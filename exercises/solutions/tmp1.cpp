// cpplings: tmp1 — 解答
// 主题: 模板元编程 — type_list, SFINAE, void_t 检测

#include "cpplings.h"
#include <type_traits>
#include <string>
#include <vector>

// === type_list ===

template <typename... Ts>
struct type_list {
    static constexpr std::size_t size = sizeof...(Ts);
};

// push_back
template <typename List, typename T>
struct push_back;

template <typename... Ts, typename T>
struct push_back<type_list<Ts...>, T> {
    using type = type_list<Ts..., T>;
};

// index_of 前向声明
template <typename List, typename T>
struct index_of;

// index_of — 基础情况: 头部匹配
template <typename T, typename... Rest>
struct index_of<type_list<T, Rest...>, T> {
    static constexpr int value = 0;
};

// index_of — 递归情况
template <typename Head, typename... Rest, typename T>
struct index_of<type_list<Head, Rest...>, T> {
    static constexpr int value = 1 + index_of<type_list<Rest...>, T>::value;
};

// at_index 前向声明
template <typename List, std::size_t I>
struct at_index;

// at_index — 基础情况: 索引 0
template <typename T, typename... Rest>
struct at_index<type_list<T, Rest...>, 0> {
    using type = T;
};

// at_index — 递归情况
template <typename Head, typename... Rest, std::size_t I>
struct at_index<type_list<Head, Rest...>, I> {
    using type = typename at_index<type_list<Rest...>, I - 1>::type;
};

// === void_t 检测 ===

template <typename T, typename = void>
struct has_size : std::false_type {};

template <typename T>
struct has_size<T, std::void_t<decltype(std::declval<T>().size())>> : std::true_type {};

template <typename T, typename = void>
struct has_value_type : std::false_type {};

template <typename T>
struct has_value_type<T, std::void_t<typename T::value_type>> : std::true_type {};

// === 变量模板 ===
template <typename List, typename T>
inline constexpr int index_of_v = index_of<List, T>::value;

template <typename List, typename T>
using push_back_t = typename push_back<List, T>::type;

template <typename List, std::size_t I>
using at_index_t = typename at_index<List, I>::type;

TEST("type_list size") {
    using list = type_list<int, double, char>;
    static_assert(list::size == 3, "三个类型");
    ASSERT_EQ(list::size, 3u);
}

TEST("push_back 添加类型") {
    using list = type_list<int, double>;
    using extended = push_back_t<list, char>;
    static_assert(extended::size == 3u, "应有3个类型");
    ASSERT_EQ(extended::size, 3u);
}

TEST("index_of 查找索引") {
    using list = type_list<int, double, char, float>;
    static_assert(index_of_v<list, int> == 0, "int 在索引 0");
    static_assert(index_of_v<list, double> == 1, "double 在索引 1");
    static_assert(index_of_v<list, char> == 2, "char 在索引 2");
    static_assert(index_of_v<list, float> == 3, "float 在索引 3");
    ASSERT_EQ((index_of_v<list, int>), 0);
    ASSERT_EQ((index_of_v<list, char>), 2);
}

TEST("at_index 获取类型") {
    using list = type_list<int, double, char>;
    static_assert(std::is_same_v<at_index_t<list, 0>, int>);
    static_assert(std::is_same_v<at_index_t<list, 1>, double>);
    static_assert(std::is_same_v<at_index_t<list, 2>, char>);
    ASSERT_TRUE(true);
}

TEST("has_size 检测 size() 成员") {
    ASSERT_TRUE((has_size<std::string>::value));
    ASSERT_TRUE((has_size<std::vector<int>>::value));
    ASSERT_FALSE((has_size<int>::value));
}

TEST("has_value_type 检测 value_type 成员类型") {
    ASSERT_TRUE((has_value_type<std::vector<int>>::value));
    ASSERT_TRUE((has_value_type<std::string>::value));
    ASSERT_FALSE((has_value_type<int>::value));
}

CPPLINGS_MAIN
