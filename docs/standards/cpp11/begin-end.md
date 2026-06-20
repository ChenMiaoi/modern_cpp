---
title: "std::begin / std::end"
topic: unknown
feature: begin-end
standard: N/A
status_checked_at: 2026-06-20
---
# std::begin / std::end

## 概述

`std::begin` 和 `std::end` 是 C++11 引入的自由函数，用于以统一的方式获取容器或数组的迭代器。在此之前，获取容器的起止迭代器需要使用成员函数（`.begin()` / `.end()`），而原始数组不支持成员函数调用——`std::begin`/`std::end` 将两者统一为相同的语法。

头文件：`<iterator>`

```cpp
#include <iterator>
```

## 基本用法

### 容器

```cpp
#include <vector>
#include <iterator>

std::vector<int> v = {1, 2, 3, 4, 5};

auto it_begin = std::begin(v);  // 等同于 v.begin()
auto it_end   = std::end(v);    // 等同于 v.end()

for (auto it = std::begin(v); it != std::end(v); ++it) {
    std::cout << *it << ' ';
}
```

### 原始数组

原始数组无法调用成员函数，但 `std::begin`/`std::end` 可以直接处理：

```cpp
int arr[] = {10, 20, 30, 40, 50};

for (auto it = std::begin(arr); it != std::end(arr); ++it) {
    std::cout << *it << ' ';
}
```

```cpp
// 没有 std::begin/std::end 时，需要手动计算指针
for (int* p = arr; p != arr + 5; ++p) {
    std::cout << *p << ' ';
}
```

### 为什么需要自由函数

```cpp
template <typename Iter>
void print_range(Iter begin, Iter end) {
    for (auto it = begin; it != end; ++it) {
        std::cout << *it << ' ';
    }
}

// 使用成员函数: 只适用于容器
std::vector<int> v = {1, 2, 3};
print_range(v.begin(), v.end());  // OK

int arr[] = {4, 5, 6};
// print_range(arr.begin(), arr.end());  // 错误: 数组没有成员函数
print_range(std::begin(arr), std::end(arr));  // OK
```

## C++14 增强：std::cbegin / std::cend

C++14 引入了 `std::cbegin` 和 `std::cend`，返回 `const_iterator`：

```cpp
#include <vector>
#include <iterator>

std::vector<int> v = {1, 2, 3};

for (auto it = std::cbegin(v); it != std::cend(v); ++it) {
    // *it = 10;  // 编译错误: const_iterator 不可修改
    std::cout << *it << ' ';
}
```

```cpp
// C++11 中需要用 std::begin 的 const 重载或 std::as_const (C++17)
std::vector<int> v = {1, 2, 3};
auto it = std::begin(static_cast<const std::vector<int>&>(v));  // 不便
```

## 反向迭代器：std::rbegin / std::rend

C++11 同时提供了反向迭代器版本：

```cpp
#include <vector>
#include <iterator>

std::vector<int> v = {1, 2, 3, 4, 5};

for (auto it = std::rbegin(v); it != std::rend(v); ++it) {
    std::cout << *it << ' ';  // 5 4 3 2 1
}
```

```cpp
// 原始数组也支持
int arr[] = {10, 20, 30};
for (auto it = std::rbegin(arr); it != std::rend(arr); ++it) {
    std::cout << *it << ' ';  // 30 20 10
}
```

C++14 同样提供了 `std::crbegin` 和 `std::crend`。

## 与范围 for 循环的配合

C++11 的范围 for 循环内部使用 `std::begin`/`std::end`：

```cpp
std::vector<int> v = {1, 2, 3};
for (auto x : v) { /* ... */ }  // 编译器转换为使用 std::begin/std::end

int arr[] = {4, 5, 6};
for (auto x : arr) { /* ... */ }  // 同样适用
```

## 代码示例

### 泛型算法中的统一迭代

```cpp
#include <iterator>
#include <vector>
#include <list>
#include <iostream>

template <typename Container>
void print_all(const Container& c) {
    // 使用 cbegin/cend 保证只读迭代
    for (auto it = std::cbegin(c); it != std::cend(c); ++it) {
        std::cout << *it << ' ';
    }
    std::cout << '\n';
}

int main() {
    std::vector<int> v = {1, 2, 3};
    std::list<double> l = {1.1, 2.2, 3.3};
    int arr[] = {10, 20, 30};

    print_all(v);  // 1 2 3
    print_all(l);  // 1.1 2.2 3.3
    print_all(arr); // 10 20 30
}
```

### 与算法库配合

```cpp
#include <algorithm>
#include <iterator>
#include <vector>
#include <iostream>

int main() {
    std::vector<int> v = {5, 3, 1, 4, 2};

    // std::begin/std::end 可直接传给算法
    std::sort(std::begin(v), std::end(v));

    // 反向排序
    std::sort(std::rbegin(v), std::rend(v));

    for (auto x : v) {
        std::cout << x << ' ';  // 5 4 3 2 1
    }
}
```

## 注意事项与陷阱

**std::end 对空容器**——`std::end` 对空容器返回的迭代器等于 `std::begin`，解引用是未定义行为：

```cpp
std::vector<int> empty;
auto it = std::begin(empty);
if (it != std::end(empty)) {
    std::cout << *it;  // 安全
}
```

**数组大小必须固定**——`std::begin`/`std::end` 要求数组大小在编译期已知：

```cpp
int arr[] = {1, 2, 3};        // OK: 编译期大小
// int n; std::cin >> n;
// int arr2[n];                // VLA: 不是标准 C++，std::begin 可能无法使用
```

**指针退化**——传递给函数后数组退化为指针，`std::begin`/`std::end` 不再适用：

```cpp
void f(int arr[]) {
    // std::begin(arr);  // 编译错误: arr 已退化为 int*
    int n = sizeof(arr) / sizeof(arr[0]);  // 错误: 不是数组大小
}
```

## 与 C++ 之前的对比

| 特性 | C++03 | C++11 |
|------|-------|-------|
| 容器起始迭代器 | `c.begin()` | `std::begin(c)` |
| 容器终止迭代器 | `c.end()` | `std::end(c)` |
| 数组起始指针 | `arr` 或 `&arr[0]` | `std::begin(arr)` |
| 数组终止指针 | `arr + N` | `std::end(arr)` |
| const 迭代器 | `c.begin()` 返回类型 | `std::cbegin(c)` (C++14) |
| 反向迭代器 | `c.rbegin()` / `c.rend()` | `std::rbegin(c)` / `std::rend(c)` |

## 编译器支持

| 编译器 | 支持版本 | 备注 |
|--------|----------|------|
| GCC | 4.5+ | 完全支持 |
| Clang | 3.1+ | 完全支持 |
| MSVC | 2012 (17.0)+ | 完全支持 |

`std::begin`/`std::end` 的引入使得 C++ 泛型编程中对容器和数组的处理更加一致，是范围 for 循环和许多现代 C++ 惯用法的基础设施。
