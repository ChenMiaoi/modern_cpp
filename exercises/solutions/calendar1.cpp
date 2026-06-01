// Solution — calendar1: 日历与时间区
#include "cpplings.h"

#if __cpp_lib_chrono >= 201907L
#include <chrono>

namespace chrono = std::chrono;

TEST("calendar — 构造日期 2024-01-15") {
    auto ymd = chrono::year(2024) / 1 / 15;
    ASSERT_TRUE(ymd.ok());
    ASSERT_EQ((int)ymd.year(), 2024);
    ASSERT_EQ((unsigned)ymd.month(), 1u);
    ASSERT_EQ((unsigned)ymd.day(), 15u);
}

TEST("calendar — 2024-01-01 是星期一") {
    auto ymd = chrono::year(2024) / 1 / 1;
    auto wd = chrono::weekday(chrono::sys_days(ymd));
    ASSERT_EQ(wd.c_encoding(), 1u);  // Monday = 1
}

TEST("calendar — sys_days 转换与日期差") {
    auto d1 = chrono::sys_days(chrono::year(2024) / 1 / 1);
    auto d2 = chrono::sys_days(chrono::year(2024) / 1 / 31);
    auto diff = d2 - d1;
    ASSERT_EQ(diff.count(), 30);
}

TEST("calendar — 日期比较") {
    auto jan = chrono::year(2024) / 1 / 15;
    auto feb = chrono::year(2024) / 2 / 15;
    ASSERT_TRUE(chrono::sys_days(jan) < chrono::sys_days(feb));
    ASSERT_FALSE(chrono::sys_days(feb) < chrono::sys_days(jan));
}

TEST("calendar — 日期算术 加30天") {
    auto start = chrono::year(2024) / 1 / 15;
    auto later = chrono::year_month_day(
        chrono::sys_days(start) + chrono::days(30));
    ASSERT_EQ((int)later.year(), 2024);
    ASSERT_EQ((unsigned)later.month(), 2u);
    ASSERT_EQ((unsigned)later.day(), 14u);
}

TEST("calendar — 闰年判断") {
    ASSERT_TRUE(chrono::year(2024).is_leap());
    ASSERT_FALSE(chrono::year(2023).is_leap());
    ASSERT_TRUE(chrono::year(2000).is_leap());
    ASSERT_FALSE(chrono::year(1900).is_leap());
}

#else
// Fallback: basic date arithmetic

struct SimpleDate {
    int year;
    unsigned month;
    unsigned day;

    SimpleDate(int y, unsigned m, unsigned d) : year(y), month(m), day(d) {}

    bool is_leap() const {
        return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    }

    static int days_in_month(int y, unsigned m) {
        static const int mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
        if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)))
            return 29;
        return mdays[m - 1];
    }

    bool ok() const {
        if (month < 1 || month > 12) return false;
        if (day < 1 || day > static_cast<unsigned>(days_in_month(year, month)))
            return false;
        return true;
    }
};

int days_from_epoch(SimpleDate d) {
    int y = d.year;
    int m = static_cast<int>(d.month);
    int day = static_cast<int>(d.day);

    // Adjust so that Mar=3..14, Jan/Feb become months 13/14 of previous year
    if (m <= 2) { y -= 1; m += 12; }

    // Days from civil calendar (Howard Hinnant's algorithm)
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = static_cast<unsigned>(y - era * 400);
    unsigned doy = (153 * (m - 3) + 2) / 5 + day - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int>(doe) - 719468;
}

// 0=Sun, 1=Mon, ..., 6=Sat
int day_of_week(SimpleDate d) {
    int days = days_from_epoch(d);
    // epoch (1970-01-01) is Thursday (4)
    return ((days % 7) + 7 + 4) % 7;
}

SimpleDate add_days(SimpleDate d, int n) {
    int total = days_from_epoch(d) + n;
    // Convert back from days-from-epoch to y/m/d
    total += 719468;
    int era = (total >= 0 ? total : total - 146096) / 146097;
    unsigned doe = static_cast<unsigned>(total - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int y = static_cast<int>(yoe) + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    unsigned d_day = doy - (153 * mp + 2) / 5 + 1;
    unsigned m = mp + 3;
    if (m > 12) { m -= 12; ++y; }
    return SimpleDate(y, m, d_day);
}

TEST("calendar — 构造日期 2024-01-15 (fallback)") {
    SimpleDate d(2024, 1, 15);
    ASSERT_TRUE(d.ok());
    ASSERT_EQ(d.year, 2024);
    ASSERT_EQ(d.month, 1u);
    ASSERT_EQ(d.day, 15u);
}

TEST("calendar — 2024-01-01 是星期一 (fallback)") {
    SimpleDate d(2024, 1, 1);
    ASSERT_EQ(day_of_week(d), 1);  // Monday = 1
}

TEST("calendar — 日期差 (fallback)") {
    SimpleDate d1(2024, 1, 1);
    SimpleDate d2(2024, 1, 31);
    ASSERT_EQ(days_from_epoch(d2) - days_from_epoch(d1), 30);
}

TEST("calendar — 日期比较 (fallback)") {
    SimpleDate jan(2024, 1, 15);
    SimpleDate feb(2024, 2, 15);
    ASSERT_TRUE(days_from_epoch(jan) < days_from_epoch(feb));
}

TEST("calendar — 日期算术 加30天 (fallback)") {
    SimpleDate start(2024, 1, 15);
    auto later = add_days(start, 30);
    ASSERT_EQ(later.year, 2024);
    ASSERT_EQ(later.month, 2u);
    ASSERT_EQ(later.day, 14u);
}

TEST("calendar — 闰年判断 (fallback)") {
    ASSERT_TRUE(SimpleDate(2024, 1, 1).is_leap());
    ASSERT_FALSE(SimpleDate(2023, 1, 1).is_leap());
    ASSERT_TRUE(SimpleDate(2000, 1, 1).is_leap());
    ASSERT_FALSE(SimpleDate(1900, 1, 1).is_leap());
}

#endif

CPPLINGS_MAIN
