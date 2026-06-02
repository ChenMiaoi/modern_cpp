// Solution: tuple 高级操作
// C++11 的 std::tuple 提供了丰富的编译期操作，
// 包括拼接、转发、遍历和类型查询。

#include "cpplings.h"
#include <tuple>
#include <string>
#include <type_traits>
#include <sstream>
#include <cstddef>
#include <functional>
#include <utility>

// C++11 手动实现 index_sequence
template <std::size_t... Is>
struct index_sequence {};

template <std::size_t N, std::size_t... Is>
struct make_index_sequence : make_index_sequence<N - 1, N - 1, Is...> {};

template <std::size_t... Is>
struct make_index_sequence<0, Is...> {
    using type = index_sequence<Is...>;
};

// tuple_cat 拼接
std::tuple<int, double, char, std::string> tuple_cat_result() {
    std::tuple<int, double> t1 = std::make_tuple(1, 3.14);
    std::tuple<char, std::string> t2 = std::make_tuple('a', std::string("hello"));
    return std::tuple_cat(t1, t2);
}

// forward_as_tuple 保持值类别
template <typename... Args>
auto make_ref_tuple(Args&&... args)
    -> decltype(std::forward_as_tuple(std::forward<Args>(args)...))
{
    return std::forward_as_tuple(std::forward<Args>(args)...);
}

// for_each_in_tuple 辅助实现
template <typename Tuple, typename F, std::size_t... Is>
void for_each_impl(Tuple&& t, F&& func, index_sequence<Is...>) {
    using swallow = int[];
    (void)swallow{0, (func(std::get<Is>(std::forward<Tuple>(t))), 0)...};
}

// for_each_in_tuple — 遍历 tuple 中的每个元素
template <typename Tuple, typename F>
void for_each_in_tuple(Tuple&& t, F&& func) {
    for_each_impl(std::forward<Tuple>(t),
                  std::forward<F>(func),
                  typename make_index_sequence<
                      std::tuple_size<typename std::decay<Tuple>::type>::value
                  >::type());
}

// get_type_name_at_0 — 利用 tuple_element 获取类型名
std::string get_type_name_at_0() {
    using T = std::tuple<int, double, std::string>;
    using Elem = std::tuple_element<0, T>::type;
    if (std::is_same<Elem, int>::value) return "int";
    if (std::is_same<Elem, double>::value) return "double";
    if (std::is_same<Elem, std::string>::value) return "string";
    return "unknown";
}

// --- Tests ---

TEST("tuple_cat 拼接") {
    auto result = tuple_cat_result();

    ASSERT_EQ(std::get<0>(result), 1);
    ASSERT_EQ(std::get<1>(result), 3.14);
    ASSERT_EQ(std::get<2>(result), 'a');
    ASSERT_EQ(std::get<3>(result), std::string("hello"));

    ASSERT_EQ(std::tuple_size<decltype(result)>::value, 4u);
}

TEST("forward_as_tuple 保持左值引用") {
    int x = 42;
    std::string s = "hello";

    auto ref_t = make_ref_tuple(x, s);

    x = 100;
    s = "world";

    ASSERT_EQ(std::get<0>(ref_t), 100);
    ASSERT_EQ(std::get<1>(ref_t), std::string("world"));

    bool holds_ref = std::is_lvalue_reference<
        std::tuple_element<0, decltype(ref_t)>::type
    >::value;
    ASSERT_TRUE(holds_ref);
}

TEST("forward_as_tuple 保持右值引用") {
    auto ref_t = make_ref_tuple(42, std::string("temp"));

    bool elem0_is_rref = std::is_rvalue_reference<
        std::tuple_element<0, decltype(ref_t)>::type
    >::value;
    ASSERT_TRUE(elem0_is_rref);
}

TEST("for_each_in_tuple 遍历求和") {
    auto t = std::make_tuple(1, 2, 3, 4, 5);
    int sum = 0;

    for_each_in_tuple(t, [&sum](int val) {
        sum += val;
    });

    ASSERT_EQ(sum, 15);
}

TEST("for_each_in_tuple 字符串拼接") {
    auto t = std::make_tuple(
        std::string("a"),
        std::string("b"),
        std::string("c")
    );
    std::string result;

    for_each_in_tuple(t, [&result](const std::string& s) {
        result += s;
    });

    ASSERT_EQ(result, std::string("abc"));
}

TEST("tuple_element 获取类型 — 静态断言") {
    using T = std::tuple<int, double, std::string>;

    static_assert(std::is_same<std::tuple_element<0, T>::type, int>::value,
                  "element 0 should be int");
    static_assert(std::is_same<std::tuple_element<1, T>::type, double>::value,
                  "element 1 should be double");
    static_assert(std::is_same<std::tuple_element<2, T>::type, std::string>::value,
                  "element 2 should be std::string");

    std::string name = get_type_name_at_0();
    ASSERT_EQ(name, std::string("int"));
}

TEST("tie 与 ignore 部分解包") {
    auto t = std::make_tuple(1, std::string("hello"), 3.14, true);

    int first;
    double third;
    std::tie(first, std::ignore, third, std::ignore) = t;

    ASSERT_EQ(first, 1);
    ASSERT_EQ(third, 3.14);
}

TEST("make_tuple decay 行为") {
    int arr[] = {1, 2, 3};
    auto t1 = std::make_tuple(arr);
    bool is_pointer = std::is_pointer<
        std::tuple_element<0, decltype(t1)>::type
    >::value;
    ASSERT_TRUE(is_pointer);

    int x = 42;
    auto t2 = std::make_tuple(std::ref(x));
    std::get<0>(t2) = 99;
    ASSERT_EQ(x, 99);
}

CPPLINGS_MAIN
