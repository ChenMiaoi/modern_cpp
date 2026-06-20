---
title: C++17 std::clamp
topic: unknown
feature: std-clamp
standard: N/A
status_checked_at: 2026-06-20
---
# C++17 std::clamp

## 概述

`std::clamp` 是 C++17 在 `<algorithm>` 中引入的自由函数，用于将一个值**限制在指定的上下界之间**。它返回 `lo`（当 `val < lo` 时）、`hi`（当 `val > hi` 时）或 `val`（当在范围内时）。这在 GUI 开发、插值计算、物理模拟等需要边界限制的场景中非常实用。

## 函数签名

```cpp
#include <algorithm>

template <class T>
constexpr const T& clamp(const T& val, const T& lo, const T& hi);
```

语义：
- 若 `val < lo`，返回 `lo`
- 若 `val > hi`，返回 `hi`
- 否则返回 `val`

**要求**：`lo <= hi`，否则行为未定义。

## 基本用法

```cpp
#include <algorithm>
#include <iostream>

int main() {
    int a = std::clamp(5, 0, 10);    // 5（在范围内）
    int b = std::clamp(-5, 0, 10);   // 0（低于下界）
    int c = std::clamp(15, 0, 10);   // 10（超过上界）

    std::cout << a << " " << b << " " << c << "\n";  // 5 0 10
}
```

## 自定义比较器

```cpp
#include <algorithm>
#include <string>
#include <cctype>

// 自定义比较：忽略大小写排序
bool case_insensitive_less(char a, char b) {
    return std::tolower(a) < std::tolower(b);
}

int main() {
    // 标准版本使用 operator<
    // C++14 起 clamp 支持自定义比较（C++17 之前的实现差异）
    // C++17 标准只提供默认版本，但你可以包装
    int val = std::clamp(100, 0, 255);  // 100
}
```

## constexpr 支持

`std::clamp` 在 C++17 中是 `constexpr`，可以在编译期使用：

```cpp
#include <algorithm>

// 编译期计算
constexpr int clamped = std::clamp(42, 0, 100);  // 42
constexpr int low = std::clamp(-10, 0, 100);     // 0
constexpr int high = std::clamp(200, 0, 100);    // 100

// 用于模板参数
template <int N>
struct ClampedValue {
    static constexpr int value = std::clamp(N, 0, 255);
};

static_assert(ClampedValue<300>::value == 255);
static_assert(ClampedValue<-5>::value == 0);
static_assert(ClampedValue<128>::value == 128);
```

## GUI 像素坐标限制

```cpp
#include <algorithm>

struct Point {
    int x, y;
};

struct Rect {
    int left, top, right, bottom;
};

Point clamp_point(const Point& p, const Rect& bounds) {
    return {
        std::clamp(p.x, bounds.left, bounds.right),
        std::clamp(p.y, bounds.top, bounds.bottom)
    };
}

int main() {
    Point p{150, -20};
    Rect bounds{0, 0, 100, 100};
    Point clamped = clamp_point(p, bounds);
    // clamped = {100, 0}
}
```

## 插值与动画

```cpp
#include <algorithm>
#include <iostream>

double lerp(double a, double b, double t) {
    return a + (b - a) * t;
}

int main() {
    double start = 0.0, end = 100.0;

    // t 应限制在 [0, 1] 范围内
    double t = 1.5;
    double t_clamped = std::clamp(t, 0.0, 1.0);  // 1.0

    double value = lerp(start, end, t_clamped);
    std::cout << "lerp: " << value << "\n";  // 100.0

    // 渐变色计算
    auto progress = [](double t) {
        t = std::clamp(t, 0.0, 1.0);
        int r = static_cast<int>(255 * (1.0 - t));
        int g = static_cast<int>(255 * t);
        return (r << 16) | (g << 8);
    };

    int color = progress(0.5);  // 0x7F7F00
}
```

## 游戏中的生命值

```cpp
#include <algorithm>
#include <iostream>

class Health {
    int current_;
    int max_;

public:
    Health(int max) : current_(max), max_(max) {}

    void take_damage(int amount) {
        current_ = std::clamp(current_ - amount, 0, max_);
    }

    void heal(int amount) {
        current_ = std::clamp(current_ + amount, 0, max_);
    }

    int current() const { return current_; }
};

int main() {
    Health hp(100);
    hp.take_damage(30);   // 70
    hp.take_damage(50);   // 20
    hp.heal(100);         // 100 (clamped to max)
    hp.take_damage(150);  // 0 (clamped to min)
    std::cout << hp.current() << "\n";  // 0
}
```

## 与手动 if-else 的对比

```cpp
#include <algorithm>

// 手动方式（容易出错）
int clamp_manual(int val, int lo, int hi) {
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

// 使用 std::clamp（简洁、安全、constexpr）
int clamp_modern(int val, int lo, int hi) {
    return std::clamp(val, lo, hi);
}

// 陷阱：手动实现容易写错边界
int clamp_wrong(int val, int lo, int hi) {
    return (val < lo) ? lo : (val > hi) ? hi : val;
    // 注意：lo 和 hi 相等时可能有问题
    // std::clamp 的实现更健壮
}
```

## 性能特性

`std::clamp` 的典型实现是一到两次比较，编译器会将其内联优化：

```cpp
// 典型实现（简化）
template <class T>
constexpr const T& clamp(const T& val, const T& lo, const T& hi) {
    return (val < lo) ? lo : (val > hi) ? hi : val;
}

// 编译器通常会生成与手动 if-else 相同的代码
// 但 std::clamp 更清晰、更不容易出错
```

## 编译器支持

| 编译器 | 最低版本 | 备注 |
|--------|---------|------|
| GCC | 7.0 | 完整支持 |
| Clang | 5.0 | 完整支持 |
| MSVC | 19.11 (VS 2017 15.3) | 完整支持 |

`std::clamp` 需要 C++17 编译模式。头文件为 `<algorithm>`。

## 最佳实践

- **任何需要限制值范围的场景**都应使用 `std::clamp`，取代手动 if-else。
- **确保 `lo <= hi`**：违反此条件是未定义行为。
- **`constexpr` 可用于编译期计算**：模板元编程中非常有用。
- **使用相同类型**：`val`、`lo`、`hi` 应为同一类型，避免隐式转换问题。
- **性能无开销**：编译器会将其优化为简单的比较指令。

## 常见陷阱

```cpp
// 陷阱 1：lo > hi 导致未定义行为
// std::clamp(5, 10, 0);  // UB！确保 lo <= hi

// 陷阱 2：类型不匹配
// int val = std::clamp(5, 0.0, 10.0);  // 可能有警告
int val = std::clamp(5.0, 0.0, 10.0);    // OK：统一类型

// 陷阱 3：引用返回的生命周期
const int& r = std::clamp(42, 0, 100);
// r 绑定到 42（字面量），安全
// 但如果传入临时对象，注意生命周期

// 陷阱 4：浮点数的 NaN
double nan = std::numeric_limits<double>::quiet_NaN();
double result = std::clamp(nan, 0.0, 1.0);
// NaN 的比较总是 false，结果未定义
```
