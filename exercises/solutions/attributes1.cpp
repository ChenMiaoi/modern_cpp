// Solution: 属性 (Attributes)
// C++11 引入了标准属性语法 [[attr]]，
// C++17 添加了 [[nodiscard]]，但这些已在编译器中广泛支持。

#include "cpplings.h"
#include <string>

[[nodiscard]] int compute(int x) {
    return x * x + 1;
}

[[maybe_unused]] static void unused_helper() {
}

[[deprecated("use new_process instead")]] int old_process(int x) {
    return x + 1;
}

int new_process(int x) {
    return x + 2;
}

int do_work(int a, [[maybe_unused]] int b) {
    return a * 2;
}

TEST("nodiscard — 返回值必须被使用") {
    int result = compute(5);
    ASSERT_EQ(result, 26);
}

TEST("nodiscard — 返回值被正确使用") {
    int val = compute(0);
    ASSERT_EQ(val, 1);

    val = compute(10);
    ASSERT_EQ(val, 101);
}

TEST("maybe_unused — 变量不产生警告") {
    [[maybe_unused]] int debug_value = 42;

    int used = compute(3);
    ASSERT_EQ(used, 10);
}

TEST("deprecated — 仍然可以调用") {
    int result = old_process(5);
    ASSERT_EQ(result, 6);

    int result2 = new_process(5);
    ASSERT_EQ(result2, 7);
}

TEST("do_work 接受 maybe_unused 参数") {
    int result = do_work(3, 0);
    ASSERT_EQ(result, 6);
}

CPPLINGS_MAIN
