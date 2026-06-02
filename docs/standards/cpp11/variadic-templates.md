---
title: "可变参数模板"
topic: unknown
feature: variadic-templates
standard: N/A
status_checked_at: 2026-06-02
---
# 可变参数模板

## 概述

可变参数模板（variadic template）是 C++11 引入的核心特性，允许模板接受**任意数量、任意类型**的参数。它是实现类型安全的可变参数函数、完美转发、以及 `std::tuple` 等标准库组件的基石。

C++11 之前，处理可变数量参数只能依赖 C 风格 `va_list`（无类型安全、不支持非 POD 类型）或重载多个固定参数版本。可变参数模板彻底解决了这个问题。

## 基本语法

```cpp
// Args 是模板参数包，args 是函数参数包
template<typename... Args>
void f(Args... args) {
    static_assert(sizeof...(Args) == sizeof...(args), "counts must match");
}
```

省略号 `...` 的两种含义：在类型名之后**声明包**，在包名之后**展开包**。`sizeof...` 返回包中元素个数。

## 递归展开

C++11 展开参数包的标准手法是**递归**——提供终止重载，逐步剥离参数：

```cpp
void print() { std::cout << '\n'; }  // base case

template<typename T, typename... Args>
void print(const T& first, const Args&... rest) {
    std::cout << first;
    if (sizeof...(rest) > 0) std::cout << ", ";
    print(rest...);  // strip first, recurse
}

print(1, "hello", 3.14, 'c');  // "1, hello, 3.14, c"
```

## 初始化列表展开（折叠表达式的 C++11 替代）

C++17 引入折叠表达式，C++11 中用初始化列表 + 逗号运算符模拟：

```cpp
template<typename... Args>
void printAll(const Args&... args) {
    int dummy[] = { (std::cout << args << ' ', 0)... };
    (void)dummy;
}
```

## 完美转发

可变参数模板与 `std::forward` 结合，保持参数的左值/右值属性：

```cpp
template<typename T, typename... Args>
std::unique_ptr<T> my_make_unique(Args&&... args) {
    // Args&& 是转发引用；std::forward 保持原始值类别
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

auto w = my_make_unique<Widget>(42, std::string("hello"));
```

当传递左值时 `Args` 推导为引用类型（`T&`），右值时推导为值类型（`T`）。`std::forward<Args>` 据此决定移动还是拷贝。

## `index_sequence` 技巧

```cpp
// C++11: 手动定义（C++14 提供 std::index_sequence）
template<std::size_t... Is> struct index_sequence {};

template<std::size_t N, std::size_t... Is>
struct make_index_sequence : make_index_sequence<N - 1, N - 1, Is...> {};

template<std::size_t... Is>
struct make_index_sequence<0, Is...> : index_sequence<Is...> {};
```

用途——按索引遍历 tuple：

```cpp
template<typename Tuple, std::size_t... Is>
void printTupleImpl(const Tuple& t, index_sequence<Is...>) {
    print(std::get<Is>(t)...);
}

template<typename... Args>
void printTuple(const std::tuple<Args...>& t) {
    printTupleImpl(t, make_index_sequence<sizeof...(Args)>{});
}
```

## tuple 简化实现

```cpp
template<typename... Types> struct Tuple;

template<typename Head, typename... Tail>
struct Tuple<Head, Tail...> : Tuple<Tail...> {
    Head value;
    Tuple(const Head& h, const Tail&... tail)
        : Tuple<Tail...>(tail...), value(h) {}
    Head& head() { return value; }
    Tuple<Tail...>& tail() { return *this; }
};

template<> struct Tuple<> {};
```

标准库实现远比这复杂（空基类优化、构造/赋值/比较），但核心思想一致：递归继承。

## 实现 `make_unique`

`std::make_unique` 是 C++14 引入的，C++11 中需自行实现——这正是可变参数模板的典型应用：

```cpp
template<typename T, typename... Args>
std::unique_ptr<T> my_make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
```

## 最佳实践

| 实践 | 说明 |
|------|------|
| `sizeof...(Args)` 优于 `sizeof...(args)` | 表示类型个数更直观 |
| 需要完美转发时用 `Args&&...` | 否则用 `const Args&...` 即可 |
| 终止函数声明在前 | 确保编译器能看到 |
| `index_sequence` 升级 C++14 | 不再需要手动定义 |

## 常见陷阱

**递归终止缺失**——没有终止版本导致编译失败：

```cpp
// 错误：f() 无定义
template<typename T, typename... Args>
void f(T first, Args... rest) { f(rest...); }

// 正确：终止版本
void f() {}
```

**`Args&&` 不总是右值引用**——当传递左值时推导为左值引用，这是转发引用而非右值引用，必须配合 `std::forward` 使用。

**展开位置错误**——`...` 只能跟在包名之后：

```cpp
f(args..., 42);   // OK: 展开为 f(a1, a2, ..., 42)
// f(args, 42)...;  // 错误
```

## 与 C++11 之前的对比

| 特性 | `va_list` | 可变参数模板 |
|------|-----------|-------------|
| 类型安全 | 否 | 是 |
| 支持非 POD | 否 | 是 |
| 支持引用 | 否 | 是 |
| 编译期检查 | 否 | 是 |
| 可内联 | 否 | 是 |
