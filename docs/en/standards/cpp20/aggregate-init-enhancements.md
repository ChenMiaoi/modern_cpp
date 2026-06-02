---
title: "Aggregate Initialization Enhancements"
topic: unknown
feature: aggregate-init-enhancements
standard: N/A
status_checked_at: 2026-06-02
---
# Aggregate Initialization Enhancements

## Overview

C++20 makes three improvements to aggregate initialization:
1. Tightening the aggregate definition — classes with user-**declared** constructors are no longer aggregates.
2. Parenthesized aggregate initialization `T(args...)` is now supported.
3. **Designated initializers** `.field = value` are introduced.

## C++20 Aggregate Definition Changes

```cpp
// C++17: a class with user-declared constructors was still an aggregate
struct Legacy {
    int x;
    Legacy() = default;
    Legacy(int v) : x(v) {}
};
// C++17: Legacy{1} was valid aggregate initialization
// C++20: Legacy is no longer an aggregate — it has user-declared constructors

// C++20 aggregate conditions:
// - No user-declared constructors (including = default and = delete)
// - No private/protected non-static data members
// - No virtual functions
// - No virtual base classes
struct Aggregate {
    int x;
    double y;
    std::string z;
};
```

## Parenthesized Aggregate Initialization

```cpp
struct Pair { int first; int second; };

Pair p1{1, 2};     // valid since C++17
Pair p2(1, 2);     // valid since C++20

Pair p3{};          // {0, 0}
// Pair p4();       // function declaration (most vexing parse)
Pair p5 = Pair();   // value-initializes {0, 0}
```

### Comparison with `explicit` Constructors

```cpp
struct Wrapper {
    int value;
    explicit Wrapper(int v) : value(v) {}
};

Wrapper w1(42);       // OK: direct initialization
// Wrapper w2 = 42;   // error: explicit prevents implicit conversion

struct AggWrap { int value; };
AggWrap a1{42};       // OK: aggregate initialization
AggWrap a2(42);       // C++20 OK: parenthesized aggregate initialization
// Note: the parenthesized version does not perform narrowing checks
```

## Designated Initializers

```cpp
struct Config {
    int width;
    int height;
    bool fullscreen;
    std::string title;
};

Config c{
    .width = 1920,
    .height = 1080,
    .fullscreen = true,
    .title = "Hello"
};
```

### Syntax Rules

```cpp
struct Point { int x, y, z; };

Point p1{.x = 1, .y = 2, .z = 3};    // OK
// Point p2{.z = 3, .x = 1};          // error: must be in declaration order

Point p3{.x = 1};                      // {1, 0, 0}: trailing members omitted
// Point p4{.x = 1, 2};               // error: cannot mix designated and non-designated
// Point p5{.x = 1, .x = 2};          // error: cannot designate the same member twice
```

### Nested Aggregates

```cpp
struct Inner { int a, b; };
struct Outer { int x; Inner inner; int y; };

Outer o{
    .x = 1,
    .inner = {.a = 10, .b = 20},
    .y = 3
};

// C++20 does not support deep designation (C99 does):
// Outer o2{.x = 1, .inner.a = 10};  // error
```

## Differences from C99 Designated Initializers

| Feature | C99 | C++20 |
|---------|-----|-------|
| `.field = value` | Supported | Supported |
| Order requirement | None | **Must be in declaration order** |
| Arrays `[i] = value` | Supported | **Not supported** |
| Deep designation `.a.b = v` | Supported | **Not supported** |

## Narrowing Checks

```cpp
struct S { char c; int i; };

// Braces: narrowing checks apply
// S s1{300, 1};    // warning or error: 300 narrows to char

// Parentheses: no narrowing checks
S s2(300, 1);       // OK (char value is implementation-defined)

// Designated initializers: narrowing checks apply
S s3{.c = 'A', .i = 1};  // OK
```

## Common Pitfalls

```cpp
// Pitfall 1: declaration order violation
struct A { int x, y; };
// A a{.y = 1, .x = 2};  // error

// Pitfall 2: implicit aggregate change
struct MaybeAgg {
    MaybeAgg() = default;   // C++17 is aggregate, C++20 is not
    int x;
};
// MaybeAgg m{42};          // C++20 error

// Pitfall 3: base class members
struct Base { int a; };
struct Derived : Base { int b; };
Derived d{.a = 1, .b = 2};  // C++20 OK (base class subobject can be initialized)
```

## Summary

- C++20 tightens the aggregate definition: classes with user-declared constructors are no longer aggregates.
- Parenthesized aggregate initialization `T(args...)` is equivalent to braces but **lacks narrowing checks**.
- Designated initializers `.field = value` must follow declaration order; array indexing and deep designation are not supported.
- Nested aggregates require writing `.inner = {.a = v}` level by level.
