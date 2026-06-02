---
title: "`std::span`"
topic: unknown
feature: span
standard: N/A
status_checked_at: 2026-06-02
---
# `std::span`

## 概述

`std::span<T>` 是连续内存区域的非拥有视图，统一了 C 数组、`std::array`、`std::vector` 和任意连续存储的接口。取代 `(pointer, size)` 对，作为函数参数的标准方式。

## 基本用法

```cpp
#include <span>
#include <vector>
#include <iostream>

void print(std::span<const int> data) {
    for (int v : data) std::cout << v << ' ';
    std::cout << '\n';
}

int main() {
    std::vector<int> v = {1, 2, 3, 4, 5};
    int arr[] = {10, 20, 30};
    std::array<int, 3> sa = {100, 200, 300};

    print(v);    // OK
    print(arr);  // OK
    print(sa);   // OK
}
```

## 静态 vs 动态 extent

```cpp
std::span<int> dyn(data, 5);        // 动态 extent
std::span<int, 5> stat(arr, 5);     // 静态 extent：编译期已知大小

// sizeof(std::span<int>)     == 16（指针 + size）
// sizeof(std::span<int, 5>)  == 8（仅指针）
```

### 静态 extent 自动推导

```cpp
int arr[4] = {1, 2, 3, 4};
std::span s1(arr);         // span<int, 4>（静态）
std::span s2(arr, 3);      // span<int>（动态，前 3 个）

std::vector<int> v = {1, 2, 3};
std::span s3(v);           // span<int>（动态）

std::array<int, 5> a = {};
std::span s4(a);           // span<int, 5>（静态）
```

## 子视图操作

```cpp
int data[] = {0, 1, 2, 3, 4, 5, 6};
std::span s(data);

auto first3 = s.first(3);      // {0, 1, 2}
auto last2 = s.last(2);        // {5, 6}
auto mid = s.subspan(2, 3);    // {2, 3, 4}
auto rest = s.subspan(2);      // {2, 3, 4, 5, 6}
```

## 替代 `pointer + size`

```cpp
// 旧风格
void process_old(int* data, size_t count) {
    for (size_t i = 0; i < count; ++i) data[i] *= 2;
}

// 新风格
void process_new(std::span<int> data) {
    for (auto& v : data) v *= 2;
}

// 调用方无需改变
std::vector<int> v = {1, 2, 3};
int arr[] = {4, 5, 6};
process_new(v);
process_new(arr);
```

## 只读视图

```cpp
double sum(std::span<const double> values) {
    double s = 0;
    for (double v : values) s += v;
    return s;
}

std::vector<double> v = {1.0, 2.5, 3.7};
double total = sum(v);  // 隐式转换到 span<const double>
```

## 与 `std::string_view` 对比

| 特性 | `std::span<T>` | `std::string_view` |
|------|----------------|---------------------|
| 元素类型 | 任意 | 字符类型 |
| 可写 | 可（`span<T>`） | 仅只读 |
| 适用场景 | 通用连续数据 | 字符串操作 |

```cpp
// 字符串仍用 string_view
std::string_view sv = "hello";

// 非字符连续数据用 span
std::span<const uint8_t> bytes(buffer, len);
```

## 与范围算法配合

```cpp
#include <span>
#include <algorithm>

void sort_in_place(std::span<int> data) {
    std::sort(data.begin(), data.end());
}

int arr[] = {3, 1, 4, 1, 5, 9};
sort_in_place(arr);
```

## `as_bytes` / `as_writable_bytes`

```cpp
#include <span>

int data[] = {0x12345678, 0x9ABCDEF0};
auto bytes = std::as_bytes(std::span(data));           // 只读字节视图
auto wbytes = std::as_writable_bytes(std::span(data)); // 可写字节视图
```

## 常见陷阱

```cpp
// 陷阱 1：从临时对象构造 span → 悬挂引用
// std::span<int> s = std::vector<int>{1, 2, 3};  // UB！

// 陷阱 2：不检查边界
std::span<int> s(arr, 3);
// s[5] = 10;  // UB（与 raw 指针相同）

// 陷阱 3：静态 extent 构造不做运行期检查
// std::span<int, 5> s5(arr, 3);  // arr 只有 3 个元素 → UB

// 陷阱 4：span 不拥有数据——确保底层存储的生命周期
std::span<int> dangling() {
    std::vector<int> v = {1, 2, 3};
    return std::span<int>(v);  // v 销毁后 span 无效
}
```

## 总结

- `span<T>` 是连续内存的轻量非拥有视图，替代 `(ptr, size)` 模式。
- 静态 extent（`span<T, N>`）省去存储 size 的开销。
- `span` 不拥有数据——确保底层存储的生命周期。
- 优先用 `span<const T>` 只读参数，`span<T>` 读写参数。
- 字符串用 `string_view`，非字符串连续数据用 `span`。
