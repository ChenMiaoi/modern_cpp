---
title: "`requires` 表达式"
topic: unknown
feature: requires-expression
standard: N/A
status_checked_at: 2026-06-02
---
# `requires` 表达式

## 概述

C++20 的 `requires` 表达式是约束系统的核心原语，在编译期检测类型是否满足特定语法要求。返回 `bool` 常量，可直接用作概念定义体或约束条件。

## 基本语法

```cpp
template <typename T>
concept Incrementable = requires(T t) {
    { ++t } -> std::same_as<T&>;
};

static_assert(Incrementable<int>);
static_assert(!Incrementable<std::string>);
```

## 四种需求

### 1. 简单需求（Simple）

验证表达式**语法合法**，不求值。

```cpp
template <typename T>
concept HasSize = requires(T t) {
    t.size();
    t.begin();
};
```

### 2. 类型需求（Type）

验证类型表达式合法。

```cpp
template <typename T>
concept HasValueType = requires {
    typename T::value_type;
    typename T::iterator;
};

static_assert(HasValueType<std::vector<int>>);
static_assert(!HasValueType<int>);
```

### 3. 复合需求（Compound）

验证表达式合法**且**结果满足类型/`noexcept` 约束。

```cpp
template <typename T>
concept Hashable = requires(T t) {
    { std::hash<T>{}(t) } noexcept -> std::convertible_to<std::size_t>;
};

template <typename T>
concept Printable = requires(std::ostream& os, T t) {
    { os << t } -> std::convertible_to<std::ostream&>;
};
```

### 4. 嵌套需求（Nested）

在 `requires` 表达式中直接施加概念约束。

```cpp
template <typename T>
concept SortableRange = requires(T& t) {
    requires std::ranges::random_access_range<T>;
    requires std::totally_ordered<std::ranges::range_value_t<T>>;
    { std::sort(t.begin(), t.end()) };
};
```

## `requires` 子句 vs `requires` 表达式

```cpp
// 子句：模板参数后施加约束
template <typename T>
    requires std::integral<T>
T add(T a, T b) { return a + b; }

// 表达式：产生 bool 值
constexpr bool v = requires { requires std::integral<int>; };

// 混合：子句内嵌表达式
template <typename T>
    requires requires(T t) { t.serialize(); }
void process(T& obj) { obj.serialize(); }
```

| | `requires` 子句 | `requires` 表达式 |
|---|---|---|
| 位置 | 模板参数、函数签名后 | 任何需要 `bool` 的位置 |
| 结果 | 不产生值，仅约束 | 产生 `bool` 常量 |
| 语法 | `requires Concept<Args>` | `requires { ... }` |

## 约束子和（Subsumption）

```cpp
template <typename T>
concept C1 = requires(T t) { t.foo(); };

template <typename T>
concept C2 = C1<T> && requires(T t) { t.bar(); };
// C2 subsumes C1

void f(auto&& x) requires C1<decltype(x)> { }
void f(auto&& x) requires C2<decltype(x)> { }

struct S { void foo(); void bar(); };
S s;
f(s);  // 选择 C2 版本：更特化
```

裸 `requires` 表达式**不参与**子和：

```cpp
void g(auto&& x) requires requires { x.foo(); } { }
void g(auto&& x) requires requires { x.foo(); x.bar(); } { }
// 二义性！直接 requires 表达式无子和关系
```

## 常见模式

```cpp
// 检查成员函数签名
template <typename T>
concept Serializable = requires(const T& t, std::ostream& os) {
    { t.serialize(os) } -> std::same_as<void>;
};

// 检查构造函数
template <typename T>
concept DefaultAndCopy = requires {
    T{};
    T{std::declval<T>()};
};

// 检查运算符
template <typename T>
concept Addable = requires(T a, T b) {
    { a + b } -> std::convertible_to<T>;
    { a += b } -> std::same_as<T&>;
};
```

## 嵌套 `requires` 与短路求值

```cpp
template <typename T>
concept ComplexConcept = requires(T t) {
    typename T::value_type;                        // 1. 类型存在
    { t.size() } -> std::convertible_to<std::size_t>; // 2. 返回值
    { t.empty() } -> std::convertible_to<bool>;   // 3. 返回值
    requires std::copyable<T>;                     // 4. 概念约束
};
// 从上到下短路：前面失败则后面不检查
```

## 总结

- **简单需求**检查表达式合法性，**类型需求**检查类型存在性，**复合需求**附加结果类型和 `noexcept`，**嵌套需求**直接施加概念。
- `requires` 子句用于约束模板，`requires` 表达式产生 `bool`。
- 约束子和仅在概念之间生效，裸 `requires` 表达式不参与。
- 优先用概念封装 `requires` 表达式，以支持子和参与重载决议。
