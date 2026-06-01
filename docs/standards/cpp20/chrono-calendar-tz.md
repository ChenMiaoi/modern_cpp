# 日历与时间区

## 概述

C++20 大幅扩展 `<chrono>`，新增日历类型（年、月、日及其组合）和时区支持（`time_zone`、`zoned_time`）。类型安全，与既有的 `time_point` / `duration` 无缝衔接。

## 日历类型

```cpp
#include <chrono>
using namespace std::chrono;

year y{2024};
month m{6};
day d{15};

day{32}.ok();            // false
year next_y = y + years{1};   // 2025
month next_m = m + months{3}; // September
day tomorrow = d + days{1};   // 16
```

### `year_month_day`

```cpp
using namespace std::chrono;

year_month_day ymd{year{2024}, month{6}, day{15}};
ymd.ok();              // true
ymd.year();            // 2024
ymd.month();           // June
ymd.day();             // 15

year_month_day bad{year{2024}, month{2}, day{30}};
bad.ok();              // false
```

### 其他日历类型

```cpp
using namespace std::chrono;

year{2024} / month{2} / last;               // Feb 29
year{2024} / month{6} / weekday{0}[2];      // 第 2 个星期日
(year{2024} / February / last).day();       // 29
```

## `sys_days` 转换

`sys_days` 是以天为精度的 `time_point`，连接日历与时间点。

```cpp
using namespace std::chrono;

year_month_day ymd{year{2024}, month{1}, day{1}};
sys_days sd = ymd;                     // 日历 → time_point
year_month_day back{sd};               // time_point → 日历

days diff = sys_days{year{2024}/12/25} - sys_days{year{2024}/1/1};
// diff.count() == 359

weekday wd = weekday{sys_days{ymd}};   // 星期几
```

## `/` 运算符构造日期

```cpp
using namespace std::chrono;

auto d1 = year{2024} / 6 / 15;       // year_month_day
auto d2 = 2024y / June / 15d;         // 同上
auto d3 = 2024y / February / last;    // 2 月最后一天
auto d4 = June / 15d;                  // month_day
```

## 时区支持

### 获取时区

```cpp
#include <chrono>
using namespace std::chrono;

const time_zone* tz = current_zone();       // 如 "Asia/Shanghai"
const time_zone* sh = locate_zone("Asia/Shanghai");
const time_zone* ny = locate_zone("America/New_York");
```

### `zoned_time` 与 `make_zoned`

```cpp
using namespace std::chrono;

auto now = system_clock::now();
zoned_time sh_t{locate_zone("Asia/Shanghai"), now};
zoned_time ny_t{locate_zone("America/New_York"), now};

// 从日期时间创建
auto zt = make_zoned("Europe/London",
    local_days{2024y / July / 4} + hours{15} + minutes{30});

// 时区转换
auto eastern = make_zoned("America/New_York", zt);
```

### 转换示例

```cpp
using namespace std::chrono;

// 北京上午 9 点 → 纽约时间
auto bj9 = make_zoned("Asia/Shanghai",
    local_days{2024y / 1 / 15} + hours{9});
auto nyc = make_zoned("America/New_York", bj9);
// 北京 09:00 = 纽约前一天 20:00（冬令时 UTC-5）
```

## 闰秒支持

```cpp
using namespace std::chrono;

auto lsi = get_leap_second_info(system_clock::now());
lsi.elapsed;          // 已累计闰秒数
lsi.is_leap_second;   // 当前秒是否闰秒
```

## 格式化

```cpp
#include <chrono>
#include <format>
using namespace std::chrono;

auto dp = floor<days>(system_clock::now());
year_month_day ymd{dp};
hh_mm_ss hms{floor<seconds>(system_clock::now() - dp)};

std::format("{}/{}/{:02d} {:02d}:{:02d}:{:02d}",
    int(ymd.year()), unsigned(ymd.month()), unsigned(ymd.day()),
    hms.hours().count(), hms.minutes().count(), hms.seconds().count());
```

## 从旧 API 迁移

```cpp
// C++17
std::time_t t = std::time(nullptr);
int year = std::localtime(&t)->tm_year + 1900;

// C++20
using namespace std::chrono;
year_month_day ymd{floor<days>(system_clock::now())};
auto y = int(ymd.year());
auto m = unsigned(ymd.month());  // 直接 1-12
```

## 常见陷阱

```cpp
// 不合法日期不自动修正
year{2023} / month{2} / day{29};  // ok() == false（非闰年）

// locate_zone 失败抛异常
try { locate_zone("Invalid/Zone"); }
catch (const std::runtime_error&) { }

// 闰年判断
year{2024}.is_leap();  // true
```

## 总结

- 类型安全日历：`year`、`month`、`day` 及组合。
- `sys_days` 是日历与 `time_point` 的桥梁。
- `zoned_time` + `time_zone` 提供完整时区支持。
- `/` 运算符简化日期构造，`make_zoned` 简化时区创建。
