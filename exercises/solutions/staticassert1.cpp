// Solution: static_assert 编译期断言
// static_assert 在编译期检查条件，失败则产生编译错误。

#include "cpplings.h"
#include <type_traits>

static_assert(sizeof(int) == 4, "int must be 4 bytes");
static_assert(sizeof(int) <= sizeof(long), "int must fit in long");

template <typename T>
T checked_add(T a, T b) {
    static_assert(std::is_integral<T>::value, "T must be integral");
    return a + b;
}

template <typename T, typename U>
T same_type_add(T a, U b) {
    static_assert(std::is_same<T, U>::value, "types must match");
    return a + b;
}

TEST("static_assert on sizeof(int)") {
    ASSERT_EQ(sizeof(int), 4u);
}

TEST("static_assert on sizeof(int) <= sizeof(long)") {
    ASSERT_TRUE(sizeof(int) <= sizeof(long));
}

TEST("checked_add 仅接受整数类型") {
    ASSERT_EQ(checked_add(3, 4), 7);
    ASSERT_EQ(checked_add(100L, 200L), 300L);
    ASSERT_EQ(checked_add(short(1), short(2)), short(3));
}

TEST("same_type_add 要求相同类型") {
    ASSERT_EQ(same_type_add(1, 2), 3);
    ASSERT_EQ(same_type_add(1.5, 2.5), 4.0);
}

CPPLINGS_MAIN
