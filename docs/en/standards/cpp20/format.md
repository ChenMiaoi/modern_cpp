---
title: "`std::format`"
topic: unknown
feature: format
standard: N/A
status_checked_at: 2026-06-02
---
# `std::format`

## Overview

C++20 introduces the `<format>` library, providing Python-style type-safe formatting. Format strings are checked at compile time, with efficient runtime concatenation — a modern replacement for `sprintf` and `iostream`.

## Basic Usage

```cpp
#include <format>
#include <iostream>

int main() {
    auto s = std::format("Hello, {}! Age: {}", "Alice", 30);
    // "Hello, Alice! Age: 30"

    auto s2 = std::format("{1} and {0}", "world", "hello");
    // "hello and world"
}
```

## Format Specifiers

Syntax: `{[position][:format-spec]}`

### Integers and Floating-Point

```cpp
std::format("{:d}", 42);          // "42"
std::format("{:x}", 255);         // "ff"
std::format("{:#x}", 255);        // "0xff"
std::format("{:08d}", 42);        // "00000042"
std::format("{:+d}", 42);         // "+42"

std::format("{:.2f}", 3.14159);   // "3.14"
std::format("{:.2e}", 1234.5);    // "1.23e+03"
```

### Alignment and Fill

```cpp
std::format("{:<10}", "left");    // "left      "
std::format("{:>10}", "right");   // "     right"
std::format("{:^10}", "center");  // "  center  "
std::format("{:*^10}", "hi");     // "****hi****"
std::format("{:{}}", "pad", 10);  // "       pad" (dynamic width)
```

## Custom Type Formatting

Specialize `std::formatter`:

```cpp
#include <format>

struct Color { uint8_t r, g, b; };

template <>
struct std::formatter<Color> : std::formatter<std::string> {
    auto format(const Color& c, auto& ctx) const {
        return std::formatter<std::string>::format(
            std::format("#{:02x}{:02x}{:02x}", c.r, c.g, c.b), ctx);
    }
};
// Color{255, 0, 0} → "#ff0000"
```

### Supporting Format Specs

```cpp
struct Point { double x, y; };

template <>
struct std::formatter<Point> {
    constexpr auto parse(auto& ctx) {
        auto it = ctx.begin();
        precision = -1;
        if (it != ctx.end() && *it == '.') {
            ++it; precision = 0;
            while (it != ctx.end() && *it >= '0' && *it <= '9')
                precision = precision * 10 + (*it++ - '0');
        }
        ctx.advance_to(it);
        return it;
    }

    auto format(const Point& p, auto& ctx) const {
        if (precision >= 0)
            return std::format_to(ctx.out(), "({:.{}f}, {:.{}f})",
                                   p.x, precision, p.y, precision);
        return std::format_to(ctx.out(), "({}, {})", p.x, p.y);
    }
    int precision = -1;
};

// std::format("{:.2f}", Point{3.14159, 2.71828}) → "(3.14, 2.72)"
```

## Compile-Time Format String Checking

```cpp
std::string s1 = std::format("{} {}", 1, 2);           // OK
// std::string s2 = std::format("{} {} {}", 1, 2);     // compile error: missing argument
```

## `vformat` Runtime Formatting

```cpp
std::string fmt = "{}";
auto s = std::vformat(fmt, std::make_format_args(42));  // runtime

std::string dynamic(std::string_view fmt, auto&&... args) {
    return std::vformat(fmt, std::make_format_args(args...));
}
```

## Output to Iterators

```cpp
std::vector<char> buf;
std::format_to(std::back_inserter(buf), "x={}, y={}", 10, 20);

char small[8];
auto r = std::format_to_n(small, sizeof(small) - 1, "Hello, {}!", "World");
*r.out = '\0';  // truncation protection
```

## Comparison with `iostream` / `sprintf`

| Feature | `std::format` | `iostream` | `sprintf` |
|---------|---------------|------------|-----------|
| Type safety | **Compile-time checked** | Yes | No |
| Performance | **Fast** | Slow | Fast |
| Safety | Safe | Safe | Buffer overflow |

```cpp
std::cout << std::setw(10) << std::setfill('0') << 42;  // iostream
std::cout << std::format("{:010}", 42);                   // format
```

## `format_string` Constraint

```cpp
template <typename... Args>
void log(std::format_string<Args...> fmt, Args&&... args) {
    auto msg = std::format(fmt, std::forward<Args>(args)...);
}
log("User {} from {}", "alice", "127.0.0.1");  // compile-time checked
```

## Common Pitfalls

```cpp
std::format("{{literal}}");      // "{literal}" (brace escaping)
std::format("{}", true);        // "true", not "1"
std::format("{:d}", true);      // "1"

// Custom types must specialize formatter
struct MyType { int x; };
// std::format("{}", MyType{1});  // compile error
```

## Summary

- Type-safe, compile-time-checked, high-performance formatting with syntax similar to Python's `.format()`.
- Custom types via specializing `std::formatter`.
- Prefer `std::format_string<Args...>` to constrain function parameters.
- `vformat` is for scenarios with runtime format strings.
