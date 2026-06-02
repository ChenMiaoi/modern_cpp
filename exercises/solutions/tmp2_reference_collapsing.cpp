// cpplings: tmp2
// 主题: 模板元编程 — 引用折叠 (Reference Collapsing)
//
// TODO: 理解 C++ 引用折叠规则
//       T& & -> T&
//       T& && -> T&
//       T&& & -> T&
//       T&& && -> T&&
//
// 提示: 引用折叠只发生在模板参数推导、auto、typedef、decltype 中

#include "cpplings.h"
#include <type_traits>

template <typename T>
struct identity { using type = T; };

// TODO: 使用引用折叠推导类型
// 当 T = int& 时，T&& = int& && = int& (引用折叠)
// 当 T = int 时，T&& = int&&
template <typename T>
void ref_collapser(T&& arg) {
    // 不需要实现，只需要编译通过
    (void)arg;
}

TEST("左值引用 + 左值引用 = 左值引用") {
    using T = int&;
    using Result = T&;  // int& & -> int&
    static_assert(std::is_same<Result, int&>::value, "");
}

TEST("右值引用 + 左值引用 = 左值引用") {
    using T = int&&;
    using Result = T&;  // int&& & -> int&
    static_assert(std::is_same<Result, int&>::value, "");
}

TEST("左值引用 + 右值引用 = 左值引用") {
    using T = int&;
    using Result = T&&;  // int& && -> int&
    static_assert(std::is_same<Result, int&>::value, "");
}

TEST("右值引用 + 右值引用 = 右值引用") {
    using T = int&&;
    using Result = T&&;  // int&& && -> int&&
    static_assert(std::is_same<Result, int&&>::value, "");
}

TEST("完美转发参数推导 — 左值") {
    int x = 42;
    ref_collapser(x);  // T = int&, arg type = int&
    // 编译通过即验证
    static_assert(std::is_same<decltype(x), int>::value, "");
}

TEST("完美转发参数推导 — 右值") {
    ref_collapser(42);  // T = int, arg type = int&&
    // 编译通过即验证
}

CPPLINGS_MAIN
