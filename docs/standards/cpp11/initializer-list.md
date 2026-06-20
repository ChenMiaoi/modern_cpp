---
title: "std::initializer_list"
topic: unknown
feature: initializer-list
standard: N/A
status_checked_at: 2026-06-20
---
# std::initializer_list

## 概述

`std::initializer_list<T>` 是 C++11 引入的轻量级容器适配器，用于让函数接受花括号初始化列表（brace-init-list）作为参数。它本身不是拥有数据的容器，而是对编译器提供的底层数组的只读视图。

典型用途包括：统一容器构造语法、实现可变参数构造函数、替代多个重载函数。

## 语法

```cpp
#include <initializer_list>

void func(std::initializer_list<int> list);

// 构造函数中使用
class MyVector {
public:
    MyVector(std::initializer_list<int> init);
};
```

## 特性

| 特性 | 说明 |
|------|------|
| 元素类型 | `const T`，不可修改 |
| 大小 | 编译期确定，不可动态增减 |
| 内存布局 | 编译器可能放在栈或只读内存区 |
| 生命周期 | 底层数组生命周期仅限于花括号所在语句 |
| 支持的操作 | `size()`、`begin()`、`end()`、范围迭代 |

## 代码示例

### 基本用法

```cpp
#include <initializer_list>
#include <vector>
#include <iostream>

class IntContainer {
    std::vector<int> data_;
public:
    IntContainer(std::initializer_list<int> init) : data_(init) {}

    void print() const {
        for (auto v : data_) std::cout << v << " ";
        std::cout << "\n";
    }
};

int main() {
    IntContainer c = {1, 2, 3, 4, 5};
    c.print();  // 1 2 3 4 5
}
```

### 作为独立函数参数

```cpp
double average(std::initializer_list<double> values) {
    double sum = 0;
    for (auto v : values) sum += v;
    return values.size() > 0 ? sum / values.size() : 0.0;
}

int main() {
    std::cout << average({1.0, 2.0, 3.0, 4.0});  // 2.5
}
```

### 嵌套 initializer_list

```cpp
#include <vector>
#include <initializer_list>

class Matrix {
    std::vector<std::vector<int>> data_;
public:
    Matrix(std::initializer_list<std::initializer_list<int>> rows) {
        for (auto& row : rows) {
            data_.emplace_back(row);
        }
    }
};

int main() {
    Matrix m = {
        {1, 2, 3},
        {4, 5, 6},
        {7, 8, 9}
    };
}
```

### 与构造函数重载对比

```cpp
class Widget {
public:
    Widget(int a, int b) { /* 两个参数 */ }
    Widget(std::initializer_list<int> list) { /* 列表 */ }
};

Widget w1(1, 2);     // 调用 Widget(int, int)
Widget w2{1, 2};     // 调用 initializer_list 版本
Widget w3 = {1, 2};  // 调用 initializer_list 版本（优先于 (int, int)）
```

## 注意事项与陷阱

**空列表产生歧义**

```cpp
Widget w1{};     // 调用默认构造函数（C++11）
Widget w2({});   // 调用 initializer_list（空列表）
```

**不可动态修改**

`initializer_list` 没有 `push_back`、`insert` 等操作。它是只读视图，大小固定。

**生命周期陷阱**

```cpp
std::initializer_list<int> get_list() {
    return {1, 2, 3};  // 危险：底层数据已超出作用域
}
```

`initializer_list` 不拥有数据。返回它时，底层数组可能已失效。实际编译器实现通常会复制，但这是未定义行为，应避免。

**性能注意**

对于少量元素，`initializer_list` 通常无额外堆分配（编译器内联到栈上）。但对于大量元素，可能有隐式拷贝开销。在性能敏感场景中，应考虑其他方案。

## 编译器支持

| 编译器 | 最低版本 |
|--------|---------|
| GCC | 4.4+ |
| Clang | 3.1+ |
| MSVC | 2013+（VS 12.0） |
