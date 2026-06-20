---
title: C++17 std::invoke
topic: unknown
feature: std-invoke
standard: N/A
status_checked_at: 2026-06-20
---
# C++17 std::invoke

## 概述

`std::invoke` 是 C++17 在 `<functional>` 中引入的自由函数，提供**统一的调用语法**。它可以调用普通函数、lambda、函数对象、成员函数指针和成员变量指针，无需针对不同类型编写不同的调用代码。它是 `std::apply`、`std::thread`、`std::invoke_result` 等设施的基础。

## 函数签名

```cpp
#include <functional>

template <class F, class... Args>
constexpr std::invoke_result_t<F, Args...> invoke(F&& f, Args&&... args);
```

- `f`：可调用对象（函数、lambda、成员函数指针、成员变量指针、bind 表达式等）
- `args...`：调用参数

## 基本用法

```cpp
#include <functional>
#include <iostream>

int free_function(int a, int b) {
    return a + b;
}

int main() {
    // 普通函数
    int r1 = std::invoke(free_function, 3, 4);  // 7

    // lambda
    auto add = [](int a, int b) { return a + b; };
    int r2 = std::invoke(add, 5, 6);  // 11

    // 函数对象
    struct Multiplier {
        int operator()(int a, int b) const { return a * b; }
    };
    int r3 = std::invoke(Multiplier{}, 3, 4);  // 12

    std::cout << r1 << " " << r2 << " " << r3 << "\n";
}
```

## 调用成员函数

这是 `std::invoke` 最有价值的功能——统一调用成员函数的语法：

```cpp
#include <functional>
#include <iostream>
#include <string>

struct Widget {
    int value;
    std::string name;

    int get_value() const { return value; }
    void set_value(int v) { value = v; }
    std::string greet() const { return "Hello, " + name; }
};

int main() {
    Widget w{42, "test"};

    // 成员函数指针 — 第二个参数是 this
    int v = std::invoke(&Widget::get_value, w);  // 42
    std::cout << v << "\n";

    // 带参数的成员函数
    std::invoke(&Widget::set_value, w, 100);
    std::cout << w.value << "\n";  // 100

    // 通过指针调用
    Widget* ptr = &w;
    std::string g = std::invoke(&Widget::greet, ptr);  // "Hello, test"
    std::cout << g << "\n";

    // 通过引用调用
    Widget& ref = w;
    int v2 = std::invoke(&Widget::get_value, ref);  // 100
}
```

## 访问成员变量

```cpp
#include <functional>
#include <iostream>

struct Point {
    double x, y;
};

int main() {
    Point p{3.0, 4.0};

    // 成员变量指针 — 返回引用
    double x = std::invoke(&Point::x, p);  // 3.0
    std::cout << "x: " << x << "\n";

    // 可以修改
    std::invoke(&Point::y, p) = 5.0;
    std::cout << "y: " << p.y << "\n";  // 5.0

    // 通过指针访问
    Point* ptr = &p;
    double y = std::invoke(&Point::y, *ptr);  // 5.0
}
```

## 配合 std::bind

```cpp
#include <functional>
#include <iostream>

void print_three(int a, int b, int c) {
    std::cout << a << ", " << b << ", " << c << "\n";
}

int main() {
    auto bound = std::bind(print_three, 1, std::placeholders::_1, 3);

    // std::invoke 可以调用 bind 表达式
    std::invoke(bound, 2);  // 1, 2, 3
}
```

## 与 lambda 的配合

```cpp
#include <functional>
#include <iostream>
#include <vector>
#include <algorithm>

struct Logger {
    void log(const std::string& msg) const {
        std::cout << "[LOG] " << msg << "\n";
    }
};

int main() {
    Logger logger;

    // 存储成员函数指针和对象的引用
    auto log_fn = std::bind(&Logger::log, &logger, std::placeholders::_1);

    // 通过 std::invoke 调用
    std::invoke(log_fn, "application started");
    std::invoke(log_fn, "processing data");
}
```

## std::invoke_result 和返回类型

```cpp
#include <functional>
#include <type_traits>
#include <iostream>

int func(int a) { return a * 2; }

int main() {
    // 获取返回类型
    using result_t = std::invoke_result_t<decltype(func), int>;
    static_assert(std::is_same_v<result_t, int>);

    // 检查是否可调用
    static_assert(std::is_invocable_v<decltype(func), int>);
    static_assert(!std::is_invocable_v<decltype(func), std::string>);

    // 获取 invoke 的结果类型
    auto r = std::invoke(func, 5);
    std::cout << r << "\n";  // 10
}
```

## 在泛型代码中使用

```cpp
#include <functional>
#include <iostream>
#include <vector>

// 泛型 apply_all：对容器每个元素调用可调用对象
template <typename Container, typename Func>
void apply_all(Container& c, Func&& func) {
    for (auto& elem : c) {
        std::invoke(std::forward<Func>(func), elem);
    }
}

int main() {
    std::vector<int> nums = {1, 2, 3, 4, 5};

    // 传入 lambda
    apply_all(nums, [](int& x) { x *= 2; });

    // 传入成员函数（需要传指针或引用容器）
    struct Doubler {
        void operator()(int& x) const { x *= 2; }
    };
    apply_all(nums, Doubler{});

    for (int n : nums) {
        std::cout << n << " ";  // 2 4 6 8 10
    }
    std::cout << "\n";
}
```

## std::apply 与 std::invoke 的关系

| 特性 | `std::apply` | `std::invoke` |
|------|-------------|---------------|
| 参数来源 | 从 tuple 展开 | 直接传递 |
| 成员函数指针 | 需手动绑定 this | 原生支持 |
| 成员变量访问 | 不支持 | 支持 |
| 主要用途 | 元组驱动的调用 | 统一调用语法 |

```cpp
#include <functional>
#include <tuple>

struct Obj {
    int x;
    int get() const { return x; }
};

int main() {
    Obj obj{42};

    // std::invoke 直接调用成员函数
    int v1 = std::invoke(&Obj::get, obj);

    // std::apply 只能展开参数，不能处理成员函数指针的 this
    // 需要手动绑定：
    auto bound = std::bind(&Obj::get, &obj);
    int v2 = std::invoke(bound);
}
```

## 编译器支持

| 编译器 | 最低版本 | 备注 |
|--------|---------|------|
| GCC | 7.0 | 完整支持 |
| Clang | 5.0 | 完整支持 |
| MSVC | 19.11 (VS 2017 15.3) | 完整支持 |

`std::invoke` 需要 C++17 编译模式。头文件为 `<functional>`。

## 最佳实践

- **统一调用语法**：在泛型代码中使用 `std::invoke` 可以同时处理自由函数和成员函数。
- **配合 `std::invoke_result_t`**：获取可调用对象的返回类型，用于 SFINAE 和 Concepts。
- **成员函数指针是核心用途**：`std::invoke(&Class::method, obj, args...)` 比 `obj.method(args...)` 更灵活。
- **不要滥用**：简单场景直接调用更清晰，`std::invoke` 适合泛型和元编程场景。

## 常见陷阱

```cpp
// 陷阱 1：成员函数指针需要对象参数
struct Foo { int bar() { return 42; } };
Foo f;
// std::invoke(&Foo::bar);  // 编译错误！缺少 this
std::invoke(&Foo::bar, f);   // OK

// 陷阱 2：const 成员函数
struct Bar { int value() const { return 1; } };
Bar b;
// 非 const 引用也能调用 const 成员函数
int v = std::invoke(&Bar::value, b);

// 陷阱 3：返回引用的成员变量
struct S { int x; };
S s{10};
int& ref = std::invoke(&S::x, s);  // OK，ref 是 s.x 的引用
ref = 20;  // s.x 现在是 20
```
