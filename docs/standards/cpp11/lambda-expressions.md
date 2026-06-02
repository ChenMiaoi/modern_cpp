---
title: "Lambda 表达式"
topic: unknown
feature: lambda-expressions
standard: N/A
status_checked_at: 2026-06-02
---
# Lambda 表达式

## 概述

Lambda 表达式是匿名函数对象，使闭包（closure）成为语言的一等公民。

## 语法

```cpp
[capture](parameters) mutable exception attribute -> return_type { body }
```

各部分均可省略（最简形式：`[]() {}`）。

## 捕获列表

| 捕获方式 | 语法 | 语义 |
|----------|------|------|
| 值捕获 | `[x]` | 拷贝一份 |
| 引用捕获 | `[&x]` | 引用外部变量 |
| 隐式值捕获 | `[=]` | 所有用到的变量按值捕获 |
| 隐式引用捕获 | `[&]` | 所有用到的变量按引用捕获 |
| 混合 | `[=, &x]` | 默认按值，x 按引用 |
| 混合 | `[&, x]` | 默认按引用，x 按值 |
| this 指针 | `[this]` | 捕获成员变量（C++17 可用 `[*this]` 按值捕获） |
| 初始化捕获 | `[x = expr]` | 移动或任意表达式初始化（C++14） |

## 基本示例

```cpp
// 最简单的 lambda
auto greet = []() { std::cout << "Hello\n"; };
greet();  // 输出: Hello

// 带参数
auto add = [](int a, int b) { return a + b; };
std::cout << add(3, 4);  // 输出: 7

// 捕获外部变量
int factor = 10;
auto multiply = [factor](int x) { return x * factor; };
std::cout << multiply(5);  // 输出: 50
```

## 与 STL 算法配合

Lambda 最常见的用途是作为 STL 算法的谓词：

```cpp
std::vector<int> vec = {3, 1, 4, 1, 5, 9, 2, 6};

// 排序
std::sort(vec.begin(), vec.end(), [](int a, int b) { return a > b; });

// 查找
auto it = std::find_if(vec.begin(), vec.end(), [](int x) { return x > 5; });

// 计数
auto count = std::count_if(vec.begin(), vec.end(), [](int x) { return x % 2 == 0; });

// 累加
int sum = std::accumulate(vec.begin(), vec.end(), 0,
    [](int acc, int x) { return acc + x * x; });

// 移除-擦除
vec.erase(std::remove_if(vec.begin(), vec.end(),
    [](int x) { return x < 3; }), vec.end());
```

## 函数返回值

Lambda 的返回类型通常自动推导。需要显式指定时使用尾置返回类型：

```cpp
auto divide = [](double a, double b) -> double {
    if (b == 0.0) return 0.0;
    return a / b;
};
```

## 存储 Lambda

Lambda 的类型是匿名的，通常使用 `auto` 或 `std::function` 来存储：

```cpp
// auto（推荐，零开销）
auto fn = [](int x) { return x * 2; };

// std::function（类型擦除，有开销）
std::function<int(int)> fn2 = [](int x) { return x * 2; };
```

## mutable Lambda

值捕获的变量默认是 `const` 的。`mutable` 允许修改：

```cpp
int counter = 0;
auto increment = [counter]() mutable { return ++counter; };
// counter 不受影响，lambda 内部的副本在变
std::cout << increment();  // 1
std::cout << increment();  // 2
std::cout << counter;      // 0
```

## 递归 Lambda

C++11 中实现递归 Lambda 需要 `std::function`：

```cpp
std::function<int(int)> factorial = [&factorial](int n) {
    return n <= 1 ? 1 : n * factorial(n - 1);
};
```

C++14 中可以使用初始化捕获更优雅地实现。

## 生命周期陷阱

```cpp
// 危险！引用捕获的变量超出作用域
std::function<int()> make_counter() {
    int count = 0;
    return [&count]() { return ++count; };  // 悬空引用！
}

// 正确：值捕获
std::function<int()> make_counter() {
    int count = 0;
    return [count]() mutable { return ++count; };
}
```

## 最佳实践

- 优先使用 `[=]` 或 `[&]` 进行简短的捕获，再根据需要调整
- 优先值捕获（避免生命周期问题），仅在必要时引用捕获
- 短 lambda 放在一行，长 lambda 提取为命名函数
- 用 `auto` 存储 lambda，只在需要类型擦除时用 `std::function`
