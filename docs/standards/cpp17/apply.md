---
title: C++17 std::apply
topic: unknown
feature: std-apply
standard: N/A
status_checked_at: 2026-06-20
---
# C++17 std::apply

## 概述

`std::apply` 是 C++17 在 `<tuple>` 中引入的自由函数，用于**将一个可调用对象应用到元组中的元素上**。它将元组展开为函数参数列表，避免了手动编写 `std::get<0>(t), std::get<1>(t), ...` 的样板代码。其内部基于 `std::index_sequence` 实现编译期索引展开。

## 函数签名

```cpp
#include <tuple>
#include <utility>

template <class F, class Tuple>
constexpr decltype(auto) apply(F&& f, Tuple&& t);
```

- `f`：任意可调用对象（函数指针、lambda、`std::bind` 表达式、成员函数指针等）
- `t`：一个 tuple-like 对象（`std::tuple`、`std::pair`、`std::array`）

## 工作原理

`std::apply` 的核心是利用 `std::index_sequence` 在编译期生成索引，将 tuple 展开：

```cpp
// 简化的实现原理
template <class F, class Tuple, size_t... I>
decltype(auto) apply_impl(F&& f, Tuple&& t, std::index_sequence<I...>) {
    return std::forward<F>(f)(std::get<I>(std::forward<Tuple>(t))...);
}

template <class F, class Tuple>
decltype(auto) apply(F&& f, Tuple&& t) {
    constexpr auto size = std::tuple_size_v<std::remove_cvref_t<Tuple>>;
    return apply_impl(std::forward<F>(f), std::forward<Tuple>(t),
                      std::make_index_sequence<size>{});
}
```

`std::index_sequence<I...>` 在编译期生成 `0, 1, 2, ...` 序列，配合包展开 `std::get<I>(t)...` 将每个元素作为独立参数传递。

## 基本用法

```cpp
#include <tuple>
#include <iostream>
#include <string>

int add(int a, int b) {
    return a + b;
}

void greet(std::string name, int age) {
    std::cout << "Hello " << name << ", age " << age << "\n";
}

int main() {
    auto t1 = std::make_tuple(3, 4);
    int result = std::apply(add, t1);
    std::cout << "add result: " << result << "\n";  // 7

    auto t2 = std::make_tuple("Alice", 30);
    std::apply(greet, t2);  // Hello Alice, age 30
}
```

## 配合 lambda 使用

```cpp
#include <tuple>
#include <iostream>

int main() {
    auto t = std::make_tuple(1, 2.0, "three");

    // lambda 接收展开后的参数
    std::apply([](auto&&... args) {
        ((std::cout << args << " "), ...);
        std::cout << "\n";
    }, t);  // 1 2 three
}
```

## 配合 pair 和 array

```cpp
#include <tuple>
#include <utility>
#include <array>

int multiply(int a, int b) { return a * b; }

int main() {
    // std::pair 也可以
    std::pair<int, int> p{5, 6};
    int r1 = std::apply(multiply, p);  // 30

    // std::array 也可以
    std::array<int, 2> arr{7, 8};
    int r2 = std::apply(multiply, arr);  // 56
}
```

## 实际应用场景

### 替代手动 emplace

```cpp
#include <tuple>
#include <vector>
#include <string>

struct Task {
    int id;
    std::string name;
    double priority;
};

int main() {
    std::vector<Task> tasks;

    // 使用 std::apply 配合 emplace
    auto args = std::make_tuple(1, "build", 0.5);
    std::apply([&tasks](auto&&... a) {
        tasks.emplace_back(std::forward<decltype(a)>(a)...);
    }, args);
}
```

### 在容器上批量调用

```cpp
#include <tuple>
#include <vector>
#include <iostream>

void process(int x, int y, int z) {
    std::cout << x << "," << y << "," << z << "\n";
}

int main() {
    std::vector<std::tuple<int, int, int>> data = {
        {1, 2, 3},
        {4, 5, 6},
        {7, 8, 9}
    };

    for (const auto& row : data) {
        std::apply(process, row);
    }
}
```

### 配合 std::make_from_tuple 构造对象

```cpp
#include <tuple>
#include <string>
#include <iostream>

struct Config {
    std::string host;
    int port;
    bool verbose;

    Config(std::string h, int p, bool v)
        : host(std::move(h)), port(p), verbose(v) {}
};

int main() {
    auto args = std::make_tuple("localhost", 8080, true);
    Config cfg = std::make_from_tuple<Config>(std::move(args));
    // C++23: std::make_from_tuple 已在 C++17 中可用
}
```

## 与 std::invoke 的关系

`std::apply` 和 `std::invoke`（C++17）是互补的：

| 特性 | `std::apply` | `std::invoke` |
|------|-------------|---------------|
| 参数传递方式 | 从 tuple 展开 | 直接传递 |
| 成员函数指针 | 需配合 tuple | 原生支持 |
| 主要用途 | 批量/元组驱动调用 | 统一调用语法 |

```cpp
#include <tuple>
#include <functional>

struct Foo {
    int value;
    int get_value() const { return value; }
};

int main() {
    Foo foo{42};

    // std::invoke 调用成员函数
    int v = std::invoke(&Foo::get_value, foo);

    // std::apply 也可以，但更繁琐
    auto t = std::make_tuple(&Foo::get_value, &foo);
    // std::apply 不直接支持成员函数指针的 this 绑定
}
```

## 编译器支持

| 编译器 | 最低版本 | 备注 |
|--------|---------|------|
| GCC | 7.0 | 完整支持 |
| Clang | 5.0 | 完整支持 |
| MSVC | 19.11 (VS 2017 15.3) | 完整支持 |

`constexpr std::apply` 在 C++17 中可用（当 `f` 和 tuple 的元素均为 `constexpr` 时）。

## 最佳实践

- **简化 tuple 展开**：任何需要 `std::get<I>(t)...` 的地方都应考虑使用 `std::apply`。
- **配合 emplace**：向容器插入复杂对象时，用 `std::apply` 配合 `emplace_back` 可减少拷贝。
- **注意引用折叠**：`std::apply` 会转发 tuple 元素，确保 lambda 参数使用完美转发（`auto&&`）。
- **不要过度使用**：简单场景下直接调用函数更清晰，`std::apply` 适合元组驱动的批量模式。

## 常见陷阱

```cpp
// 陷阱 1：tuple 元素的生命周期
auto make_args() {
    return std::make_tuple(1, std::string("hello"));
    // string 是临时对象，apply 调用前必须保证生命周期
}

// 陷阱 2：引用参数需要 ref wrapper
int x = 10;
auto t = std::make_tuple(std::ref(x));
std::apply([](int& v) { v = 20; }, t);
// x 现在是 20

// 陷阱 3：空 tuple
std::apply([]() { std::cout << "no args\n"; }, std::make_tuple());
// OK：零参数调用
```
