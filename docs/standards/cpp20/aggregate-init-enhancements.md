# 聚合初始化增强

## 概述

C++20 对聚合初始化做出三项改进：
1. 收紧聚合定义——禁止有用户**声明**构造函数的类作为聚合。
2. 支持圆括号聚合初始化 `T(args...)`。
3. 引入**指定初始化器** `.field = value`。

## C++20 聚合定义变化

```cpp
// C++17：有用户声明构造函数仍算聚合
struct Legacy {
    int x;
    Legacy() = default;
    Legacy(int v) : x(v) {}
};
// C++17: Legacy{1} 合法聚合初始化
// C++20: Legacy 不再是聚合——拥有用户声明的构造函数

// C++20 聚合条件：
// - 无用户声明的构造函数（含 = default 和 = delete）
// - 无 private/protected 非静态数据成员
// - 无虚函数
// - 无虚基类
struct Aggregate {
    int x;
    double y;
    std::string z;
};
```

## 圆括号聚合初始化

```cpp
struct Pair { int first; int second; };

Pair p1{1, 2};     // C++17 起合法
Pair p2(1, 2);     // C++20 起合法

Pair p3{};          // {0, 0}
// Pair p4();       // 函数声明（most vexing parse）
Pair p5 = Pair();   // 值初始化 {0, 0}
```

### 与 `explicit` 构造函数对比

```cpp
struct Wrapper {
    int value;
    explicit Wrapper(int v) : value(v) {}
};

Wrapper w1(42);       // OK：直接初始化
// Wrapper w2 = 42;   // 错误：explicit 阻止隐式转换

struct AggWrap { int value; };
AggWrap a1{42};       // OK：聚合初始化
AggWrap a2(42);       // C++20 OK：圆括号聚合初始化
// 注意：圆括号版本不支持窄化检查
```

## 指定初始化器

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

### 语法规则

```cpp
struct Point { int x, y, z; };

Point p1{.x = 1, .y = 2, .z = 3};    // OK
// Point p2{.z = 3, .x = 1};          // 错误：必须按声明顺序

Point p3{.x = 1};                      // {1, 0, 0}：省略末尾成员
// Point p4{.x = 1, 2};               // 错误：不能混合指定和非指定
// Point p5{.x = 1, .x = 2};          // 错误：不能重复指定
```

### 嵌套聚合

```cpp
struct Inner { int a, b; };
struct Outer { int x; Inner inner; int y; };

Outer o{
    .x = 1,
    .inner = {.a = 10, .b = 20},
    .y = 3
};

// C++20 不支持深层指定（C99 支持）：
// Outer o2{.x = 1, .inner.a = 10};  // 错误
```

## 与 C99 指定初始化器的差异

| 特性 | C99 | C++20 |
|------|-----|-------|
| `.field = value` | 支持 | 支持 |
| 顺序要求 | 无 | **必须按声明顺序** |
| 数组 `[i] = value` | 支持 | **不支持** |
| 深层指定 `.a.b = v` | 支持 | **不支持** |

## 窄化检查

```cpp
struct S { char c; int i; };

// 花括号：有窄化检查
// S s1{300, 1};    // 警告或错误：300 窄化到 char

// 圆括号：无窄化检查
S s2(300, 1);       // OK（char 值实现定义）

// 指定初始化器：有窄化检查
S s3{.c = 'A', .i = 1};  // OK
```

## 常见陷阱

```cpp
// 陷阱 1：声明顺序违反
struct A { int x, y; };
// A a{.y = 1, .x = 2};  // 错误

// 陷阱 2：隐式聚合的变更
struct MaybeAgg {
    MaybeAgg() = default;   // C++17 是聚合，C++20 不再是
    int x;
};
// MaybeAgg m{42};          // C++20 错误

// 陷阱 3：基类成员
struct Base { int a; };
struct Derived : Base { int b; };
Derived d{.a = 1, .b = 2};  // C++20 OK（基类子对象可初始化）
```

## 总结

- C++20 收紧聚合定义：有用户声明构造函数的类不再是聚合。
- 圆括号聚合初始化 `T(args...)` 等效花括号，但**缺少窄化检查**。
- 指定初始化器 `.field = value` 必须按声明顺序，不支持数组索引或深层指定。
- 嵌套聚合需逐层写 `.inner = {.a = v}`。
