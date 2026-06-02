---
title: C++26 反射 (Reflection)
status_checked_at: 2026-06-01
---

# C++26 反射 (Reflection)

## 概述

反射是 C++26 的编译期类型自省机制，通过 `^` 生成元对象，`[: :]` splice 注入代码。是 C++ 历史上最重要的语言特性之一。

**提案状态：** P2996R7 已被 C++26 接受。P3394（注解）仍在演进中。

## 元对象与 `^` 操作符

```cpp
#include <meta>

constexpr auto meta_int = ^int;
constexpr auto meta_vec = ^std::vector<int>;
constexpr auto meta_ns  = ^std;          // 命名空间元对象
constexpr auto meta_fn  = ^std::sort;    // 函数元对象
```

元对象不透明，只能通过查询函数和 splice 使用。

## 查询函数

```cpp
#include <meta>
#include <print>

enum class Color { Red, Green, Blue, Alpha };

constexpr auto colors = std::meta::enumerators_of(^Color);
static_assert(colors.size() == 4);
constexpr auto first = std::meta::name_of(colors[0]);  // "Red"
```

| 函数 | 返回 | 说明 |
|------|------|------|
| `name_of(^T)` | `string_view` | 名称 |
| `members_of(^T)` | `vector<info>` | 所有成员 |
| `enumerators_of(^T)` | `vector<info>` | 枚举值 |
| `nonstatic_data_members_of(^T)` | `vector<info>` | 数据成员 |
| `type_of(member)` | `info` | 成员类型 |
| `value_of<T>(^e)` | `T` | 编译期常量值 |

## splice 操作符 `[: :]`

```cpp
using T = [:^int:];           // T = int
Point p{3, 4};
[:^int:] val = 42;            // int val = 42;

constexpr auto mems = std::meta::nonstatic_data_members_of(^Point);
```

## 枚举转字符串

```cpp
#include <meta>
#include <print>

enum class Status { Pending, Active, Suspended, Closed };

consteval std::string_view enum_to_string(Status s) {
    template for (constexpr auto e : std::meta::enumerators_of(^Status)) {
        if (s == [:e:]) return std::meta::name_of(e);
    }
    return "unknown";
}

std::println("{}", enum_to_string(Status::Active));  // Active
```

无需手动映射表或宏，新增枚举值不会遗漏。

## 自动化序列化

```cpp
#include <meta>
#include <string>
#include <sstream>

struct User { std::string name; int age; bool active; };

template <typename T>
std::string to_json(T const& obj) {
    std::ostringstream os; os << "{";
    bool first = true;
    template for (constexpr auto mem : std::meta::nonstatic_data_members_of(^T)) {
        if (!first) os << ", "; first = false;
        os << "\"" << std::meta::name_of(mem) << "\": ";
        if constexpr (std::is_same_v<[: std::meta::type_of(mem) :], std::string>)
            os << "\"" << obj.[:mem:] << "\"";
        else if constexpr (std::is_same_v<[: std::meta::type_of(mem) :], bool>)
            os << (obj.[:mem:] ? "true" : "false");
        else os << obj.[:mem:];
    }
    os << "}"; return os.str();
}
// to_json(User{"Alice", 30, true}) => {"name": "Alice", "age": 30, "active": true}
```

## 与模板元编程对比

| 维度 | 传统模板元编程 | C++26 反射 |
|------|---------------|-----------|
| 语法 | SFINAE、`if constexpr` | `^`、`[: :]`、`template for` |
| 枚举遍历 | 需宏或外部工具 | `enumerators_of` 原生 |
| 成员遍历 | 需 tuple 化 | `nonstatic_data_members_of` |
| 学习曲线 | 极陡 | 中等 |

反射补充而非替代模板元编程。

## 相关提案

- **P2996**：核心反射 + splice
- **P3394**：注解语法 `[[=attr(...)]]`
- **P3436**：反射相关 constexpr 扩展
- **P3068**：constexpr 异常

## 实现状态

| 编译器 | 状态 |
|--------|------|
| Clang/LLVM | 参考实现，`-freflection` 实验性 |
| GCC | 实验性分支 |
| MSVC | 跟进中 |

## 总结

C++26 反射通过 `^` 和 `[: :]` 带来编译期类型自省。结合 `template for` 和 constexpr 容器扩展，使枚举转字符串、自动化序列化等过去需大量模板技巧的任务变得简洁直接。
