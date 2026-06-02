---
title: "三路比较运算符：`operator<=>`"
topic: unknown
feature: spaceship-operator
standard: N/A
status_checked_at: 2026-06-02
---
# 三路比较运算符：`operator<=>`

## 概述

C++20 引入三路比较运算符 `<=>`（太空船运算符），一次调用确定两个对象的相等或排序关系。编译器据此自动合成 `==`、`!=`、`<`、`>`、`<=`、`>=`，大幅减少样板代码。

## 默认三路比较

```cpp
struct Point {
    int x, y, z;
    auto operator<=>(const Point&) const = default;
};

Point a{1, 2, 3}, b{1, 2, 4};
bool eq = (a == b);  // false
bool lt = (a < b);   // true（逐字段 lexicographic）
```

`= default` 要求所有成员均可比较，编译器按声明顺序逐字段生成比较逻辑。

## 比较类别

| 类型 | 语义 | 典型场景 |
|------|------|----------|
| `std::strong_ordering` | 无等价可替代对象 | 整数、枚举、指针 |
| `std::weak_ordering` | 等价但可区分 | 字典序、大小写不敏感 |
| `std::partial_ordering` | 部分值不可比较 | 浮点数（NaN） |

```cpp
#include <compare>

struct Version {
    int major, minor, patch;
    auto operator<=>(const Version&) const = default;
    // 三字段均为 int → strong_ordering → == 自动合成
};

struct SensorReading {
    double value;
    std::partial_ordering operator<=>(const SensorReading& rhs) const {
        return value <=> rhs.value;  // NaN 产生 unordered
    }
    bool operator==(const SensorReading& rhs) const {
        return value == rhs.value;
    }
};
```

## 自定义比较

```cpp
#include <compare>

struct Circle {
    double radius, x, y;
    // 仅按半径排序
    std::weak_ordering operator<=>(const Circle& rhs) const {
        if (auto c = radius <=> rhs.radius; c != 0) return c;
        return std::weak_ordering::equivalent;
    }
    bool operator==(const Circle& rhs) const {
        return radius == rhs.radius;
    }
};
```

## `==` 合成规则

- `operator<=>` 返回 `strong_ordering` → 编译器自动合成 `operator==`。
- 返回 `weak_ordering` 或 `partial_ordering` → **不会**自动合成 `==`，必须手动提供。
- 反向运算符（如 `b < a`）由编译器从 `a <=> b` 推导。

## 与标准库集成

```cpp
#include <algorithm>
#include <vector>

struct Record {
    int id;
    std::string name;
    auto operator<=>(const Record&) const = default;
};

void sort_records(std::vector<Record>& v) {
    std::sort(v.begin(), v.end());  // <=> 自动生成所需比较
}
```

## 混合类型比较

```cpp
struct Meter {
    double value;
    explicit Meter(double v) : value(v) {}
    std::partial_ordering operator<=>(double rhs) const {
        return value <=> rhs;
    }
    bool operator==(double rhs) const { return value == rhs; }
};

// Meter(3.0) <=> 5.0 → partial_ordering::less
// Meter(3.0) == 3.0   → true
```

混合类型需手动实现，`<=>` 不会跨类型自动推导。

## 常见陷阱

```cpp
// 陷阱 1：浮点默认比较含 NaN
struct Bad { float val; auto operator<=>(const Bad&) const = default; };
// val 为 NaN 时 <=> 返回 partial_ordering::unordered

// 陷阱 2：指针成员
struct WithPtr { int* p; auto operator<=>(const WithPtr&) const = default; };
// 指针 <=> 要求指向同一数组，否则 UB

// 陷阱 3：= default 不比较基类（除非基类也提供 <=>）
struct Base { int id; auto operator<=>(const Base&) const = default; };
struct Derived : Base {
    std::string name;
    auto operator<=>(const Derived& rhs) const {
        if (auto c = Base::operator<=>(rhs); c != 0) return c;
        return name <=> rhs.name;
    }
};
```

## 与 C++17 `std::tie` 比较

```cpp
// C++17：手写六个运算符或 tie
struct Old {
    int a, b;
    bool operator<(const Old& r) const { return std::tie(a,b) < std::tie(r.a,r.b); }
    bool operator==(const Old& r) const { return std::tie(a,b) == std::tie(r.a,r.b); }
    // 还需 !=, >, <=, >= …
};

// C++20：一行搞定
struct New { int a, b; auto operator<=>(const New&) const = default; };
```

## 总结

- 默认 `= default` 按声明顺序逐字段 lexicographic 比较。
- 返回类型决定可用运算符；`strong_ordering` 自动合成 `==`。
- 浮点和含指针成员需注意语义陷阱。
- 混合类型比较需手动实现 `operator<=>(const OtherType&)`。
