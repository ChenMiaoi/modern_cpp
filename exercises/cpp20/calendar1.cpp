// cpplings: calendar1
// 主题: C++20 — 日历与时间区
//
// TODO: 使用 C++20 chrono 日历类型处理日期
//   - year / month / day 类型
//   - year_month_day 构造（operator/）
//   - sys_days 作为日期与 time_point 的桥梁
//   - 星期几计算
//   - 日期算术
//
// 提示: auto ymd = 2024y / 6 / 15;  — 构造日期
//       auto wd = std::chrono::weekday(ymd); — 获取星期
//       auto dp = sys_days(ymd); — 转为天精度 time_point

#include "cpplings.h"

#if __cpp_lib_chrono >= 201907L
#include <chrono>
#include <string>

namespace chrono = std::chrono;

// TODO: 用 operator/ 构造 year_month_day
//   chrono::year_month_day make_date(int y, unsigned m, unsigned d) {
//       return chrono::year / m / d;  // 用 year_month_day literal
//   }

TEST("calendar — 构造日期 2024-01-15") {
    int _todo_ = "请删除此行，实现上面的 TODO";
    // auto ymd = chrono::year(2024) / 1 / 15;
    // ASSERT_TRUE(ymd.ok());
    // ASSERT_EQ((int)ymd.year(), 2024);
    // ASSERT_EQ((unsigned)ymd.month(), 1u);
    // ASSERT_EQ((unsigned)ymd.day(), 15u);
}

// TODO: 计算星期几
//   std::string day_of_week(chrono::year_month_day ymd) {
//       auto wd = chrono::weekday(chrono::sys_days(ymd));
//       // 返回中文或英文星期名
//   }

TEST("calendar — 2024-01-01 是星期一") {
    int _todo_ = "请删除此行，实现上面的 TODO";
    // auto ymd = chrono::year(2024) / 1 / 1;
    // auto wd = chrono::weekday(chrono::sys_days(ymd));
    // ASSERT_EQ(wd.c_encoding(), 1u);  // Monday = 1
}

// TODO: sys_days 转换 — 将日期转为天精度 time_point
//   chrono::sys_days to_sys_days(chrono::year_month_day ymd) {
//       return chrono::sys_days(ymd);
//   }

TEST("calendar — sys_days 转换与日期差") {
    int _todo_ = "请删除此行，实现上面的 TODO";
    // auto d1 = chrono::sys_days(chrono::year(2024) / 1 / 1);
    // auto d2 = chrono::sys_days(chrono::year(2024) / 1 / 31);
    // auto diff = d2 - d1;
    // ASSERT_EQ(diff.count(), 30);
}

// TODO: 日期比较
//   bool is_before(chrono::year_month_day a, chrono::year_month_day b) {
//       return chrono::sys_days(a) < chrono::sys_days(b);
//   }

TEST("calendar — 日期比较") {
    int _todo_ = "请删除此行，实现上面的 TODO";
    // auto jan = chrono::year(2024) / 1 / 15;
    // auto feb = chrono::year(2024) / 2 / 15;
    // ASSERT_TRUE(chrono::sys_days(jan) < chrono::sys_days(feb));
    // ASSERT_FALSE(chrono::sys_days(feb) < chrono::sys_days(jan));
}

// TODO: 日期算术 — 给定日期加 N 天
//   chrono::year_month_day add_days(chrono::year_month_day ymd, int n) {
//       return chrono::year_month_day(chrono::sys_days(ymd) + chrono::days(n));
//   }

TEST("calendar — 日期算术 加30天") {
    int _todo_ = "请删除此行，实现上面的 TODO";
    // auto start = chrono::year(2024) / 1 / 15;
    // auto later = chrono::year_month_day(
    //     chrono::sys_days(start) + chrono::days(30));
    // ASSERT_EQ((int)later.year(), 2024);
    // ASSERT_EQ((unsigned)later.month(), 2u);
    // ASSERT_EQ((unsigned)later.day(), 14u);
}

// TODO: 闰年判断
//   bool is_leap_year(int y) {
//       return chrono::year(y).is_leap();
//   }

TEST("calendar — 闰年判断") {
    int _todo_ = "请删除此行，实现上面的 TODO";
    // ASSERT_TRUE(chrono::year(2024).is_leap());
    // ASSERT_FALSE(chrono::year(2023).is_leap());
    // ASSERT_TRUE(chrono::year(2000).is_leap());
    // ASSERT_FALSE(chrono::year(1900).is_leap());
}

#else
// Fallback: basic date arithmetic without C++20 calendar types

// TODO: 实现简单的日期结构和操作
//   struct SimpleDate { int year; int month; int day; };
//   - 构造和 ok() 验证
//   - 星期几计算（蔡勒公式或基姆拉尔森公式）
//   - 日期差（天数）
//   - 加减天数
//   - 闰年判断

struct SimpleDate {
    int year;
    unsigned month;
    unsigned day;

    // TODO: 构造函数，验证日期合法性
    // SimpleDate(int y, unsigned m, unsigned d) ...

    // TODO: ok() — 验证日期是否合法
    bool ok() const {
        int _todo_ = "请删除此行，实现上面的 TODO";
        return false;
    }

    // TODO: is_leap() — 闰年判断
    bool is_leap() const {
        int _todo_ = "请删除此行，实现上面的 TODO";
        return false;
    }
};

// TODO: days_from_epoch — 计算从 1970-01-01 起的天数
//   int days_from_epoch(SimpleDate d) { ... }

// TODO: day_of_week — 星期几（0=Sun, 1=Mon, ..., 6=Sat）
//   int day_of_week(SimpleDate d) { ... }

TEST("calendar — 构造日期 2024-01-15 (fallback)") {
    int _todo_ = "请删除此行，实现上面的 TODO";
    // SimpleDate d(2024, 1, 15);
    // ASSERT_TRUE(d.ok());
    // ASSERT_EQ(d.year, 2024);
    // ASSERT_EQ(d.month, 1u);
    // ASSERT_EQ(d.day, 15u);
}

TEST("calendar — 2024-01-01 是星期一 (fallback)") {
    int _todo_ = "请删除此行，实现上面的 TODO";
    // SimpleDate d(2024, 1, 1);
    // ASSERT_EQ(day_of_week(d), 1);  // Monday = 1
}

TEST("calendar — 日期差 (fallback)") {
    int _todo_ = "请删除此行，实现上面的 TODO";
    // SimpleDate d1(2024, 1, 1);
    // SimpleDate d2(2024, 1, 31);
    // ASSERT_EQ(days_from_epoch(d2) - days_from_epoch(d1), 30);
}

TEST("calendar — 日期比较 (fallback)") {
    int _todo_ = "请删除此行，实现上面的 TODO";
    // SimpleDate jan(2024, 1, 15);
    // SimpleDate feb(2024, 2, 15);
    // ASSERT_TRUE(days_from_epoch(jan) < days_from_epoch(feb));
}

TEST("calendar — 日期算术 加30天 (fallback)") {
    int _todo_ = "请删除此行，实现上面的 TODO";
}

TEST("calendar — 闰年判断 (fallback)") {
    int _todo_ = "请删除此行，实现上面的 TODO";
    // ASSERT_TRUE(SimpleDate(2024, 1, 1).is_leap());
    // ASSERT_FALSE(SimpleDate(2023, 1, 1).is_leap());
    // ASSERT_TRUE(SimpleDate(2000, 1, 1).is_leap());
    // ASSERT_FALSE(SimpleDate(1900, 1, 1).is_leap());
}

#endif

CPPLINGS_MAIN
