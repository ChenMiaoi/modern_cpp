---
title: "std::ratio"
topic: unknown
feature: ratio
standard: N/A
status_checked_at: 2026-06-20
---
# std::ratio

## Overview

`std::ratio` is a compile-time rational arithmetic library introduced in C++11, defined in the `<ratio>` header. It enables exact fractional arithmetic at compile time with no floating-point precision loss. Its primary use is in `std::chrono` duration definitions, but it can also be used in any scenario requiring compile-time precise numeric computation.

```cpp
#include <ratio>
```

## Basic Definition of std::ratio

`std::ratio<Num, Denom>` represents the fraction `Num / Denom`, where `Num` is the numerator and `Denom` is the denominator. The denominator defaults to 1.

```cpp
#include <ratio>
#include <type_traits>

// Represents 3/4
using three_fourths = std::ratio<3, 4>;

// Represents 5/1 = 5
using five = std::ratio<5>;

// Represents 1/1000 = 0.001
using milli = std::ratio<1, 1000>;

static_assert(three_fourths::num == 3, "");
static_assert(three_fourths::den == 4, "");
```

### Compile-Time Reduction

`std::ratio` automatically reduces fractions to simplest form:

```cpp
using result = std::ratio<6, 8>;  // automatically reduces to 3/4

static_assert(result::num == 3, "");
static_assert(result::den == 4, "");
```

## Arithmetic Operations

### ratio_add — Addition

```cpp
#include <ratio>
#include <type_traits>

// 1/3 + 1/6 = 2/6 + 1/6 = 3/6 = 1/2
using sum = std::ratio_add<std::ratio<1, 3>, std::ratio<1, 6>>;

static_assert(sum::num == 1, "");
static_assert(sum::den == 2, "");
```

### ratio_subtract — Subtraction

```cpp
// 3/4 - 1/4 = 2/4 = 1/2
using diff = std::ratio_subtract<std::ratio<3, 4>, std::ratio<1, 4>>;

static_assert(diff::num == 1, "");
static_assert(diff::den == 2, "");
```

### ratio_multiply — Multiplication

```cpp
// 2/3 * 3/5 = 6/15 = 2/5
using product = std::ratio_multiply<std::ratio<2, 3>, std::ratio<3, 5>>;

static_assert(product::num == 2, "");
static_assert(product::den == 5, "");
```

### ratio_divide — Division

```cpp
// (1/2) / (3/4) = (1/2) * (4/3) = 4/6 = 2/3
using quotient = std::ratio_divide<std::ratio<1, 2>, std::ratio<3, 4>>;

static_assert(quotient::num == 2, "");
static_assert(quotient::den == 3, "");
```

## Comparison Operations

### ratio_equal — Equality Comparison

```cpp
using a = std::ratio<2, 4>;  // reduces to 1/2
using b = std::ratio<3, 6>;  // reduces to 1/2

static_assert(std::ratio_equal<a, b>::value, "1/2 == 1/2");
static_assert(!std::ratio_equal<std::ratio<1, 3>, std::ratio<1, 4>>::value, "");
```

### ratio_less — Less-Than Comparison

```cpp
static_assert(std::ratio_less<std::ratio<1, 3>, std::ratio<1, 2>>::value, "1/3 < 1/2");
static_assert(!std::ratio_less<std::ratio<3, 4>, std::ratio<1, 2>>::value, "3/4 >= 1/2");
```

### ratio_greater — Greater-Than Comparison

```cpp
static_assert(std::ratio_greater<std::ratio<5, 6>, std::ratio<1, 2>>::value, "5/6 > 1/2");
```

## Predefined Unit Aliases

The `<ratio>` header provides a set of commonly used SI prefix aliases:

| Alias | Value | Alias | Value |
|-------|-------|-------|-------|
| `std::ratio<1>` | 1 | `std::ratio<-1>` | -1 |
| `std::ratio<1000>` | 1000 | `std::ratio<1000000>` | 1000000 |
| `std::ratio<1, 1000>` | 1/1000 | `std::ratio<1, 1000000>` | 1/1000000 |

```cpp
#include <ratio>

// Common aliases
using kilo   = std::ratio<1000>;           // 1000
using milli  = std::ratio<1, 1000>;        // 1/1000
using micro  = std::ratio<1, 1000000>;     // 1/1000000
using nano   = std::ratio<1, 1000000000>;  // 1/1000000000
```

## Integration with std::chrono

The most common use of `std::ratio` is in `std::chrono` for defining time units. Each `std::chrono::duration` template parameter is a `std::ratio`:

```cpp
#include <chrono>
#include <ratio>
#include <iostream>

int main() {
    // std::chrono::seconds period is std::ratio<1>
    // std::chrono::milliseconds period is std::ratio<1, 1000>
    // std::chrono::microseconds period is std::ratio<1, 1000000>

    using namespace std::chrono;

    seconds s(1);
    milliseconds ms = s;     // 1000ms
    microseconds us = ms;    // 1000000us

    std::cout << s.count() << '\n';   // 1
    std::cout << ms.count() << '\n';  // 1000
    std::cout << us.count() << '\n';  // 1000000

    // Custom time unit
    using fortnight = std::chrono::duration<long long, std::ratio<1209600>>;
    // 1 fortnight = 1209600 seconds (14 days)

    fortnight f(1);
    seconds equivalent = f;
    std::cout << equivalent.count() << '\n';  // 1209600
}
```

## Code Examples

### Compile-Time Unit Conversion

```cpp
#include <ratio>
#include <iostream>

template <typename FromRatio, typename ToRatio>
constexpr double convert(double value) {
    // value * FromRatio::num / FromRatio::den * ToRatio::den / ToRatio::num
    // simplified to: value * FromRatio::num * ToRatio::den / (FromRatio::den * ToRatio::num)
    return value * FromRatio::num * ToRatio::den
           / static_cast<double>(FromRatio::den * ToRatio::num);
}

int main() {
    using km_to_m = std::ratio<1000, 1>;     // 1 km = 1000 m
    using m_to_km = std::ratio<1, 1000>;     // 1 m = 1/1000 km

    double km = 5.0;
    double m = convert<km_to_m, std::ratio<1>>(km);  // convert to meters
    std::cout << km << " km = " << m << " m\n";       // 5 km = 5000 m

    double back_km = convert<std::ratio<1>, km_to_m>(m);  // convert back to km
    std::cout << m << " m = " << back_km << " km\n";     // 5000 m = 5 km
}
```

## Notes and Pitfalls

**Division by zero** — if the denominator is zero, the compiler produces a compile-time error (not a runtime error):

```cpp
// using bad = std::ratio<1, 0>;  // compile error: denominator must not be zero
```

**Overflow** — when the product of numerators or denominators exceeds the `long long` range, undefined behavior or compile errors may occur:

```cpp
// using huge = std::ratio<999999999999LL, 999999999999LL>;  // may overflow
```

**ratio is not a runtime type** — `std::ratio` is a purely compile-time construct with no runtime values. You cannot create `std::ratio` objects at runtime or use their values for dynamic computation:

```cpp
// These are compile-time static members, not runtime values
static_assert(std::ratio<3, 4>::num == 3, "");
```

**Conversion to floating-point** — `std::ratio` itself does not provide conversion to floating-point; manual calculation is required:

```cpp
using r = std::ratio<3, 4>;
double val = static_cast<double>(r::num) / r::den;  // 0.75
```

## Compiler Support

| Compiler | Supported Since | Notes |
|----------|-----------------|-------|
| GCC | 4.5+ | Full support |
| Clang | 3.1+ | Full support |
| MSVC | 2012 (17.0)+ | Full support |

`std::ratio` is the foundational infrastructure for `std::chrono`. While direct usage scenarios are limited, understanding it helps deepen comprehension of the C++ time library design. It embodies the C++11 philosophy of "zero-overhead abstraction" — exact compile-time computation with no runtime overhead whatsoever.
