// cpplings: tmp3
// 主题: 模板元编程 — 两阶段查找 (Two-phase Lookup)
//
// TODO: 理解两阶段查找的区别
//       阶段 1: 模板定义时查找非依赖名
//       阶段 2: 模板实例化时查找依赖名
//
// 提示: 依赖名 (dependent name) 依赖于模板参数
//       非依赖名在模板定义时就必须可见

#include "cpplings.h"
#include <type_traits>
#include <string>

// 演示: 非依赖名在模板定义前必须可见
inline int helper(int x) { return x * 2; }

template <typename T>
int use_helper(T x) {
    // helper 是非依赖名，阶段 1 查找
    return helper(x);
}

// 演示: 依赖名在实例化时查找
struct Special {};
inline int helper(Special) { return 999; }

template <typename T>
int use_dependent(T x) {
    // 这里 helper(x) 中 x 的类型依赖 T，所以是依赖名
    // 阶段 2 查找时会找到 Special 重载
    return helper(x);
}

TEST("非依赖名 — 使用定义时可见的重载") {
    ASSERT_EQ(use_helper(5), 10);
}

TEST("依赖名 — 使用实例化时可见的重载") {
    ASSERT_EQ(use_dependent(Special{}), 999);
}

TEST("依赖名 — 普通类型用原始 helper") {
    ASSERT_EQ(use_dependent(3), 6);
}

TEST("ADL 与两阶段查找") {
    // ADL (Argument-Dependent Lookup) 在阶段 2 也会被考虑
    // 这里 std::string 在 namespace std 中，ADL 会找到 std::to_string
    std::string s = "hello";
    ASSERT_EQ(s.size(), 5u);
}

CPPLINGS_MAIN
