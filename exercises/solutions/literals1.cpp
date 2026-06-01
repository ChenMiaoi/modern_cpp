// Solution: 用户定义字面量
// 实现 operator""_km 和 operator""_m 用户定义字面量

#include "cpplings.h"

struct Distance {
    double meters;

    constexpr Distance(double m) : meters(m) {}

    constexpr double to_km() const { return meters / 1000.0; }
    constexpr double to_m() const { return meters; }
};

constexpr Distance operator""_km(long double km) {
    return Distance(static_cast<double>(km * 1000.0));
}

constexpr Distance operator""_m(long double m) {
    return Distance(static_cast<double>(m));
}

constexpr Distance operator""_m(unsigned long long m) {
    return Distance(static_cast<double>(m));
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
