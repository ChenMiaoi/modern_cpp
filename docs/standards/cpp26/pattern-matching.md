---
title: C++26 模式匹配 (Pattern Matching)
status_checked_at: 2026-06-01
---

# C++26 模式匹配 (Pattern Matching)

## 概述

模式匹配旨在为 C++ 提供类似 Rust `match` 的表达式化值匹配能力。

**提案状态：** P2688（`inspect` 表达式）处于活跃讨论阶段，尚未被 C++26 正式接受。

## `inspect` 表达式

```cpp
int classify(int x) {
    return inspect (x) {
        0            => 0;
        1 | 2 | 3   => 1;       // 多值匹配
        _            => -1;      // 通配符
    };
}
```

## 范围匹配

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

## variant / optional / 结构体绑定

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

嵌套结构体和守卫子句：

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

## 与 visit + overloaded 对比

```cpp
// C++20 — 需辅助模板
template <typename... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
auto fmt = std::visit(overloaded{
    [](int i)          { return std::format("int: {}", i); },
    [](double d)       { return std::format("double: {}", d); },
}, v);

// C++26 — 声明式
return inspect (v) {
    (int i)          => std::format("int: {}", i);
    (double d)       => std::format("double: {}", d);
};
```

| 维度 | `visit` + `overloaded` | `inspect` |
|------|----------------------|-----------|
| 可读性 | 需辅助模板 | 声明式 |
| 嵌套匹配 | 多层 visit | 原生支持 |
| 守卫条件 | if + throw | `if` 子句 |
| 结构体解构 | 不支持 | 支持 |

## 穷尽性检查

```cpp
enum class Direction { Up, Down, Left, Right };
std::string to_arrow(Direction d) {
    return inspect (d) {
        Direction::Up    => "↑";
        Direction::Down  => "↓";
        Direction::Left  => "←";
        Direction::Right => "→";
    };  // 无需 default
}
```

## 实现状态

| 编译器 | 状态 |
|--------|------|
| GCC | 实验性分支 |
| Clang / MSVC | 尚未实现 |

## 总结

`inspect` 为 C++ 带来结构化模式匹配，在可读性、嵌套匹配、穷尽性检查和结构体解构方面显著优于 `visit` + `overloaded`。虽尚未被 C++26 正式接受，但设计方向明确。
