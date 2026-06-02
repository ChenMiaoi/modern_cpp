// Solution: nullptr vs NULL
// 将所有 NULL 替换为 nullptr，修复函数重载歧义

#include "cpplings.h"
#include <type_traits>
#include <cstddef>

void process(int val) {
    (void)val;
}

void process(void* ptr) {
    (void)ptr;
}

TEST("nullptr 选择正确的重载") {
    process(nullptr);

    static_assert(!std::is_same<decltype(nullptr), int>::value,
                  "nullptr 不应该是 int 类型");
    static_assert(std::is_same<decltype(nullptr), std::nullptr_t>::value,
                  "nullptr 应该是 std::nullptr_t 类型");
    ASSERT_TRUE(true);
}

TEST("nullptr 与指针比较") {
    int* p = nullptr;
    int* q = nullptr;

    ASSERT_TRUE(p == nullptr);
    ASSERT_TRUE(q == nullptr);
    ASSERT_TRUE(p == q);

    double* dp = nullptr;
    ASSERT_TRUE(dp == nullptr);
}

TEST("nullptr 隐式转换") {
    int* ip = nullptr;
    double* dp = nullptr;
    const char* sp = nullptr;

    ASSERT_TRUE(ip == nullptr);
    ASSERT_TRUE(dp == nullptr);
    ASSERT_TRUE(sp == nullptr);

    static_assert(!std::is_convertible<std::nullptr_t, int>::value,
                  "nullptr 不能转换为 int");
}

CPPLINGS_MAIN
