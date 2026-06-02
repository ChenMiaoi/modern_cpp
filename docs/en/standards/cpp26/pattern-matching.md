---
title: C++26 Pattern Matching
status_checked_at: 2026-06-01
topic: unknown
feature: pattern-matching
standard: N/A
---


# C++26 Pattern Matching

## Overview

Pattern matching aims to provide C++ with Rust-like `match` expression-based value matching capabilities.

**Proposal status:** P2688 (`inspect` expression) is under active discussion and has not yet been officially accepted for C++26.

## `inspect` Expression

```cpp
int classify(int x) {
    return inspect (x) {
        0            => 0;
        1 | 2 | 3   => 1;       // Multi-value match
        _            => -1;      // Wildcard
    };
}
```

## Range Matching

```cpp
int classify_char(char c) {
    return inspect (c) {
        'a' .. 'z'  => 0;
        'A' .. 'Z'  => 1;
        '0' .. '9'  => 2;
        _            => 3;
    };
}
```

## variant / optional / Structured Binding

```cpp
#include <variant>
#include <optional>
#include <format>

using Value = std::variant<int, double, std::string>;

std::string format_value(Value const& v) {
    return inspect (v) {
        (int i)          => std::format("int: {}", i);
        (double d)       => std::format("double: {}", d);
        (std::string s)  => std::format("str: {}", s);
    };
}

std::string unwrap(std::optional<int> const& opt) {
    return inspect (opt) {
        (int value)     => std::format("got {}", value);
        (std::nullopt)  => "empty";
    };
}

struct Point { int x; int y; };
std::string classify_point(Point const& p) {
    return inspect (p) {
        (Point { .x = 0, .y = 0 })  => "origin";
        (Point { .x = x, .y = 0 })  => std::format("x-axis at {}", x);
        (Point { .x = 0, .y = y })  => std::format("y-axis at {}", y);
        (Point { .x = x, .y = y })  => std::format("({}, {})", x, y);
    };
}
```

Nested structs and guard clauses:

```cpp
struct Line { Point start; Point end; };
bool is_horizontal(Line const& l) {
    return inspect (l) {
        (Line { .start = { .y = y1 }, .end = { .y = y2 } }) => y1 == y2;
    };
}

int categorize(int x) {
    return inspect (x) {
        0                      => 0;
        (n) if (n < 0)        => -1;
        (n) if (n % 2 == 0)   => 2;
        (n)                    => 3;
    };
}
```

## Comparison with visit + overloaded

```cpp
// C++20 — requires helper template
template <typename... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
auto fmt = std::visit(overloaded{
    [](int i)          { return std::format("int: {}", i); },
    [](double d)       { return std::format("double: {}", d); },
}, v);

// C++26 — declarative
return inspect (v) {
    (int i)          => std::format("int: {}", i);
    (double d)       => std::format("double: {}", d);
};
```

| Dimension | `visit` + `overloaded` | `inspect` |
|-----------|----------------------|-----------|
| Readability | Requires helper template | Declarative |
| Nested matching | Multiple levels of visit | Natively supported |
| Guard conditions | if + throw | `if` clause |
| Struct destructuring | Not supported | Supported |

## Exhaustiveness Checking

```cpp
enum class Direction { Up, Down, Left, Right };
std::string to_arrow(Direction d) {
    return inspect (d) {
        Direction::Up    => "↑";
        Direction::Down  => "↓";
        Direction::Left  => "←";
        Direction::Right => "→";
    };  // No default needed
}
```

## Implementation Status

| Compiler | Status |
|----------|--------|
| GCC | Experimental branch |
| Clang / MSVC | Not yet implemented |

## Summary

`inspect` brings structured pattern matching to C++, significantly outperforming `visit` + `overloaded` in readability, nested matching, exhaustiveness checking, and struct destructuring. Although not yet officially accepted for C++26, the design direction is clear.
