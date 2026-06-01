// cpplings: tmp1
// 主题: 模板元编程 — type_list, SFINAE, void_t 检测
//
// TODO: 实现编译期类型列表操作和类型特征检测
//
// 提示: type_list 是模板参数包的包装
//       用递归模板特化实现 index_of
//       用 void_t 实现 SFINAE 检测

#include "cpplings.h"
#include <type_traits>
#include <string>

int _todo_ = "请删除此行，实现所有 TODO";  // 编译错误：类型不匹配

// === type_list 基础设施 ===

template <typename... Ts>
struct type_list {
    static constexpr std::size_t size = sizeof...(Ts);
};

// TODO: push_back — 在 type_list 尾部添加类型
// push_back<type_list<A,B>, C> => type_list<A,B,C>
template <typename List, typename T>
struct push_back;

template <typename... Ts, typename T>
struct push_back<type_list<Ts...>, T> {
    // TODO: 实现 type_list<Ts..., T>
    using type = type_list</* FILL IN */>;
};

// TODO: index_of — 查找类型在 type_list 中的索引
// index_of<type_list<A,B,C>, B> => 1
// 约束: 类型必须在列表中
template <typename List, typename T>
struct index_of;

// 提示: 用递归实现，基础情况是头部匹配
// template <typename T, typename... Rest>
// struct index_of<type_list<T, Rest...>, T> { static constexpr int value = 0; };
//
// template <typename Head, typename... Rest, typename T>
// struct index_of<type_list<Head, Rest...>, T> {
//     static constexpr int value = 1 + index_of<type_list<Rest...>, T>::value;
// };

// TODO: at_index — 获取 type_list 中指定索引处的类型
// at_index<type_list<A,B,C>, 1> => B
template <typename List, std::size_t I>
struct at_index;

// === void_t SFINAE 检测 ===

// TODO: 用 void_t 检测类型是否有 size() 成员函数
// has_size<T>::value 应为 true 当且仅当 T 有 size() 方法
template <typename T, typename = void>
struct has_size : std::false_type {};

template <typename T>
struct has_size<T, std::void_t<decltype(std::declval<T>().size())>> : std::true_type {
    // TODO: 填入 void_t 检测表达式
};

// TODO: 用 void_t 检测类型是否有 value_type 成员类型
template <typename T, typename = void>
struct has_value_type : std::false_type {};

template <typename T>
struct has_value_type<T, std::void_t<typename T::value_type>> : std::true_type {
    // TODO: 填入 void_t 检测表达式
};

// === 辅助变量模板 ===
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
