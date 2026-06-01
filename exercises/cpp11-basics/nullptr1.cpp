// Exercise: nullptr vs NULL
// 将所有 NULL 替换为 nullptr，修复函数重载歧义
//
// 任务:
//   1. 理解 NULL 在 C++ 中的本质（通常是整数 0）
//   2. 用 nullptr 替代 NULL，获得类型安全的空指针
//   3. 修复 NULL 导致的函数重载歧义
//
// 提示: nullptr 的类型是 std::nullptr_t，可以隐式转换为任意指针类型，
//       但不会转换为整数类型。这使得重载决议正确选择指针版本。

#include "cpplings.h"
#include <type_traits>
#include <cstddef>

// 以下两个重载函数用于演示 NULL 的歧义问题
void process(int val) {
    (void)val;
}

void process(void* ptr) {
    (void)ptr;
}

TEST("nullptr 选择正确的重载") {
    // TODO: 将下面的 NULL 替换为 nullptr
    // NULL 在大多数实现中是整数 0，会调用 process(int)
    // nullptr 是指针类型，应该调用 process(void*)

    int _todo_ = "请删除此行，将 NULL 替换为 nullptr";  // 编译错误

    process(NULL);  // ← 修改此行：将 NULL 改为 nullptr

    // 验证 nullptr 的类型
    static_assert(!std::is_same_v<decltype(nullptr), int>,
                  "nullptr 不应该是 int 类型");
    static_assert(std::is_same_v<decltype(nullptr), std::nullptr_t>,
                  "nullptr 应该是 std::nullptr_t 类型");
    ASSERT_TRUE(true);
}

TEST("nullptr 与指针比较") {
    // TODO: 将所有 NULL 替换为 nullptr

    int _todo2_ = "请删除此行，替换 NULL 为 nullptr";  // 编译错误

    int* p = nullptr;
    int* q = NULL;      // ← 修改此行：将 NULL 改为 nullptr

    ASSERT_TRUE(p == nullptr);
    ASSERT_TRUE(q == nullptr);
    ASSERT_TRUE(p == q);

    // nullptr 可以和任意指针类型比较
    double* dp = nullptr;
    ASSERT_TRUE(dp == nullptr);
}

TEST("nullptr 隐式转换") {
    // TODO: 用 nullptr 初始化各种指针类型

    int _todo3_ = "请删除此行，用 nullptr 初始化指针";  // 编译错误

    int* ip = nullptr;
    double* dp = nullptr;
    const char* sp = nullptr;

    ASSERT_TRUE(ip == nullptr);
    ASSERT_TRUE(dp == nullptr);
    ASSERT_TRUE(sp == nullptr);

    // nullptr 不能隐式转换为整数
    static_assert(!std::is_convertible_v<std::nullptr_t, int>,
                  "nullptr 不能转换为 int");
}

CPPLINGS_MAIN
