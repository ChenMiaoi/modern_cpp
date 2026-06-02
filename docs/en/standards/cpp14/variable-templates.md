---
title: "C++14 Variable Templates"
topic: unknown
feature: variable-templates
standard: N/A
status_checked_at: 2026-06-02
---
# C++14 Variable Templates

## Overview

C++14 introduces variable templates, allowing the definition of compile-time constant variables that depend on template parameters. In C++11, achieving a similar effect required static member functions (`value`) or `constexpr` functions, resulting in verbose code. Variable templates make type-parameterized constant definitions intuitive.

## Syntax

```cpp
template <typename T>
constexpr T variable_name = initial_value;

// Usage
auto val = variable_name<double>;
```

## Code Examples

### Basic Usage: Mathematical Constants

```cpp
#include <cmath>
#include <iostream>

// Pi constant at different precisions
template <typename T>
constexpr T pi = T(3.141592653589793238462643383279502884L);

int main() {
    std::cout << pi<float>  << '\n';   // 3.14159 (float precision)
    std::cout << pi<double> << '\n';   // 3.141592653589793 (double precision)
}
```

### With Type Traits

```cpp
#include <type_traits>

// C++11 approach: requires ::value
template <typename T>
void check() {
    static_assert(std::is_integral<T>::value, "must be integral");
}

// C++14 standard library already provides _v-suffixed variable templates
// Equivalent to std::is_integral<T>::value
template <typename T>
void check_v2() {
    static_assert(std::is_integral_v<T>, "must be integral");
}

// Custom type trait variable template
template <typename T>
constexpr bool is_small = sizeof(T) <= sizeof(int);

static_assert(is_small<char>);
static_assert(!is_small<double>);
```

### Constrained Variable Templates

```cpp
#include <type_traits>

// Valid only for arithmetic types
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
constexpr T zero = T(0);

// Specialization: returns epsilon for floating-point types instead of zero
template <typename T>
constexpr T epsilon = T(1e-10);

template <>
constexpr float epsilon<float> = 1e-6f;

template <>
constexpr double epsilon<double> = 1e-10;
```

### Variable Templates in Classes (C++14 Static Data Member Templates)

```cpp
#include <cstddef>

struct Config {
    // In-class variable template — implicitly inline in C++14 (requires C++17 inline in non-template classes)
    template <typename T>
    static constexpr std::size_t max_size = 1024;
};

// Out-of-class specialization
template <>
constexpr std::size_t Config::max_size<char> = 4096;

template <>
constexpr std::size_t Config::max_size<double> = 128;
```

### Compile-Time Computation Scenarios

```cpp
#include <cstddef>

// Compile-time factorial table
template <std::size_t N>
constexpr std::size_t factorial = N * factorial<N - 1>;

template <>
constexpr std::size_t factorial<0> = 1;

// Used for array size
int table[factorial<5>];  // Array of 120 elements

// Compile-time unit conversion
template <typename T>
constexpr T inches_to_cm = T(2.54);

template <typename T>
constexpr T miles_to_km = T(1.60934);
```

### Variable Templates vs constexpr Functions

```cpp
// Approach A: constexpr function
constexpr double pi_func() { return 3.141592653589793; }

// Approach B: variable template
template <typename T = double>
constexpr T pi_var = T(3.141592653589793238462643383279502884L);

// Advantages of variable templates:
// 1. More concise syntax: pi<double> vs pi_func() — though function style is also common
// 2. Specializable: pi<long double> can provide higher precision
// 3. Can be passed as template arguments

template <typename T, T Value>
struct Constant {};

// Variable templates can participate in type construction
Constant<double, pi_var<double>> c;  // OK
// Constant<double, pi_func()> c2;   // Non-type template parameters as floating-point allowed only from C++20
```

## Best Practices

1. **Prefer variable templates over `::value` suffix**: When defining custom type traits, provide both `::value` and `_v` variable templates for consistency with the standard library.
2. **Use variable templates for mathematical constants**: `pi<T>` is more elegant than multiple `#define`s or `constexpr auto pi_f = ...; constexpr auto pi_d = ...;`.
3. **Be aware of template instantiation overhead**: Variable templates may be instantiated at each use site, but the linker merges identical instances, so no duplicate storage is produced.
4. **Variable templates cannot be constrained by `constexpr` functions (C++14/17)**: When SFINAE constraints are needed, use default template parameters with `std::enable_if`.
5. **In-class static variable templates are implicitly inline from C++17**: In C++14, placing the definition of an in-class static variable template in a header may cause multiple definition errors — watch out for ODR violations.
6. **Avoid name collisions with functions/types**: Variable templates introduce new namespace-scoped variable names; ensure they do not conflict with macros, functions, or type names.
