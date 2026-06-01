// Exercise: 属性 (Attributes)
// C++11 引入了标准属性语法 [[attr]]，
// C++17 添加了 [[nodiscard]]，但这些已在编译器中广泛支持。
//
// 任务:
//   1. 使用 [[nodiscard]] 标记函数，使其返回值不可被忽略
//   2. 使用 [[maybe_unused]] 抑制未使用变量/参数的警告
//   3. 使用 [[deprecated("原因")]] 标记弃用的函数
//
// 提示: [[nodiscard]] 用于函数声明，忽略返回值时编译器会警告或报错
//       [[maybe_unused]] 可用于变量、参数、函数
//       [[deprecated("msg")]] 标记函数为弃用，调用时产生警告

#include "cpplings.h"
#include <string>

// TODO 1: 给 compute 函数添加 [[nodiscard]] 属性
//   提示: [[nodiscard]] 放在返回类型前面
int compute(int x) {
    return x * x + 1;
}

// TODO 2: 给 unused_helper 函数添加 [[maybe_unused]] 属性
//   提示: [[maybe_unused]] 放在函数声明前
static void unused_helper() {
    // 此函数可能不被调用，但不应产生警告
}

// TODO 3: 给 old_process 函数添加 [[deprecated("use new_process instead")]] 属性
//   提示: [[deprecated("msg")]] 放在函数声明前
int old_process(int x) {
    return x + 1;
}

int new_process(int x) {
    return x + 2;
}

// 这个函数接受一个可能不使用的参数
// TODO 4: 给参数 b 添加 [[maybe_unused]] 属性
int do_work(int a, int b) {
    return a * 2;
}

int _todo_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

TEST("nodiscard — 返回值必须被使用") {
    // compute 有 [[nodiscard]]，忽略返回值应产生警告/错误
    int result = compute(5);
    ASSERT_EQ(result, 26);  // 5*5+1
}

TEST("nodiscard — 返回值被正确使用") {
    int val = compute(0);
    ASSERT_EQ(val, 1);

    val = compute(10);
    ASSERT_EQ(val, 101);
}

TEST("maybe_unused — 变量不产生警告") {
    [[maybe_unused]] int debug_value = 42;
    // debug_value 未被使用，但 [[maybe_unused]] 抑制了警告

    int used = compute(3);
    ASSERT_EQ(used, 10);
}

TEST("deprecated — 仍然可以调用") {
    // old_process 被标记为 deprecated，但仍然可以编译和调用
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
