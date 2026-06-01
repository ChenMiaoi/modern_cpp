// cpplings: sfinae1
// 主题: C++11 模板与泛型 — SFINAE 检测成员
//
// 练习：使用 SFINAE 和 std::void_t 检测类型是否具有特定成员
// 删除每个 TODO 区域的 _todo_ 行，实现特征类和函数
// 让所有断言通过

#include "cpplings.h"
#include <type_traits>
#include <vector>
#include <string>

// 示例类型（用于测试，不要修改）
struct HasSize {
    std::size_t size() const { return 42; }
};

struct HasNoSize {
    int value = 0;
};

// TODO: 实现 has_size — 检测类型是否有 size() 成员
// 1. 主模板：template <typename T, typename = void> struct has_size : std::false_type {};
// 2. 偏特化：template <typename T> struct has_size<T, std::void_t<...>> : std::true_type {};
//    其中 ... 检测 decltype(std::declval<T>().size())
int _todo_ = "FILL IN";

TEST("has_size 检测有 size() 的类型") {
    static_assert(has_size<std::vector<int>>::value, "vector 有 size()");
    static_assert(has_size<std::string>::value, "string 有 size()");
    static_assert(has_size<HasSize>::value, "HasSize 有 size()");

    ASSERT_TRUE(has_size<std::vector<int>>::value);
    ASSERT_TRUE(has_size<std::string>::value);
    ASSERT_TRUE(has_size<HasSize>::value);
}

TEST("has_size 检测没有 size() 的类型") {
    static_assert(!has_size<int>::value, "int 没有 size()");
    static_assert(!has_size<double>::value, "double 没有 size()");
    static_assert(!has_size<HasNoSize>::value, "HasNoSize 没有 size()");

    ASSERT_FALSE(has_size<int>::value);
    ASSERT_FALSE(has_size<double>::value);
    ASSERT_FALSE(has_size<HasNoSize>::value);
}

// TODO: 实现 has_begin — 检测类型是否有 begin() 成员
// 同样的 void_t 技巧，检测 decltype(std::declval<T>().begin())
int _todo_ = "FILL IN";

TEST("has_begin 检测容器") {
    static_assert(has_begin<std::vector<int>>::value, "vector 有 begin()");
    static_assert(has_begin<std::string>::value, "string 有 begin()");
    static_assert(!has_begin<int>::value, "int 没有 begin()");

    ASSERT_TRUE(has_begin<std::vector<int>>::value);
    ASSERT_FALSE(has_begin<int>::value);
}

// TODO: 实现 safe_size — SFINAE 条件启用函数重载
// 两个重载，返回类型为 std::size_t：
//   1. std::enable_if_t<has_size<T>::value, std::size_t>  → return container.size();
//   2. std::enable_if_t<!has_size<T>::value, std::size_t> → return 0;
int _todo_ = "FILL IN";

TEST("SFINAE 条件重载") {
    std::vector<int> v = {1, 2, 3};
    int x = 42;

    ASSERT_EQ(safe_size(v), 3u);
    ASSERT_EQ(safe_size(x), 0u);
}

CPPLINGS_MAIN
