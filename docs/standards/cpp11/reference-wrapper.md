---
title: "std::reference_wrapper"
topic: unknown
feature: reference-wrapper
standard: N/A
status_checked_at: 2026-06-20
---
# std::reference_wrapper

## 概述

`std::reference_wrapper` 是 C++11 引入的包装器，用于将左值引用包装为**可复制、可赋值**的对象。普通的引用（`T&`）不可拷贝、不可赋值，也无法存储在标准容器中。`std::reference_wrapper` 解决了这个问题——它持有一个指向目标对象的指针，支持拷贝和赋值语义，同时可以隐式转换回 `T&`。

头文件：`<functional>`

```cpp
#include <functional>
```

## 基本用法

### 创建 reference_wrapper

```cpp
int x = 42;
std::reference_wrapper<int> r1 = std::ref(x);   // 推荐方式
std::reference_wrapper<int> r2(x);               // 直接构造

r1 = 99;          // 通过包装器修改原始值
std::cout << x;   // 输出 99
```

### std::ref() 与 std::cref()

```cpp
int val = 10;
std::reference_wrapper<int>       rw  = std::ref(val);    // 可修改引用
std::reference_wrapper<const int> crw = std::cref(val);   // const 引用

val = 20;
std::cout << rw.get() << '\n';   // 20
// crw.get() = 30;               // 编译错误: 不能通过 const 引用修改
```

`std::ref` 和 `std::cref` 是便捷工厂函数，返回 `std::reference_wrapper<T>` 和 `std::reference_wrapper<const T>`。

## 隐式转换

`std::reference_wrapper` 提供到 `T&` 的隐式转换，因此大多数接受引用的函数和算法可以直接使用它：

```cpp
void increment(int& v) { ++v; }

int n = 5;
std::reference_wrapper<int> rw = std::ref(n);
increment(rw);           // 隐式转换为 int&
std::cout << n;          // 6
```

## 在容器中存储引用

标准容器要求元素类型可拷贝或可移动。引用本身不满足这一要求，但 `std::reference_wrapper` 满足：

```cpp
#include <vector>
#include <functional>
#include <iostream>

int main() {
    int a = 1, b = 2, c = 3;
    std::vector<std::reference_wrapper<int>> refs = {std::ref(a), std::ref(b), std::ref(c)};

    for (auto& r : refs) {
        r.get() *= 10;   // 通过包装器修改原始对象
    }

    std::cout << a << ' ' << b << ' ' << c << '\n';  // 10 20 30
}
```

### 存储不同类型的引用

使用 `std::reference_wrapper` 时，容器中所有元素类型相同（`reference_wrapper<int>`），即使原始对象类型不同，也可以通过基类指针等方式存储：

```cpp
struct Base { int id = 0; };
struct Derived : Base { int extra = 42; };

Derived d;
std::reference_wrapper<Base> rw = std::ref(static_cast<Base&>(d));
```

## 在 std::bind 中的使用

`std::bind` 内部使用 `std::reference_wrapper` 来保持对参数的引用。默认情况下 `std::bind` 会拷贝参数，使用 `std::ref` 可以阻止拷贝：

```cpp
#include <functional>
#include <iostream>

void add_one(int& v) { ++v; }

int main() {
    int value = 10;
    auto bound = std::bind(add_one, std::ref(value));
    bound();              // value 被修改
    std::cout << value;   // 11
}
```

```cpp
// 不使用 std::ref 时，bind 会拷贝值
int value = 10;
auto bound = std::bind(add_one, value);  // 拷贝了 value
bound();              // 修改的是拷贝，原始 value 不变
std::cout << value;   // 10 (未改变)
```

## 不能绑定到右值

`std::reference_wrapper` 只能绑定到左值，不能绑定到右值（临时对象）：

```cpp
int x = 42;
std::reference_wrapper<int> r1 = std::ref(x);      // OK
// std::reference_wrapper<int> r2 = std::ref(42);   // 编译错误: 不能绑定右值

// 也不能绑定临时对象
std::string temp() { return "hello"; }
// auto r3 = std::ref(temp());  // 编译错误: 不能绑定临时对象的返回值
```

原因：如果允许绑定右值，临时对象在表达式结束后即被销毁，`reference_wrapper` 将持有一个悬垂引用。

## reference_wrapper 的成员

| 成员函数 | 说明 |
|----------|------|
| `get()` | 返回 `T&`，访问被包装的引用 |
| `operator T&()` | 隐式转换到 `T&` |
| `operator=` | 通过包装器赋值给被引用的对象 |

```cpp
int x = 5, y = 10;
std::reference_wrapper<int> r1 = std::ref(x);
std::reference_wrapper<int> r2 = std::ref(y);

r1 = r2;           // 等同于 x = y，x 变为 10
r1.get() = 100;    // x = 100
```

## 代码示例

### 实现一个泛型容器

```cpp
#include <vector>
#include <functional>
#include <algorithm>
#include <iostream>

template <typename T>
class MutableView {
    std::vector<std::reference_wrapper<T>> refs_;
public:
    void add(T& obj) { refs_.push_back(std::ref(obj)); }

    void scale_all(double factor) {
        for (auto& r : refs_) {
            r.get() = static_cast<T>(r.get() * factor);
        }
    }

    void print() const {
        for (const auto& r : refs_) {
            std::cout << r.get() << ' ';
        }
        std::cout << '\n';
    }
};

int main() {
    double a = 2.0, b = 3.0, c = 4.0;
    MutableView<double> view;
    view.add(a);
    view.add(b);
    view.add(c);

    view.scale_all(10.0);
    view.print();  // 20 30 40

    std::cout << a << ' ' << b << ' ' << c << '\n';  // 20 30 40
}
```

### reference_wrapper 作为回调参数

```cpp
#include <functional>
#include <vector>
#include <iostream>

void process(std::function<void(int&)> callback, int& val) {
    callback(val);
}

int main() {
    int counter = 0;

    auto increment = [](int& v) { ++v; };
    auto double_val = [](int& v) { v *= 2; };

    process(increment, counter);
    process(double_val, counter);

    std::cout << counter << '\n';  // 2
}
```

## 注意事项与陷阱

**不要持有 reference_wrapper 到临时对象**——虽然编译器会阻止 `std::ref(42)`，但如果通过 `reinterpret_cast` 或其他方式绕过类型系统，可能产生悬垂引用。

**reference_wrapper 不是 nullptr 安全的**——它不检查内部指针是否为空：

```cpp
std::reference_wrapper<int> r = std::ref(*static_cast<int*>(nullptr));
r.get();  // 未定义行为
```

**与 const 正确性**——`std::ref` 推导出 `reference_wrapper<T>`，`std::cref` 推导出 `reference_wrapper<const T>`。选择时要注意：

```cpp
int val = 0;
const auto& cr = std::cref(val);  // const 引用包装器
// cr.get() = 5;                  // 编译错误

auto r = std::ref(val);           // 可修改引用包装器
r.get() = 5;                      // OK
```

## 编译器支持

| 编译器 | 支持版本 | 备注 |
|--------|----------|------|
| GCC | 4.5+ | 完全支持 |
| Clang | 3.1+ | 完全支持 |
| MSVC | 2012 (17.0)+ | 完全支持 |

`std::reference_wrapper` 在 C++11 中被广泛支持，是标准库中唯一能将引用放入容器的机制。与 `std::bind` 的配合使其成为函数式编程风格中的重要工具。
