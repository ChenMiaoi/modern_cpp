// Solution — aggregate1: 聚合初始化增强
#include "cpplings.h"
#include <string>

struct Point {
    double x;
    double y;
};

TEST("designated initializers — basic") {
    Point p{.x = 3.0, .y = 4.0};
    ASSERT_EQ(p.x, 3.0);
    ASSERT_EQ(p.y, 4.0);
}

TEST("designated initializers — partial") {
    Point p{.x = 5.0};
    ASSERT_EQ(p.x, 5.0);
    ASSERT_EQ(p.y, 0.0);
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

TEST("designated initializers — nested") {
    ColoredPoint cp{.pos{.x = 1.0, .y = 2.0}, .color{.r = 255, .g = 0, .b = 0}};
    ASSERT_EQ(cp.pos.x, 1.0);
    ASSERT_EQ(cp.pos.y, 2.0);
    ASSERT_EQ(cp.color.r, 255);
    ASSERT_EQ(cp.color.g, 0);
    ASSERT_EQ(cp.color.b, 0);
}

TEST("parenthesized aggregate init") {
    Point p(1.0, 2.0);
    ASSERT_EQ(p.x, 1.0);
    ASSERT_EQ(p.y, 2.0);
}

struct Config {
    std::string name;
    int width;
    int height;
    bool fullscreen;
};

TEST("designated initializers — mixed types") {
    Config cfg{.name = "window", .width = 800, .height = 600, .fullscreen = false};
    ASSERT_EQ(cfg.name, "window");
    ASSERT_EQ(cfg.width, 800);
    ASSERT_EQ(cfg.height, 600);
    ASSERT_FALSE(cfg.fullscreen);
}

CPPLINGS_MAIN
