// Solution: if constexpr

#include "cpplings.h"
#include <string>
#include <sstream>
#include <type_traits>
#include <vector>

template <typename T>
std::string serialize(const T& value) {
    if constexpr (std::is_same_v<T, bool>) {
        return value ? "bool:true" : "bool:false";
    } else if constexpr (std::is_same_v<T, int>) {
        return "int:" + std::to_string(value);
    } else if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) {
        return "float:" + std::to_string(value);
    } else if constexpr (std::is_same_v<T, std::string>) {
        return "str:\"" + value + "\"";
    } else {
        return "unknown";
    }
}

TEST("serialize — int") {
    ASSERT_EQ(serialize(42), "int:42");
    ASSERT_EQ(serialize(-1), "int:-1");
    ASSERT_EQ(serialize(0), "int:0");
}

TEST("serialize — float") {
    auto result = serialize(3.14);
    ASSERT_TRUE(result.find("float:") == 0);
}

TEST("serialize — string") {
    ASSERT_EQ(serialize(std::string("hello")), "str:\"hello\"");
}

TEST("serialize — bool") {
    ASSERT_EQ(serialize(true), "bool:true");
    ASSERT_EQ(serialize(false), "bool:false");
}

template <typename T, typename = void>
struct has_size : std::false_type {};

template <typename T>
struct has_size<T, std::void_t<decltype(std::declval<T>().size())>> : std::true_type {};

template <typename Container>
std::size_t get_size_or_zero(const Container& c) {
    if constexpr (has_size<Container>::value) {
        return c.size();
    } else {
        return 0;
    }
}

TEST("get_size_or_zero — with size()") {
    std::vector<int> v = {1, 2, 3};
    ASSERT_EQ(get_size_or_zero(v), 3u);
}

TEST("get_size_or_zero — without size()") {
    int x = 42;
    ASSERT_EQ(get_size_or_zero(x), 0u);
}

template <typename T>
T sum_all(T val) {
    return val;
}

template <typename T, typename... Rest>
auto sum_all(T first, Rest... rest) {
    if constexpr (sizeof...(rest) == 0) {
        return first;
    } else {
        return first + sum_all(rest...);
    }
}

TEST("sum_all") {
    ASSERT_EQ(sum_all(1, 2, 3), 6);
    ASSERT_EQ(sum_all(100), 100);
    ASSERT_EQ(sum_all(1, 2, 3, 4, 5), 15);
}

CPPLINGS_MAIN
