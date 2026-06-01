// Exercise: 委托构造函数 (Delegating Constructors)
// C++11 允许一个构造函数调用同类的另一个构造函数，
// 避免重复的初始化代码。
//
// 任务:
//   1. 实现 Point 类，包含 x, y, z 三个 int 成员
//   2. 实现三参数构造函数 Point(int x, int y, int z) 完成初始化
//   3. 实现单参数构造函数委托到三参数版本: Point(int x) : Point(x, 0, 0)
//   4. 实现默认构造函数委托到三参数版本: Point() : Point(0, 0, 0)
//
// 提示: 委托构造函数语法为 构造函数() : 类名(参数...) { }

#include "cpplings.h"

// TODO: 实现 Point 类
//   - 三个 public int 成员: x, y, z
//   - Point(int x, int y, int z) — 用初始化列表设置 x, y, z
//   - Point(int x) — 委托到 Point(x, 0, 0)
//   - Point() — 委托到 Point(0, 0, 0)
//
// 注意: 把成员声明为 public，方便测试直接访问。

int _todo_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

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
