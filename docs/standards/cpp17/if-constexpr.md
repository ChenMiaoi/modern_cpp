---
title: "C++17 if constexpr"
topic: unknown
feature: if-constexpr
standard: N/A
status_checked_at: 2026-06-02
---
# C++17 if constexpr

## 概述

`if constexpr` 是 C++17 引入的编译期条件分支语句，允许在模板函数中根据编译期常量表达式选择性地编译代码分支。被丢弃的分支不参与模板实例化，不要求该分支对当前模板参数类型合法。这一特性取代了 SFINAE、标签分发和模板特化等复杂模式。

## 基本语法

```cpp
if constexpr (编译期常量表达式) {
    // 条件为 true 时编译
} else {
    // 条件为 false 时编译（可选）
}
```

## 基本用法

```cpp
#include <iostream>
#include <type_traits>

template<typename T>
void describe(T value) {
    if constexpr (std::is_integral_v<T>) {
        std::cout << "integral: " << value << "\n";
    } else if constexpr (std::is_floating_point_v<T>) {
        std::cout << "float: " << value << "\n";
    } else {
        std::cout << "other type\n";
    }
}

describe(42);       // integral: 42
describe(3.14);     // float: 3.14
describe("hello");  // other type
```

## 与普通 if 的区别

```cpp
template<typename T> void foo(T x) {
    if (std::is_integral_v<T>) {
        // 普通 if：两个分支都必须在编译期合法
    } else {
        // x.nonexistent_method();  // 编译错误——即使运行时不走
    }
}

template<typename T> void bar(T x) {
    if constexpr (std::is_integral_v<T>) {
        // ...
    } else {
        // x.nonexistent_method();  // 丢弃分支——不实例化，不报错
    }
}
```

## 废弃分支规则

1. **不进行实例化**——不要求对当前类型合法。
2. **必须可解析**——语法结构必须合法。
3. **非依赖名称仍查找**——不依赖模板参数的名称仍然绑定。
4. **返回类型推导**——各分支可返回不同类型。

```cpp
template<typename T>
auto convert(T val) {
    if constexpr (std::is_same_v<T, std::string>)
        return val.length();         // size_t
    else if constexpr (std::is_arithmetic_v<T>)
        return std::to_string(val);  // std::string
}
```

## 取代 SFINAE

```cpp
// C++14：两个重载 + enable_if
template<typename T>
std::enable_if_t<std::is_integral_v<T>, T>
safe_add(T a, T b) {
    if (b > 0 && a > std::numeric_limits<T>::max() - b)
        throw std::overflow_error("overflow");
    return a + b;
}
template<typename T>
std::enable_if_t<!std::is_integral_v<T>, T>
safe_add(T a, T b) { return a + b; }

// C++17：单个函数体
template<typename T>
T safe_add(T a, T b) {
    if constexpr (std::is_integral_v<T>) {
        if (b > 0 && a > std::numeric_limits<T>::max() - b)
            throw std::overflow_error("overflow");
    }
    return a + b;
}
```

## 取代标签分发

```cpp
// C++14：多个重载 + 标签类型
template<typename Iter>
void advance_impl(Iter& it, int n, std::random_access_iterator_tag) { it += n; }
template<typename Iter>
void advance_impl(Iter& it, int n, std::input_iterator_tag) {
    for (int i = 0; i < n; ++i) ++it;
}
template<typename Iter>
void my_advance(Iter& it, int n) {
    advance_impl(it, n, typename std::iterator_traits<Iter>::iterator_category{});
}

// C++17：单个函数
template<typename Iter>
void my_advance(Iter& it, int n) {
    using Cat = typename std::iterator_traits<Iter>::iterator_category;
    if constexpr (std::is_base_of_v<std::random_access_iterator_tag, Cat>)
        it += n;
    else
        for (int i = 0; i < n; ++i) ++it;
}
```

## 递归展开替代

```cpp
#include <iostream>
#include <tuple>

template<typename Tuple, std::size_t N = 0>
void print_tuple(const Tuple& t) {
    if constexpr (N < std::tuple_size_v<Tuple>) {
        if constexpr (N > 0) std::cout << ", ";
        std::cout << std::get<N>(t);
        print_tuple<Tuple, N + 1>(t);
    }
    // N 达到大小时条件为 false，递归自然终止，无需基类特化
}
```

## 与模板特化的关系

适合 `if constexpr` 的：同一函数体内的类型分支、替代 SFINAE、替代 tag dispatch。

仍需模板特化的：完全不同的类定义/成员变量、变量模板特化。

## 最佳实践

- **替代 `enable_if`**——可读性好一个量级。
- **替代标签分发**——无需额外重载和标签类型。
- **递归模板展开**——替代基类特化终止递归。
- **仅用于编译期条件**——运行时条件用普通 `if`。

## 常见陷阱

```cpp
// 陷阱 1：条件不是编译期常量
int x = 42;
// if constexpr (x > 0) {}  // 编译错误

// 陷阱 2：废弃分支的语法错误仍被检测
// x ==== 42;  // 语法错误——即使在废弃分支中也报

// 陷阱 3：非依赖名称在废弃分支中仍查找
template<typename T> void tricky() {
    if constexpr (std::is_same_v<T, int>) {
        // some_undefined_name();  // 非依赖名仍报错
    }
}

// 陷阱 4：不能检测成员是否存在
// 检测成员需 decltype + void_t 或 C++20 concepts
```
