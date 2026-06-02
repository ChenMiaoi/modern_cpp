---
title: "Three-Way Comparison Operator: `operator<=>`"
topic: unknown
feature: spaceship-operator
standard: N/A
status_checked_at: 2026-06-02
---
# Three-Way Comparison Operator: `operator<=>`

## Overview

C++20 introduces the three-way comparison operator `<=>` (spaceship operator), which determines equality or ordering relationships between two objects in a single call. The compiler uses it to automatically synthesize `==`, `!=`, `<`, `>`, `<=`, `>=`, greatly reducing boilerplate code.

## Default Three-Way Comparison

```cpp
struct Point {
    int x, y, z;
    auto operator<=>(const Point&) const = default;
};

Point a{1, 2, 3}, b{1, 2, 4};
bool eq = (a == b);  // false
bool lt = (a < b);   // true (field-by-field lexicographic)
```

`= default` requires all members to be comparable; the compiler generates comparison logic field-by-field in declaration order.

## Comparison Categories

| Type | Semantics | Typical Scenarios |
|------|-----------|-------------------|
| `std::strong_ordering` | No equivalent substitutable objects | Integers, enums, pointers |
| `std::weak_ordering` | Equivalent but distinguishable | Lexicographic order, case-insensitive |
| `std::partial_ordering` | Some values incomparable | Floating-point (NaN) |

```cpp
#include <compare>

struct Version {
    int major, minor, patch;
    auto operator<=>(const Version&) const = default;
    // All three fields are int → strong_ordering → == is auto-synthesized
};

struct SensorReading {
    double value;
    std::partial_ordering operator<=>(const SensorReading& rhs) const {
        return value <=> rhs.value;  // NaN produces unordered
    }
    bool operator==(const SensorReading& rhs) const {
        return value == rhs.value;
    }
};
```

## Custom Comparison

```cpp
#include <compare>

struct Circle {
    double radius, x, y;
    // Sort by radius only
    std::weak_ordering operator<=>(const Circle& rhs) const {
        if (auto c = radius <=> rhs.radius; c != 0) return c;
        return std::weak_ordering::equivalent;
    }
    bool operator==(const Circle& rhs) const {
        return radius == rhs.radius;
    }
};
```

## `==` Synthesis Rules

- `operator<=>` returns `strong_ordering` → compiler automatically synthesizes `operator==`.
- Returns `weak_ordering` or `partial_ordering` → **does not** automatically synthesize `==`; must be provided manually.
- Reverse operators (e.g., `b < a`) are deduced by the compiler from `a <=> b`.

## Integration with the Standard Library

```cpp
#include <algorithm>
#include <vector>

struct Record {
    int id;
    std::string name;
    auto operator<=>(const Record&) const = default;
};

void sort_records(std::vector<Record>& v) {
    std::sort(v.begin(), v.end());  // <=> auto-generates the needed comparisons
}
```

## Mixed-Type Comparison

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

Mixed-type comparisons must be implemented manually; `<=>` does not automatically deduce across types.

## Common Pitfalls

```cpp
// Pitfall 1: Floating-point default comparison includes NaN
struct Bad { float val; auto operator<=>(const Bad&) const = default; };
// When val is NaN, <=> returns partial_ordering::unordered

// Pitfall 2: Pointer members
struct WithPtr { int* p; auto operator<=>(const WithPtr&) const = default; };
// Pointer <=> requires pointing to the same array, otherwise UB

// Pitfall 3: = default does not compare base classes (unless base also provides <=>)
struct Base { int id; auto operator<=>(const Base&) const = default; };
struct Derived : Base {
    std::string name;
    auto operator<=>(const Derived& rhs) const {
        if (auto c = Base::operator<=>(rhs); c != 0) return c;
        return name <=> rhs.name;
    }
};
```

## Comparison with C++17 `std::tie`

```cpp
// C++17: hand-write six operators or use tie
struct Old {
    int a, b;
    bool operator<(const Old& r) const { return std::tie(a,b) < std::tie(r.a,r.b); }
    bool operator==(const Old& r) const { return std::tie(a,b) == std::tie(r.a,r.b); }
    // Still need !=, >, <=, >= …
};

// C++20: one line
struct New { int a, b; auto operator<=>(const New&) const = default; };
```

## Summary

- Default `= default` performs field-by-field lexicographic comparison in declaration order.
- The return type determines which operators are available; `strong_ordering` auto-synthesizes `==`.
- Floating-point and pointer members have semantic pitfalls to watch for.
- Mixed-type comparison requires manually implementing `operator<=>(const OtherType&)`.
