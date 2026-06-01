// Exercise: enum class
// 将传统 enum 转换为 enum class，修复所有编译错误
//
// 任务:
//   1. 将传统 enum 转换为 enum class
//   2. 为枚举值添加作用域前缀（Color::Red 等）
//   3. 理解 enum class 的类型安全优势
//
// 提示: enum class 不会隐式转换为 int，不会污染外层命名空间，
//       不同 enum class 类型之间不能直接比较。

#include "cpplings.h"
#include <type_traits>

// TODO: 将以下传统 enum 转换为 enum class
// 提示: 在 enum 后面加上 class 关键字
enum Color { Red, Green, Blue };
enum Direction { North = 0, South = 1, East = 2, West = 3 };

int _todo_ = "请删除此行，将 enum 改为 enum class";  // 编译错误

TEST("enum class 不会隐式转换为 int") {
    // TODO: 改为 enum class 后，需要使用 Color::Red 而非 Red
    Color c = Red;  // ← 修改: Color c = Color::Red;

    // TODO: enum class 需要用 static_cast 转换为 int
    // 修改下面的比较，用 static_cast<int>(c) 替代 c
    ASSERT_EQ(c, 0);  // ← 修改: ASSERT_EQ(static_cast<int>(c), 0);
    ASSERT_TRUE(true);
}

TEST("enum class 提供类型安全") {
    // TODO: 改为 enum class 后，使用 Color::Green 和 Direction::North
    Color c = Green;       // ← 修改: Color c = Color::Green;
    Direction d = North;   // ← 修改: Direction d = Direction::North;

    // 不同 enum class 类型不能直接比较（这是好事！）
    static_assert(!std::is_same_v<Color, Direction>,
                  "Color 和 Direction 应该是不同类型");
    ASSERT_TRUE(true);
}

TEST("enum class 值的正确性") {
    // TODO: 使用 Direction::North 等带作用域前缀的形式
    // 并用 static_cast<int>() 获取底层值
    ASSERT_EQ(static_cast<int>(Direction::North), 0);
    ASSERT_EQ(static_cast<int>(Direction::South), 1);
    ASSERT_EQ(static_cast<int>(Direction::East),  2);
    ASSERT_EQ(static_cast<int>(Direction::West),  3);
}

TEST("enum class 在 switch 中使用") {
    // TODO: 使用 Color::Blue 带作用域前缀
    Color c = Blue;  // ← 修改: Color c = Color::Blue;
    int result = 0;

    switch (c) {
        case Color::Red:   result = 1; break;
        case Color::Green: result = 2; break;
        case Color::Blue:  result = 3; break;
    }

    ASSERT_EQ(result, 3);
}

CPPLINGS_MAIN
