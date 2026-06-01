// cpplings: aggregate1
// Title: 聚合初始化增强
// Description: Use C++20 aggregate initialization features including
//   designated initializers for structs, and understand the expanded
//   aggregate definition (no user-declared constructors needed).
//
// Instructions:
//   1. Use designated initializers to initialize a struct with specific fields.
//   2. Use parenthesized aggregate initialization.
//   3. Demonstrate that aggregates can now have no user-declared constructors
//      (C++20 removed the restriction on default constructors).
//   4. Use designated initializers with nested structs.
//   5. Delete each _todo_ guard after filling in the TODO block.
//
// Hint: Point p{.x = 1, .y = 2};  — designated initializer
//       Point p(1, 2);             — parenthesized (C++20)

#include "cpplings.h"
#include <string>

struct Point {
    double x;
    double y;
};

// TODO: Use designated initializers to create a Point.
//   Point p{.x = 3.0, .y = 4.0};

TEST("designated initializers — basic") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // Point p{.x = 3.0, .y = 4.0};
    // ASSERT_EQ(p.x, 3.0);
    // ASSERT_EQ(p.y, 4.0);
}

// TODO: Designated initializers can skip fields (they get zero-initialized).
//   Point p{.x = 5.0};  // y is 0.0

TEST("designated initializers — partial") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // Point p{.x = 5.0};
    // ASSERT_EQ(p.x, 5.0);
    // ASSERT_EQ(p.y, 0.0);
}

struct Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
};

struct ColoredPoint {
    Point pos;
    Color color;
};

// TODO: Use designated initializers with nested structs.
//   ColoredPoint cp{.pos{.x = 1.0, .y = 2.0}, .color{.r = 255, .g = 0, .b = 0}};

TEST("designated initializers — nested") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // ColoredPoint cp{.pos{.x = 1.0, .y = 2.0}, .color{.r = 255, .g = 0, .b = 0}};
    // ASSERT_EQ(cp.pos.x, 1.0);
    // ASSERT_EQ(cp.pos.y, 2.0);
    // ASSERT_EQ(cp.color.r, 255);
    // ASSERT_EQ(cp.color.g, 0);
    // ASSERT_EQ(cp.color.b, 0);
}

// TODO: C++20 allows parenthesized aggregate initialization.
//   Point p(1.0, 2.0);  // was ill-formed before C++20

TEST("parenthesized aggregate init") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // Point p(1.0, 2.0);
    // ASSERT_EQ(p.x, 1.0);
    // ASSERT_EQ(p.y, 2.0);
}

struct Config {
    std::string name;
    int width;
    int height;
    bool fullscreen;
};

// TODO: Designated initializers with std::string member.
//   Config cfg{.name = "window", .width = 800, .height = 600, .fullscreen = false};

TEST("designated initializers — mixed types") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // Config cfg{.name = "window", .width = 800, .height = 600, .fullscreen = false};
    // ASSERT_EQ(cfg.name, "window");
    // ASSERT_EQ(cfg.width, 800);
    // ASSERT_EQ(cfg.height, 600);
    // ASSERT_FALSE(cfg.fullscreen);
}

CPPLINGS_MAIN
