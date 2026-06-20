---
title: C++17 std::clamp
topic: unknown
feature: std-clamp
standard: N/A
status_checked_at: 2026-06-20
---
# C++17 std::clamp

## Overview

`std::clamp` is a free function introduced in C++17 within `<algorithm>` that **constrains a value within specified lower and upper bounds**. It returns `lo` (when `val < lo`), `hi` (when `val > hi`), or `val` (when in range). It is widely used in GUI development, interpolation, physics simulation, and other scenarios requiring boundary limiting.

## Function Signature

```cpp
#include <algorithm>

template <class T>
constexpr const T& clamp(const T& val, const T& lo, const T& hi);
```

Semantics:
- If `val < lo`, returns `lo`
- If `val > hi`, returns `hi`
- Otherwise returns `val`

**Requirement**: `lo <= hi`, otherwise behavior is undefined.

## Basic Usage

```cpp
#include <algorithm>
#include <iostream>

int main() {
    int a = std::clamp(5, 0, 10);    // 5 (in range)
    int b = std::clamp(-5, 0, 10);   // 0 (below lower bound)
    int c = std::clamp(15, 0, 10);   // 10 (above upper bound)

    std::cout << a << " " << b << " " << c << "\n";  // 5 0 10
}
```

## Custom Comparators

```cpp
#include <algorithm>
#include <string>
#include <cctype>

// Custom comparison: case-insensitive sort
bool case_insensitive_less(char a, char b) {
    return std::tolower(a) < std::tolower(b);
}

int main() {
    // Standard version uses operator<
    // C++17 standard only provides the default version, but you can wrap it
    int val = std::clamp(100, 0, 255);  // 100
}
```

## constexpr Support

`std::clamp` is `constexpr` in C++17 and can be used at compile time:

```cpp
#include <algorithm>

// Compile-time computation
constexpr int clamped = std::clamp(42, 0, 100);  // 42
constexpr int low = std::clamp(-10, 0, 100);     // 0
constexpr int high = std::clamp(200, 0, 100);    // 100

// Used in template parameters
template <int N>
struct ClampedValue {
    static constexpr int value = std::clamp(N, 0, 255);
};

static_assert(ClampedValue<300>::value == 255);
static_assert(ClampedValue<-5>::value == 0);
static_assert(ClampedValue<128>::value == 128);
```

## GUI Pixel Coordinate Clamping

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

## Interpolation and Animation

```cpp
#include <algorithm>
#include <iostream>

double lerp(double a, double b, double t) {
    return a + (b - a) * t;
}

int main() {
    double start = 0.0, end = 100.0;

    // t should be clamped to [0, 1]
    double t = 1.5;
    double t_clamped = std::clamp(t, 0.0, 1.0);  // 1.0

    double value = lerp(start, end, t_clamped);
    std::cout << "lerp: " << value << "\n";  // 100.0

    // Gradient color calculation
    auto progress = [](double t) {
        t = std::clamp(t, 0.0, 1.0);
        int r = static_cast<int>(255 * (1.0 - t));
        int g = static_cast<int>(255 * t);
        return (r << 16) | (g << 8);
    };

    int color = progress(0.5);  // 0x7F7F00
}
```

## Game Health System

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

## Comparison with Manual if-else

```cpp
#include <algorithm>

// Manual approach (error-prone)
int clamp_manual(int val, int lo, int hi) {
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

// Using std::clamp (concise, safe, constexpr)
int clamp_modern(int val, int lo, int hi) {
    return std::clamp(val, lo, hi);
}

// Pitfall: manual implementation can get boundary wrong
int clamp_wrong(int val, int lo, int hi) {
    return (val < lo) ? lo : (val > hi) ? hi : val;
    // Note: may have issues when lo == hi
    // std::clamp's implementation is more robust
}
```

## Performance Characteristics

A typical `std::clamp` implementation involves one to two comparisons, which compilers inline and optimize:

```cpp
// Typical implementation (simplified)
template <class T>
constexpr const T& clamp(const T& val, const T& lo, const T& hi) {
    return (val < lo) ? lo : (val > hi) ? hi : val;
}

// Compilers typically generate the same code as manual if-else
// But std::clamp is clearer and less error-prone
```

## Compiler Support

| Compiler | Minimum Version | Notes |
|----------|----------------|-------|
| GCC | 7.0 | Full support |
| Clang | 5.0 | Full support |
| MSVC | 19.11 (VS 2017 15.3) | Full support |

`std::clamp` requires C++17 compilation mode. The header is `<algorithm>`.

## Best Practices

- **Any scenario requiring value range limiting** should use `std::clamp` instead of manual if-else.
- **Ensure `lo <= hi`**: violating this condition is undefined behavior.
- **`constexpr` works for compile-time computation**: very useful in template metaprogramming.
- **Use the same type**: `val`, `lo`, and `hi` should be the same type to avoid implicit conversion issues.
- **Zero overhead**: compilers optimize it to simple comparison instructions.

## Common Pitfalls

```cpp
// Pitfall 1: lo > hi causes undefined behavior
// std::clamp(5, 10, 0);  // UB! Ensure lo <= hi

// Pitfall 2: type mismatch
// int val = std::clamp(5, 0.0, 10.0);  // may warn
int val = std::clamp(5.0, 0.0, 10.0);    // OK: uniform type

// Pitfall 3: reference return lifetime
const int& r = std::clamp(42, 0, 100);
// r binds to 42 (literal), safe
// But watch lifetime when passing temporaries

// Pitfall 4: NaN with floating point
double nan = std::numeric_limits<double>::quiet_NaN();
double result = std::clamp(nan, 0.0, 1.0);
// NaN comparisons are always false, result is undefined
```
