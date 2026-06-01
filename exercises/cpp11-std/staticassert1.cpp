// Exercise: static_assert 编译期断言
// static_assert 在编译期检查条件，失败则产生编译错误。
// C++11 要求提供字符串消息，C++17 可省略。
//
// 任务:
//   1. 用 static_assert 确保 int 大小为 4 字节
//   2. 用 static_assert 确保 sizeof(int) <= sizeof(long)
//   3. 实现一个函数模板 checked_add，用 static_assert 限制模板参数为整数类型
//      (使用 std::is_integral<T>::value)
//   4. 用 static_assert 确保两个模板参数类型相同
//
// 提示: static_assert(constexpr条件, "消息");
//       std::is_integral<T>::value 在 T 为整数类型时为 true

#include "cpplings.h"
#include <type_traits>

// TODO 1: 在此处添加 static_assert，验证 sizeof(int) == 4

// TODO 2: 在此处添加 static_assert，验证 sizeof(int) <= sizeof(long)

// TODO 3: 实现函数模板 checked_add
//   template <typename T>
//   T checked_add(T a, T b) {
//       static_assert(std::is_integral<T>::value, "T must be integral");
//       return a + b;
//   }

int _todo_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

// TODO 4: 实现函数模板 same_type_add
//   template <typename T, typename U>
//   T same_type_add(T a, U b) {
//       static_assert(std::is_same<T, U>::value, "types must match");
//       return a + b;
//   }

int _todo2_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

TEST("static_assert on sizeof(int)") {
    // 如果编译通过，说明 sizeof(int) == 4 的 static_assert 成功
    ASSERT_EQ(sizeof(int), 4u);
}

TEST("static_assert on sizeof(int) <= sizeof(long)") {
    // 如果编译通过，说明大小关系的 static_assert 成功
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
    // same_type_add(1, 2.0) 应该编译失败 — 类型不同
}

CPPLINGS_MAIN
