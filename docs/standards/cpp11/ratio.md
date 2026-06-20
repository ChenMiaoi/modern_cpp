---
title: "std::ratio"
topic: unknown
feature: ratio
standard: N/A
status_checked_at: 2026-06-20
---
# std::ratio

## 概述

`std::ratio` 是 C++11 引入的编译期有理数算术库，定义在 `<ratio>` 头文件中。它允许在编译期执行精确的分数运算，不会产生浮点数的精度损失。主要用于 `std::chrono` 中的时间单位定义，也可以在任何需要编译期精确数值计算的场景中使用。

```cpp
#include <ratio>
```

## std::ratio 的基本定义

`std::ratio<Num, Denom>` 表示分数 `Num / Denom`，其中 `Num` 是分子，`Denom` 是分母。分母默认为 1。

```cpp
#include <ratio>
#include <type_traits>

// 表示 3/4
using three_fourths = std::ratio<3, 4>;

// 表示 5/1 = 5
using five = std::ratio<5>;

// 表示 1/1000 = 0.001
using milli = std::ratio<1, 1000>;

static_assert(three_fourths::num == 3, "");
static_assert(three_fourths::den == 4, "");
```

### 编译期归约

`std::ratio` 会自动将分数化为最简形式：

```cpp
using result = std::ratio<6, 8>;  // 自动归约为 3/4

static_assert(result::num == 3, "");
static_assert(result::den == 4, "");
```

## 算术运算

### ratio_add — 加法

```cpp
#include <ratio>
#include <type_traits>

// 1/3 + 1/6 = 2/6 + 1/6 = 3/6 = 1/2
using sum = std::ratio_add<std::ratio<1, 3>, std::ratio<1, 6>>;

static_assert(sum::num == 1, "");
static_assert(sum::den == 2, "");
```

### ratio_subtract — 减法

```cpp
// 3/4 - 1/4 = 2/4 = 1/2
using diff = std::ratio_subtract<std::ratio<3, 4>, std::ratio<1, 4>>;

static_assert(diff::num == 1, "");
static_assert(diff::den == 2, "");
```

### ratio_multiply — 乘法

```cpp
// 2/3 * 3/5 = 6/15 = 2/5
using product = std::ratio_multiply<std::ratio<2, 3>, std::ratio<3, 5>>;

static_assert(product::num == 2, "");
static_assert(product::den == 5, "");
```

### ratio_divide — 除法

```cpp
// (1/2) / (3/4) = (1/2) * (4/3) = 4/6 = 2/3
using quotient = std::ratio_divide<std::ratio<1, 2>, std::ratio<3, 4>>;

static_assert(quotient::num == 2, "");
static_assert(quotient::den == 3, "");
```

## 比较运算

### ratio_equal — 相等比较

```cpp
using a = std::ratio<2, 4>;  // 归约为 1/2
using b = std::ratio<3, 6>;  // 归约为 1/2

static_assert(std::ratio_equal<a, b>::value, "1/2 == 1/2");
static_assert(!std::ratio_equal<std::ratio<1, 3>, std::ratio<1, 4>>::value, "");
```

### ratio_less — 小于比较

```cpp
static_assert(std::ratio_less<std::ratio<1, 3>, std::ratio<1, 2>>::value, "1/3 < 1/2");
static_assert(!std::ratio_less<std::ratio<3, 4>, std::ratio<1, 2>>::value, "3/4 >= 1/2");
```

### ratio_greater — 大于比较

```cpp
static_assert(std::ratio_greater<std::ratio<5, 6>, std::ratio<1, 2>>::value, "5/6 > 1/2");
```

## 预定义的单位别名

`<ratio>` 头文件提供了一系列常用的 SI 前缀别名：

| 别名 | 值 | 别名 | 值 |
|------|----|------|----|
| `std::ratio<1>` | 1 | `std::ratio<-1>` | -1 |
| `std::ratio<1000>` | 1000 | `std::ratio<1000000>` | 1000000 |
| `std::ratio<1, 1000>` | 1/1000 | `std::ratio<1, 1000000>` | 1/1000000 |

```cpp
#include <ratio>

// 常用别名
using kilo   = std::ratio<1000>;           // 1000
using milli  = std::ratio<1, 1000>;        // 1/1000
using micro  = std::ratio<1, 1000000>;     // 1/1000000
using nano   = std::ratio<1, 1000000000>;  // 1/1000000000
```

## 与 std::chrono 的结合

`std::ratio` 最常见的用途是在 `std::chrono` 中定义时间单位。每个 `std::chrono::duration` 的模板参数都是一个 `std::ratio`：

```cpp
#include <chrono>
#include <ratio>
#include <iostream>

int main() {
    // std::chrono::seconds 的 period 是 std::ratio<1>
    // std::chrono::milliseconds 的 period 是 std::ratio<1, 1000>
    // std::chrono::microseconds 的 period 是 std::ratio<1, 1000000>

    using namespace std::chrono;

    seconds s(1);
    milliseconds ms = s;     // 1000ms
    microseconds us = ms;    // 1000000us

    std::cout << s.count() << '\n';   // 1
    std::cout << ms.count() << '\n';  // 1000
    std::cout << us.count() << '\n';  // 1000000

    // 自定义时间单位
    using fortnight = std::chrono::duration<long long, std::ratio<1209600>>;
    // 1 fortnight = 1209600 seconds (14 days)

    fortnight f(1);
    seconds equivalent = f;
    std::cout << equivalent.count() << '\n';  // 1209600
}
```

## 代码示例

### 编译期单位换算

```cpp
#include <ratio>
#include <iostream>

template <typename FromRatio, typename ToRatio>
constexpr double convert(double value) {
    // value * FromRatio::num / FromRatio::den * ToRatio::den / ToRatio::num
    // 简化为 value * FromRatio::num * ToRatio::den / (FromRatio::den * ToRatio::num)
    return value * FromRatio::num * ToRatio::den
           / static_cast<double>(FromRatio::den * ToRatio::num);
}

int main() {
    using km_to_m = std::ratio<1000, 1>;     // 1 km = 1000 m
    using m_to_km = std::ratio<1, 1000>;     // 1 m = 1/1000 km

    double km = 5.0;
    double m = convert<km_to_m, std::ratio<1>>(km);  // 转为米
    std::cout << km << " km = " << m << " m\n";       // 5 km = 5000 m

    double back_km = convert<std::ratio<1>, km_to_m>(m);  // 转回千米
    std::cout << m << " m = " << back_km << " km\n";     // 5000 m = 5 km
}
```

## 注意事项与陷阱

**除以零**——如果分母为零，编译器会产生编译错误（不是运行时错误）：

```cpp
// using bad = std::ratio<1, 0>;  // 编译错误: 分母不能为零
```

**溢出**——当分子或分母的乘积超出 `long long` 范围时，会产生未定义行为或编译错误：

```cpp
// using huge = std::ratio<999999999999LL, 999999999999LL>;  // 可能溢出
```

**ratio 不是运行时类型**——`std::ratio` 是纯编译期构造，没有运行时值。不能在运行时创建 `std::ratio` 对象或使用其值进行动态计算：

```cpp
// 这是编译期静态成员，不是运行时值
static_assert(std::ratio<3, 4>::num == 3, "");
```

**与浮点数的转换**——`std::ratio` 本身不提供到浮点数的转换，需要手动计算：

```cpp
using r = std::ratio<3, 4>;
double val = static_cast<double>(r::num) / r::den;  // 0.75
```

## 编译器支持

| 编译器 | 支持版本 | 备注 |
|--------|----------|------|
| GCC | 4.5+ | 完全支持 |
| Clang | 3.1+ | 完全支持 |
| MSVC | 2012 (17.0)+ | 完全支持 |

`std::ratio` 是 `std::chrono` 的基础设施，虽然直接使用的场景较少，但理解它有助于深入理解 C++ 时间库的设计。它体现了 C++11 "零开销抽象"的理念——编译期精确计算，运行时无任何额外开销。
