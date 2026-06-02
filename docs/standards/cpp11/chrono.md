---
title: "C++11 chrono 时间库"
topic: unknown
feature: chrono
standard: N/A
status_checked_at: 2026-06-02
---
# C++11 chrono 时间库

## 概述

`<chrono>` 是 C++11 引入的类型安全时间库，替代了 C 风格 `time.h` 中基于 `double` 和隐式单位转换的脆弱设计。通过模板参数在编译期强制单位正确性——秒与毫秒相加不会产生静默错误，编译器自动完成单位转换。

chrono 由三个核心组件构成：
- **duration**：时间间隔（如 5 秒、200 毫秒）
- **time_point**：时间线上的某个时刻
- **clock**：当前时间的来源（系统时钟、单调时钟等）

## 核心 API

### duration — 时间间隔

`std::duration<Rep, Period>` 是模板类，`Rep` 是存储类型，`Period` 是 `std::ratio` 表示的时间单位。

```cpp
#include <chrono>
#include <ratio>

using namespace std::chrono;

// 标准预定义别名
// duration<long long, ratio<1>>         → seconds
// duration<long long, ratio<1, 1000>>    → milliseconds
// duration<long long, ratio<1, 1000000000>> → nanoseconds
// duration<double, ratio<1>>            → 亚秒精度浮点秒

seconds sec(10);                  // 10 秒
milliseconds ms(500);             // 500 毫秒
nanoseconds ns(1'000'000'000);    // 10 亿纳秒 = 1 秒
```

#### duration 算术与转换

```cpp
using namespace std::chrono;

seconds s1(3), s2(5);
auto result = s1 + s2;           // seconds(8)

// 不同单位相加——编译器自动转换到更精细的单位
auto sum = seconds(1) + milliseconds(500);  // milliseconds(1500)

// 乘除标量
auto doubled = seconds(1) * 2;   // seconds(2)

// 精度降低必须显式 duration_cast（截断，非四舍五入）
milliseconds ms2(1500);
seconds s = duration_cast<seconds>(ms2);  // 1 秒

// 精度提升不需要 cast（无信息丢失）
nanoseconds ns2(3'000'000'000);
seconds s3 = ns2;  // OK，隐式转换为 3 秒
```

#### 自定义 duration 类型

```cpp
using namespace std::chrono;

// 1 帧 = 1/60 秒
using frames = duration<int64_t, ratio<1, 60>>;
frames f(120);                              // 120 帧 = 2 秒
seconds s = duration_cast<seconds>(f);      // 2 秒

// 浮点 rep 获取亚帧精度
using precise_frames = duration<double, ratio<1, 60>>;
milliseconds ms2 = duration_cast<milliseconds>(precise_frames(1.5)); // 25ms
```

### time_point — 时间点

`time_point<Clock, Duration>` 表示相对于某个时钟纪元的时间点：

```cpp
using namespace std::chrono;

system_clock::time_point now = system_clock::now();
auto later = now + seconds(3600);          // 1 小时后
auto elapsed = later - now;                // 类型为 system_clock::duration

// time_point_cast——转换精度
auto tp_sec = time_point_cast<seconds>(now);
```

### Clock — 三种时钟

| 时钟 | 单调性 | 典型用途 |
|------|--------|----------|
| `system_clock` | **否**（可被系统调整） | 日历时间、挂钟时间 |
| `steady_clock` | **是** | 测量耗时、计时器 |
| `high_resolution_clock` | 可能是 steady_clock 别名 | 高精度基准测试 |

#### system_clock — 系统挂钟

```cpp
using namespace std::chrono;

auto now = system_clock::now();
std::time_t t = system_clock::to_time_t(now);
std::cout << std::ctime(&t);  // 输出当前日期时间

// 从 time_t 转回 time_point
system_clock::time_point tp = system_clock::from_time_t(t);
```

#### steady_clock — 单调时钟（测量耗时首选）

```cpp
using namespace std::chrono;

steady_clock::time_point start = steady_clock::now();

volatile int x = 0;
for (int i = 0; i < 1000000; ++i) x += i;

auto elapsed = duration_cast<microseconds>(steady_clock::now() - start);
std::cout << "耗时: " << elapsed.count() << " 微秒\n";
```

## 测量耗时的惯用模式

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

// 使用
auto dur = measure([] {
    volatile int sum = 0;
    for (int i = 0; i < 10'000'000; ++i) sum += i;
});

// 浮点秒数——精确表示
duration<double> sec = dur;
std::cout << sec.count() << "s\n";
```

## C++14 chrono 字面量（提及）

C++14 将 `duration` 算术提升为 `constexpr`，并引入 `std::chrono_literals` 字面量后缀：

```cpp
#include <chrono>
using namespace std::chrono_literals;

constexpr auto timeout = 500ms + 200ms;   // 编译期求值 700ms
static_assert(timeout == 700ms);

auto one_sec      = 1s;            // seconds(1)
auto half_sec     = 500ms;         // milliseconds(500)
auto one_usec     = 1us;           // microseconds(1)
auto one_nsec     = 1ns;           // nanoseconds(1)
auto floating_sec = 2.5s;          // duration<double>(2.5)
```

## 最佳实践

1. **测量耗时用 `steady_clock`**——它不受系统时间调整影响，是唯一保证单调的时钟。
2. **优先用 `auto` 接收 duration**——避免手动指定类型导致精度丢失或类型不匹配。
3. **需要浮点结果时用 `duration<double>`**——整数 `duration_cast` 会截断，浮点转换保持精度。
4. **用 `duration_cast` 显式表达截断意图**——精度降低需要显式转换，这是 chrono 类型安全的体现。
5. **日历/日期处理考虑 C++20 `<chrono>` 扩展**——C++11 chrono 缺少日历和时区支持。

## 常见陷阱

- **用 `system_clock` 测量耗时**：NTP 调时、用户修改系统时间都会导致结果为负或异常大。始终使用 `steady_clock`。
- **`duration::count()` 误解**：`count()` 返回底层存储值。`milliseconds(1500).count()` 是 `1500`，不是 `1.5`。
- **隐式精度降低是编译错误**：`seconds s = milliseconds(1500);` 无法编译。这是设计意图，使用 `duration_cast`。
- **`high_resolution_clock` 不可靠**：某些实现中它是 `system_clock` 的别名，不保证单调。跨平台代码应避免依赖它。
- **整数除法截断**：`seconds(1) / 3` 结果为 `seconds(0)`。需要 `duration<double>(1) / 3` 得到精确值。
- **`time_point` 的纪元未指定**：标准不保证 `system_clock` 的纪元是 Unix epoch，不要硬编码假设。
