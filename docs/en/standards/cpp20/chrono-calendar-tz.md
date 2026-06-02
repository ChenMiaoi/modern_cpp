---
title: "Calendars and Time Zones"
topic: unknown
feature: chrono-calendar-tz
standard: N/A
status_checked_at: 2026-06-02
---
# Calendars and Time Zones

## Overview

C++20 significantly expands `<chrono>`, adding calendar types (year, month, day, and their combinations) and time zone support (`time_zone`, `zoned_time`). Types are type-safe and integrate seamlessly with existing `time_point` / `duration`.

## Calendar Types

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

### Other Calendar Types

```cpp
using namespace std::chrono;

year{2024} / month{2} / last;               // Feb 29
year{2024} / month{6} / weekday{0}[2];      // 2nd Sunday
(year{2024} / February / last).day();       // 29
```

## `sys_days` Conversion

`sys_days` is a day-precision `time_point` connecting calendars and time points.

```cpp
using namespace std::chrono;

year_month_day ymd{year{2024}, month{1}, day{1}};
sys_days sd = ymd;                     // calendar → time_point
year_month_day back{sd};               // time_point → calendar

days diff = sys_days{year{2024}/12/25} - sys_days{year{2024}/1/1};
// diff.count() == 359

weekday wd = weekday{sys_days{ymd}};   // day of the week
```

## `/` Operator for Date Construction

```cpp
using namespace std::chrono;

auto d1 = year{2024} / 6 / 15;       // year_month_day
auto d2 = 2024y / June / 15d;         // same as above
auto d3 = 2024y / February / last;    // last day of February
auto d4 = June / 15d;                  // month_day
```

## Time Zone Support

### Getting Time Zones

```cpp
#include <chrono>
using namespace std::chrono;

const time_zone* tz = current_zone();       // e.g., "Asia/Shanghai"
const time_zone* sh = locate_zone("Asia/Shanghai");
const time_zone* ny = locate_zone("America/New_York");
```

### `zoned_time` and `make_zoned`

```cpp
using namespace std::chrono;

auto now = system_clock::now();
zoned_time sh_t{locate_zone("Asia/Shanghai"), now};
zoned_time ny_t{locate_zone("America/New_York"), now};

// Create from date-time
auto zt = make_zoned("Europe/London",
    local_days{2024y / July / 4} + hours{15} + minutes{30});

// Time zone conversion
auto eastern = make_zoned("America/New_York", zt);
```

### Conversion Example

```cpp
using namespace std::chrono;

// Beijing 9 AM → New York time
auto bj9 = make_zoned("Asia/Shanghai",
    local_days{2024y / 1 / 15} + hours{9});
auto nyc = make_zoned("America/New_York", bj9);
// Beijing 09:00 = New York previous day 20:00 (winter time, UTC-5)
```

## Leap Second Support

```cpp
using namespace std::chrono;

auto lsi = get_leap_second_info(system_clock::now());
lsi.elapsed;          // accumulated leap seconds
lsi.is_leap_second;   // whether the current second is a leap second
```

## Formatting

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

## Migrating from Legacy APIs

```cpp
// C++17
std::time_t t = std::time(nullptr);
int year = std::localtime(&t)->tm_year + 1900;

// C++20
using namespace std::chrono;
year_month_day ymd{floor<days>(system_clock::now())};
auto y = int(ymd.year());
auto m = unsigned(ymd.month());  // directly 1-12
```

## Common Pitfalls

```cpp
// Invalid dates are not automatically corrected
year{2023} / month{2} / day{29};  // ok() == false (not a leap year)

// locate_zone throws on failure
try { locate_zone("Invalid/Zone"); }
catch (const std::runtime_error&) { }

// Leap year check
year{2024}.is_leap();  // true
```

## Summary

- Type-safe calendars: `year`, `month`, `day`, and their combinations.
- `sys_days` is the bridge between calendars and `time_point`.
- `zoned_time` + `time_zone` provide full time zone support.
- The `/` operator simplifies date construction; `make_zoned` simplifies time zone creation.
