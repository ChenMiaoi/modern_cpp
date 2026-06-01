// Exercise: 用户定义字面量
// 实现 operator""_km 和 operator""_m 用户定义字面量
//
// 任务:
//   1. 实现 _km 字面量：1.5_km → 1500.0 米
//   2. 实现 _m 字面量（浮点版）：500.0_m → 500.0 米
//   3. 实现 _m 字面量（整数版）：250_m → 250.0 米
//
// 提示: 用户定义字面量运算符必须是 constexpr。
//       运算符参数类型决定了匹配哪种字面量：
//         long double   → 浮点数字面量
//         unsigned long long → 整数字面量

#include "cpplings.h"

// 距离类型：内部以米为单位存储
struct Distance {
    double meters;

    constexpr Distance(double m) : meters(m) {}

    constexpr double to_km() const { return meters / 1000.0; }
    constexpr double to_m() const { return meters; }
};

// TODO: 实现用户定义字面量运算符
//
// _km 接收 long double 参数，返回以米为单位的 Distance
// 例如: 1.5_km → Distance(1500.0)
constexpr Distance operator""_km(long double km) {
    int _todo_ = "请删除此行，实现 _km 运算符";  // 编译错误
    (void)km;
    return Distance(static_cast<double>(_todo_));
}

// TODO: _m 接收 long double 参数（浮点数字面量）
// 例如: 500.0_m → Distance(500.0)
constexpr Distance operator""_m(long double m) {
    int _todo2_ = "请删除此行，实现 _m(long double) 运算符";  // 编译错误
    (void)m;
    return Distance(static_cast<double>(_todo2_));
}

// TODO: _m 接收 unsigned long long 参数（整数字面量）
// 例如: 250_m → Distance(250.0)
constexpr Distance operator""_m(unsigned long long m) {
    int _todo3_ = "请删除此行，实现 _m(unsigned long long) 运算符";  // 编译错误
    (void)m;
    return Distance(static_cast<double>(_todo3_));
}

TEST("km 字面量转换") {
    auto d = 1.5_km;
    ASSERT_EQ(d.to_m(), 1500.0);
    ASSERT_EQ(d.to_km(), 1.5);
}

TEST("m 字面量转换") {
    auto d = 500.0_m;
    ASSERT_EQ(d.to_m(), 500.0);
    ASSERT_EQ(d.to_km(), 0.5);
}

TEST("整数 m 字面量") {
    auto d = 250_m;
    ASSERT_EQ(d.to_m(), 250.0);
}

TEST("字面量可以用于 constexpr") {
    constexpr auto d = 2.5_km;
    static_assert(d.to_m() == 2500.0, "2.5km = 2500m");
    ASSERT_EQ(d.to_m(), 2500.0);
}

CPPLINGS_MAIN
