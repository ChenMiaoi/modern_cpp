// Exercise: nodiscard1 — [[nodiscard]] 等属性
// 使用 C++17 的新属性: [[nodiscard]]、[[maybe_unused]]、[[fallthrough]]。
//
// 任务:
//   1. 使用 [[nodiscard]] 标记函数，编译器会在返回值被忽略时发出警告
//   2. 使用 [[maybe_unused]] 抑制未使用变量的警告
//   3. 使用 [[fallthrough]] 在 switch-case 中标记有意的穿透
// 提示: 这些属性不影响运行时行为，但帮助编译器检查代码意图

#include "cpplings.h"
#include <string>

// TODO: 给函数添加 [[nodiscard]] 属性
// 提示: [[nodiscard]] int compute(int x) { return x * 2; }
// 如果调用 compute(5); 但不使用返回值，编译器会发出警告

TEST("nodiscard — 函数返回值不可忽略") {
    // TODO: 实现一个 [[nodiscard]] 函数 compute(x) 返回 x * 2
    // 调用它并使用返回值
    int _todo_ = "请删除此行，实现上面的 TODO";
    // int result = compute(21);
    // ASSERT_EQ(result, 42);
}

TEST("nodiscard — 带消息的 nodiscard") {
    // TODO: 实现 [[nodiscard("use the result")]] allocate(size) 返回 size * 8
    int _todo_ = "请删除此行，实现上面的 TODO";
    // int bytes = allocate(100);
    // ASSERT_EQ(bytes, 800);
}

TEST("maybe_unused — 抑制未使用警告") {
    // TODO: 声明一个 [[maybe_unused]] 变量
    // 提示: [[maybe_unused]] int debug_val = 42;
    int _todo_ = "请删除此行，实现上面的 TODO";
    // [[maybe_unused]] int debug_val = 42;
    // // 不使用 debug_val 也不会有编译器警告
    // ASSERT_TRUE(true);  // placeholder
}

TEST("fallthrough — switch 中的有意穿透") {
    int value = 1;
    int result = 0;

    switch (value) {
    case 1:
        result += 10;
        // TODO: 在此添加 [[fallthrough]] 标记有意穿透到 case 2
        [[fallthrough]];
    case 2:
        result += 20;
        break;
    case 3:
        result += 30;
        break;
    }

    // case 1 穿透到 case 2，result 应为 30
    ASSERT_EQ(result, 30);
}

TEST("fallthrough — 无穿透时 case 独立") {
    int value = 2;
    int result = 0;

    switch (value) {
    case 1:
        result += 10;
        break;
    case 2:
        result += 20;
        break;
    case 3:
        result += 30;
        break;
    }

    ASSERT_EQ(result, 20);
}

CPPLINGS_MAIN
