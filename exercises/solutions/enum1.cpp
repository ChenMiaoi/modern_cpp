// Solution: enum class
// 将传统 enum 转换为 enum class，修复所有编译错误

#include "cpplings.h"
#include <type_traits>

enum class Color { Red, Green, Blue };
enum class Direction { North = 0, South = 1, East = 2, West = 3 };

TEST("enum class 不会隐式转换为 int") {
    Color c = Color::Red;

    ASSERT_EQ(static_cast<int>(c), 0);
    ASSERT_TRUE(true);
}

TEST("enum class 提供类型安全") {
    Color c = Color::Green;
    Direction d = Direction::North;

    static_assert(!std::is_same<Color, Direction>::value,
                  "Color 和 Direction 应该是不同类型");
    ASSERT_TRUE(true);
}

TEST("enum class 值的正确性") {
    ASSERT_EQ(static_cast<int>(Direction::North), 0);
    ASSERT_EQ(static_cast<int>(Direction::South), 1);
    ASSERT_EQ(static_cast<int>(Direction::East),  2);
    ASSERT_EQ(static_cast<int>(Direction::West),  3);
}

TEST("enum class 在 switch 中使用") {
    Color c = Color::Blue;
    int result = 0;

    switch (c) {
        case Color::Red:   result = 1; break;
        case Color::Green: result = 2; break;
        case Color::Blue:  result = 3; break;
    }

    ASSERT_EQ(result, 3);
}

CPPLINGS_MAIN
