---
title: "C++11 chrono Time Library"
topic: unknown
feature: chrono
standard: N/A
status_checked_at: 2026-06-02
---
# C++11 chrono Time Library

## Overview

`<chrono>` is a type-safe time library introduced in C++11, replacing the fragile C-style `time.h` design based on `double` and implicit unit conversions. It enforces unit correctness at compile time through template parameters — adding seconds and milliseconds will not produce a silent error; the compiler automatically handles unit conversion.

chrono consists of three core components:
- **duration**: A time interval (e.g., 5 seconds, 200 milliseconds)
- **time_point**: A point in time on a timeline
- **clock**: A source of current time (system clock, monotonic clock, etc.)

## Core API

### duration — Time Interval

`std::duration<Rep, Period>` is a template class where `Rep` is the storage type and `Period` is the time unit expressed as a `std::ratio`.

```cpp
#include <chrono>
#include <ratio>

using namespace std::chrono;

// Standard predefined aliases
// duration<long long, ratio<1>>         → seconds
// duration<long long, ratio<1, 1000>>    → milliseconds
// duration<long long, ratio<1, 1000000000>> → nanoseconds
// duration<double, ratio<1>>            → sub-second precision floating-point seconds

seconds sec(10);                  // 10 seconds
milliseconds ms(500);             // 500 milliseconds
nanoseconds ns(1'000'000'000);    // 1 billion nanoseconds = 1 second
```

#### duration Arithmetic and Conversion

```cpp
using namespace std::chrono;

seconds s1(3), s2(5);
auto result = s1 + s2;           // seconds(8)

// Adding different units — compiler automatically converts to the finer unit
auto sum = seconds(1) + milliseconds(500);  // milliseconds(1500)

// Multiply/divide by scalar
auto doubled = seconds(1) * 2;   // seconds(2)

// Precision reduction requires explicit duration_cast (truncation, not rounding)
milliseconds ms2(1500);
seconds s = duration_cast<seconds>(ms2);  // 1 second

// Precision promotion does not require cast (no information loss)
nanoseconds ns2(3'000'000'000);
seconds s3 = ns2;  // OK, implicitly converts to 3 seconds
```

#### Custom duration Types

```cpp
using namespace std::chrono;

// 1 frame = 1/60 second
using frames = duration<int64_t, ratio<1, 60>>;
frames f(120);                              // 120 frames = 2 seconds
seconds s = duration_cast<seconds>(f);      // 2 seconds

// Floating-point rep for sub-frame precision
using precise_frames = duration<double, ratio<1, 60>>;
milliseconds ms2 = duration_cast<milliseconds>(precise_frames(1.5)); // 25ms
```

### time_point — Point in Time

`time_point<Clock, Duration>` represents a point in time relative to a clock's epoch:

```cpp
using namespace std::chrono;

system_clock::time_point now = system_clock::now();
auto later = now + seconds(3600);          // 1 hour later
auto elapsed = later - now;                // type is system_clock::duration

// time_point_cast — convert precision
auto tp_sec = time_point_cast<seconds>(now);
```

### Clock — Three Clocks

| Clock | Monotonic | Typical Use |
|-------|-----------|-------------|
| `system_clock` | **No** (adjustable by system) | Calendar time, wall-clock time |
| `steady_clock` | **Yes** | Measuring elapsed time, timers |
| `high_resolution_clock` | May be alias for steady_clock | High-precision benchmarks |

#### system_clock — System Wall Clock

```cpp
using namespace std::chrono;

auto now = system_clock::now();
std::time_t t = system_clock::to_time_t(now);
std::cout << std::ctime(&t);  // outputs current date and time

// Convert back from time_t to time_point
system_clock::time_point tp = system_clock::from_time_t(t);
```

#### steady_clock — Monotonic Clock (Preferred for Elapsed Time Measurement)

```cpp
using namespace std::chrono;

steady_clock::time_point start = steady_clock::now();

volatile int x = 0;
for (int i = 0; i < 1000000; ++i) x += i;

auto elapsed = duration_cast<microseconds>(steady_clock::now() - start);
std::cout << "Elapsed: " << elapsed.count() << " microseconds\n";
```

## Idiomatic Pattern for Measuring Elapsed Time

```cpp
#include <chrono>
#include <functional>

template<typename Func>
auto measure(Func&& func) {
    using namespace std::chrono;
    auto start = steady_clock::now();
    std::forward<Func>(func)();
    return duration_cast<microseconds>(steady_clock::now() - start);
}

// Usage
auto dur = measure([] {
    volatile int sum = 0;
    for (int i = 0; i < 10'000'000; ++i) sum += i;
});

// Floating-point seconds — precise representation
duration<double> sec = dur;
std::cout << sec.count() << "s\n";
```

## C++14 chrono Literals (Mentioned)

C++14 elevated `duration` arithmetic to `constexpr` and introduced `std::chrono_literals` suffix literals:

```cpp
#include <chrono>
using namespace std::chrono_literals;

constexpr auto timeout = 500ms + 200ms;   // compile-time evaluation 700ms
static_assert(timeout == 700ms);

auto one_sec      = 1s;            // seconds(1)
auto half_sec     = 500ms;         // milliseconds(500)
auto one_usec     = 1us;           // microseconds(1)
auto one_nsec     = 1ns;           // nanoseconds(1)
auto floating_sec = 2.5s;          // duration<double>(2.5)
```

## Best Practices

1. **Use `steady_clock` for elapsed time measurement** — it is unaffected by system time adjustments and is the only clock guaranteed to be monotonic.
2. **Prefer `auto` to receive durations** — avoids precision loss or type mismatch from manual type specification.
3. **Use `duration<double>` when floating-point results are needed** — integer `duration_cast` truncates; floating-point conversion preserves precision.
4. **Use `duration_cast` to explicitly express truncation intent** — precision reduction requires explicit conversion; this is chrono's type safety in action.
5. **Consider C++20 `<chrono>` extensions for calendar/date handling** — C++11 chrono lacks calendar and timezone support.

## Common Pitfalls

- **Using `system_clock` to measure elapsed time**: NTP time adjustments or user changes to system time can cause negative or abnormally large results. Always use `steady_clock`.
- **`duration::count()` misunderstanding**: `count()` returns the underlying storage value. `milliseconds(1500).count()` is `1500`, not `1.5`.
- **Implicit precision reduction is a compilation error**: `seconds s = milliseconds(1500);` will not compile. This is by design; use `duration_cast`.
- **`high_resolution_clock` is unreliable**: In some implementations it is an alias for `system_clock` and is not guaranteed to be monotonic. Avoid depending on it for cross-platform code.
- **Integer division truncation**: `seconds(1) / 3` results in `seconds(0)`. Use `duration<double>(1) / 3` for the precise value.
- **`time_point` epoch is unspecified**: The standard does not guarantee that `system_clock`'s epoch is Unix epoch; do not hardcode assumptions.
