---
title: C++17 std::optional
topic: cpp17-basics
feature: optional
standard: C++17
status_checked_at: 2026-06-01
exercises:
  - exercises/cpp17-basics/optional1.cpp
solutions:
  - exercises/solutions/optional1.cpp
---
# C++17 std::optional

## 概述

`std::optional<T>` 是 C++17 在 `<optional>` 中引入的工具类模板，表示一个**可能存在也可能不存在的值**。它为"无值"提供类型安全的表达方式，取代了哨兵值（`-1`、`nullptr`）、`std::pair<T, bool>` 或输出参数等传统手法。特别适合作为函数返回类型：操作可能成功返回结果，也可能无结果。

## 基本构造

```cpp
#include <optional>
#include <string>

std::optional<int> opt_a{42};           // 有值
std::optional<std::string> opt_b{"hello"};
std::optional<int> empty{};             // 空
std::optional<int> empty2 = std::nullopt; // 空，使用 nullopt 常量
std::optional<double> opt_c = 3.14;     // 隐式转换
```

`std::nullopt` 是表示"无值"的特殊常量，类型为 `std::nullopt_t`。

## 状态查询与值访问

```cpp
std::optional<int> opt{42};
std::optional<int> empty;

// has_value() / operator bool
if (opt.has_value()) { /* 有值 */ }
if (opt) { /* 等价写法 */ }

// operator* —— 不检查是否为空，空时 UB
int v1 = *opt;

// value() —— 无值时抛 std::bad_optional_access
try {
    int v2 = empty.value();
} catch (const std::bad_optional_access& e) {
    std::cout << e.what() << "\n";
}

// value_or(default) —— 无值时返回默认值
int v3 = opt.value_or(0);      // 42
int v4 = empty.value_or(0);    // 0
```

| 方法 | 有值时 | 无值时 |
|------|--------|--------|
| `operator*` / `operator->` | 返回值/指针 | **未定义行为** |
| `value()` | 返回引用 | 抛 `bad_optional_access` |
| `value_or(default)` | 返回值副本 | 返回 `default` |

## emplace：就地构造

```cpp
#include <optional>

struct Widget {
    int id;
    std::string name;
    Widget(int i, std::string n) : id(i), name(std::move(n)) {}
};

std::optional<Widget> opt;
opt.emplace(1, "first");    // 直接在 optional 内部构造
opt.emplace(2, "second");   // 先析构旧值，再构造新对象
```

`emplace` 避免额外的移动或拷贝，构造开销大时尤为重要。

## 比较运算符

```cpp
std::optional<int> a{1}, b{2}, empty;

a < b;                    // true：按值比较
a == 1;                   // true：与裸值比较
empty == std::nullopt;    // true
empty < a;                // true：nullopt 小于任何有值 optional
```

支持完整的 `<`、`>`、`==`、`!=`、`<=`、`>=` 比较。

## 典型用法：替代错误码

```cpp
#include <optional>
#include <map>
#include <string>

std::optional<int> find_user_id(
    const std::map<std::string, int>& db, const std::string& name)
{
    auto it = db.find(name);
    if (it != db.end()) return it->second;
    return std::nullopt;
}

void demo() {
    std::map<std::string, int> users = {{"alice", 1}, {"bob", 2}};

    if (auto id = find_user_id(users, "alice")) {
        std::cout << "found: " << *id << "\n";
    }

    // 使用 value_or 提供默认值
    int uid = find_user_id(users, "eve").value_or(-1);
}
```

相比返回 `-1` 或 `nullptr`，`std::optional` 使"可能无值"的语义在类型系统中显式可见。

## C++23 单子操作（提及）

C++23 添加了链式操作减少样板代码：

```cpp
// C++23 — transform（map）
auto doubled = std::optional{42}.transform([](int n) { return n * 2; });
// optional<int>{84}

// C++23 — and_then（flat_map）
auto result = std::optional{42}.and_then(
    [](int n) -> std::optional<std::string> {
        return n > 0 ? std::optional{std::to_string(n)} : std::nullopt;
    });

// C++23 — or_else
auto fallback = std::optional<int>{}.or_else(
    []() -> std::optional<int> { return 0; });
```

## reference_wrapper 替代引用

`std::optional<T&>` 不被标准支持。替代方案：

```cpp
#include <optional>
#include <functional>

int x = 42;
std::optional<std::reference_wrapper<int>> opt_ref{x};
if (opt_ref) {
    opt_ref->get() = 100;  // x 现在是 100
}
```

## 最佳实践

- **函数可能返回"无结果"时**，优先使用 `std::optional<T>` 返回类型。
- **访问值之前**，始终检查 `has_value()` 或使用 `value_or()`。
- **使用 `emplace`** 就地构造复杂对象，避免多余拷贝/移动。
- **不要用 `optional` 表示错误**——若需携带错误信息，使用 `std::expected`（C++23）或自定义错误类型。
- **不要嵌套 `optional<optional<T>>`**——两种"空"语义令人困惑。

## 常见陷阱

```cpp
// 陷阱 1：解引用空 optional（UB）
std::optional<int> empty;
// int x = *empty;  // 未定义行为！必须先检查

// 陷阱 2：value() 在禁异常的项目中不可用
// int x = empty.value();  // 抛异常——用 value_or 或手动检查替代

// 陷阱 3：optional 的值拷贝开销
std::optional<std::string> opt{"hello"};  // 构造一次 string
std::optional<std::string> opt2;
opt2.emplace("hello");                    // 直接在内部构造，更高效

// 陷阱 4：operator-> 的行为
std::optional<std::string> opt_s{"test"};
opt_s->size();    // OK，等价于 (*opt_s).size()
// std::optional<std::string> empty_s;
// empty_s->size();  // UB！
```
