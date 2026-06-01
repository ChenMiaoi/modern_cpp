// Solution: 委托构造函数 (Delegating Constructors)
// C++11 允许一个构造函数调用同类的另一个构造函数，
// 避免重复的初始化代码。

#include "cpplings.h"

struct Point {
    int x, y, z;
    Point(int x, int y, int z) : x(x), y(y), z(z) {}
    Point(int x) : Point(x, 0, 0) {}
    Point() : Point(0, 0, 0) {}
};

TEST("默认构造函数委托到 (0,0,0)") {
    Point p;
    ASSERT_EQ(p.x, 0);
    ASSERT_EQ(p.y, 0);
    ASSERT_EQ(p.z, 0);
}

TEST("单参数构造函数委托到 (x,0,0)") {
    Point p(42);
    ASSERT_EQ(p.x, 42);
    ASSERT_EQ(p.y, 0);
    ASSERT_EQ(p.z, 0);
}

TEST("三参数构造函数直接初始化") {
    Point p(1, 2, 3);
    ASSERT_EQ(p.x, 1);
    ASSERT_EQ(p.y, 2);
    ASSERT_EQ(p.z, 3);
}

CPPLINGS_MAIN
