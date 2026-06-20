---
title: "Range-based for"
topic: unknown
feature: range-based-for
standard: N/A
status_checked_at: 2026-06-20
---
# Range-based for

## 概述

Range-based for 是 C++11 引入的循环语法，用于简化对数组、容器和初始化列表的遍历。它消除了显式迭代器和索引管理，使代码更简洁、更安全。

## 语法

```cpp
for (auto& x : container) { /* ... */ }
for (const auto& x : container) { /* ... */ }
for (auto x : container) { /* ... */ }
```

## 展开机制

Range-based for 在编译期展开为等价的迭代器代码：

```cpp
// 你写的代码
for (auto& x : container) { use(x); }

// 编译器展开为
auto&& __range = container;
auto __begin = std::begin(__range);
auto __end = std::end(__range);
for (; __begin != __end; ++__begin) {
    auto& x = *__begin;
    use(x);
}
```

## 拷贝 vs 引用语义

```cpp
std::vector<int> v = {1, 2, 3};

// auto 会拷贝每个元素 — 浪费且可能出错
for (auto x : v) { x = 0; }  // 修改的是拷贝，原容器不变

// auto& 引用原元素 — 正确高效
for (auto& x : v) { x = 0; }  // 修改原容器

// const auto& 只读引用 — 最安全的只读遍历
for (const auto& x : v) { std::cout << x; }
```

## 支持的容器类型

Range-based for 可遍历以下类型：

| 类型 | 说明 |
|------|------|
| C 风格数组 | `int arr[] = {1, 2, 3};` |
| `std::vector`、`std::list` 等 | 标准容器 |
| `std::map`、`std::set` | 关联容器 |
| `std::initializer_list` | 初始化列表 |
| 自定义类型 | 需实现 `begin()` 和 `end()` |

## 代码示例

### 遍历关联容器

```cpp
#include <map>
#include <string>
#include <iostream>

int main() {
    std::map<std::string, int> ages = {
        {"Alice", 30}, {"Bob", 25}, {"Charlie", 35}
    };

    for (const auto& [name, age] : ages) {  // C++17 结构化绑定
        std::cout << name << ": " << age << "\n";
    }

    // C++11 兼容写法
    for (const auto& pair : ages) {
        std::cout << pair.first << ": " << pair.second << "\n";
    }
}
```

### 遍历 C 风格数组

```cpp
int arr[] = {10, 20, 30, 40, 50};

for (const auto& x : arr) {
    std::cout << x << " ";
}
// 输出: 10 20 30 40 50
```

### 修改容器元素

```cpp
#include <vector>
#include <algorithm>

int main() {
    std::vector<int> nums = {1, 2, 3, 4, 5};

    for (auto& x : nums) {
        x *= 2;  // 原地修改
    }
    // nums 现在是 {2, 4, 6, 8, 10}
}
```

## 注意事项与陷阱

**循环中修改容器**

```cpp
std::vector<int> v = {1, 2, 3, 4, 5};
for (auto& x : v) {
    if (x % 2 == 0) v.push_back(x * 10);  // 危险：修改容器可能使迭代器失效
}
```

在 range-based for 中修改容器大小（添加或删除元素）通常会导致未定义行为。如需在遍历中修改容器，应使用索引循环或显式迭代器。

**自定义类型需提供 `begin()` 和 `end()`**

```cpp
class Range {
    int start_, end_;
public:
    Range(int s, int e) : start_(s), end_(e) {}
    int* begin() { return &start_; }
    int* end() { return &end_; }
};

for (auto x : Range(1, 10)) {
    // 注意：这不会按预期工作，因为存储的是指针
    // 需要为迭代器类型提供 operator* 和 operator!=
}
```

**`auto` 拷贝陷阱**

对于大型对象，使用 `auto` 会触发拷贝构造函数，产生意外开销。始终使用 `const auto&` 进行只读遍历。

## 与传统循环对比

| 特性 | Range-based for | 索引循环 | 迭代器循环 |
|------|----------------|---------|-----------|
| 简洁性 | 最高 | 中等 | 较低 |
| 安全性 | 高 | 中（越界风险） | 中 |
| 修改元素 | 需 `auto&` | 直接 | 通过迭代器 |
| 删除元素 | 不支持 | 不推荐 | 支持 |
| 获取索引 | 不直接 | 直接 | 不直接 |

## 编译器支持

| 编译器 | 最低版本 |
|--------|---------|
| GCC | 4.6+ |
| Clang | 3.0+ |
| MSVC | 2012+（VS 11.0） |
