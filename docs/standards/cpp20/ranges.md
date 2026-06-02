---
title: C++20 Ranges（范围库）
topic: cpp20
feature: ranges
standard: C++20
status_checked_at: 2026-06-01
exercises:
  - exercises/cpp20/ranges1.cpp
solutions:
  - exercises/solutions/ranges1.cpp
---
# C++20 Ranges（范围库）

## 概述

C++20 Ranges 是对 STL 算法和容器抽象的一次根本性升级。传统 STL 算法接受一对迭代器（`begin`/`end`），容易产生类型不匹配、越界、冗余参数等问题。Ranges 将「可遍历的序列」提升为一等公民，引入 **range** 概念、**view** 视图和 **pipe operator `|`** 链式组合，使数据处理代码从命令式变为声明式。

核心目标：用单一 range 对象代替迭代器对；通过 view 实现惰性求值（lazy evaluation），避免中间容器分配；提供可组合的管道式数据变换语法。头文件：`<ranges>`，算法的 ranges 重载在 `<algorithm>` 中。

## Range 概念层次

| Concept | 要求 | 典型类型 |
|---|---|---|
| `input_range` | 至少可遍历一次 | `std::istream_view` |
| `forward_range` | 支持多遍遍历 | `std::forward_list` |
| `bidirectional_range` | 支持反向迭代 | `std::list` |
| `random_access_range` | O(1) 下标访问 | `std::deque` |
| `contiguous_range` | 元素内存连续 | `std::vector`, `std::string` |
| `viewable_range` | 可被适配为 view | 左值 range 或某些右值 |

```cpp
#include <ranges>
#include <vector>
#include <list>
static_assert(std::ranges::contiguous_range<std::vector<int>>);
static_assert(std::ranges::bidirectional_range<std::list<int>>);
static_assert(std::ranges::input_range<std::list<int>>);
// std::list 不满足 random_access_range
```

## View 与惰性求值

**View** 是轻量级的 range 适配结果，满足 `std::ranges::view` concept。惰性求值：view 不会立即计算，仅在迭代时按需产生元素。O(1) 构造与拷贝，多个 view 通过 `|` 串联形成管道：

```cpp
#include <ranges>
#include <vector>
#include <iostream>
int main() {
    std::vector data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto result = data
        | std::views::filter([](int n) { return n % 2 == 0; })
        | std::views::transform([](int n) { return n * n; });
    for (int v : result)
        std::cout << v << ' ';  // 4 16 36 64 100
}
```

`result` 本身只是一个轻量 view 对象，`for` 循环遍历时才逐元素执行 filter 和 transform。

## 常用 View 适配器

所有适配器均位于 `std::views`（即 `std::ranges::views`）命名空间：

```cpp
std::vector data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

// filter：保留满足谓词的元素
auto even = data | std::views::filter([](int n) { return n % 2 == 0; });
// transform：对每个元素应用变换
auto squared = data | std::views::transform([](int n) { return n * n; });
// take / drop：取前 N 个 / 跳过前 N 个
auto first3 = std::views::iota(1) | std::views::take(3);  // 1, 2, 3
auto skip2  = data | std::views::drop(2);                  // 从第3个开始
// iota：生成整数序列（可无限）
auto naturals = std::views::iota(1);      // 无限: 1, 2, 3, ...
auto bounded  = std::views::iota(1, 10);  // 有界: 1..9
// join：展平嵌套 range
std::vector<std::vector<int>> nested = {{1, 2}, {3, 4}, {5}};
auto flat = nested | std::views::join;    // 1, 2, 3, 4, 5
// reverse / elements
auto rev = data | std::views::reverse;    // 10, 9, ..., 1
std::vector<std::pair<int, std::string>> items = {{1, "a"}, {2, "b"}};
auto keys = items | std::views::elements<0>;  // 1, 2
```

## 管道组合与 Range 适配器

`|` 运算符将左侧的 range 与右侧的 view 适配器连接，支持任意深度的链式组合：

```cpp
#include <ranges>
#include <vector>
#include <iostream>
#include <string>
int main() {
    std::vector<std::string> words = {
        "the", "quick", "brown", "fox", "jumps", "over", "lazy", "dog"
    };
    auto result = words
        | std::views::filter([](const std::string& s) { return s.size() >= 4; })
        | std::views::transform([](std::string s) {
              for (auto& c : s) c = static_cast<char>(std::toupper(c));
              return s;
          })
        | std::views::take(3);
    for (const auto& w : result)
        std::cout << w << ' ';  // QUICK BROWN JUMPS
}
```

适配器本身是函数对象，也可以直接调用：`std::views::filter(pred)(range)` 等价于 `range | std::views::filter(pred)`。

## subrange 与 common_view

`std::ranges::subrange` 将一对迭代器（或迭代器+哨兵）封装为 range 对象，解决「迭代器对散落各处」的问题：

```cpp
std::vector v = {10, 20, 30, 40, 50};
std::ranges::subrange sub(v.begin() + 1, v.begin() + 4);
for (int x : sub) std::cout << x << ' ';  // 20 30 40
```

`std::views::common` 将哨兵类型不同的 view 适配为 `begin`/`end` 类型一致的 common view，用于与旧代码交互：
```cpp
auto cv = std::views::iota(1, 10) | std::views::common;
```

## ranges 命名空间算法

C++20 为几乎所有 STL 算法提供了 `std::ranges::` 版本，接受 range 而非迭代器对，并支持投影（projection）：

```cpp
std::vector v = {5, 3, 1, 4, 2};
std::ranges::sort(v);
auto found = std::ranges::find(v, 4);
bool all_pos = std::ranges::all_of(v, [](int n) { return n > 0; });
// projection：按指定字段排序
std::vector<std::pair<int, std::string>> items = {{3, "c"}, {1, "a"}, {2, "b"}};
std::ranges::sort(items, {}, &std::pair<int,std::string>::first);
```

## 与传统 STL 对比

| 维度 | 传统 STL | Ranges |
|---|---|---|
| 接口 | 迭代器对 `begin/end` | 单一 range 对象 |
| 链式操作 | 需要中间容器或手动循环 | `\|` 管道，零中间分配 |
| 惰性 | 无（算法立即执行） | view 按需计算 |
| 错误提示 | 模板错误冗长 | concept 约束，错误更清晰 |
| 自定义 | 重载算法或写函数对象 | 实现 `view_interface` 或适配器 |

## C++23 新增视图

```cpp
std::vector a = {1, 2, 3};
std::vector b = {'a', 'b', 'c'};
// views::zip：并行遍历多个 range
for (auto [x, y] : std::views::zip(a, b))
    std::cout << x << y << ' ';  // 1a 2b 3c
// views::enumerate：带下标遍历
for (auto [i, v] : std::views::enumerate(a))
    std::cout << i << ':' << v << ' ';  // 0:1 1:2 2:3
// views::chunk：按大小分块
std::vector data = {1, 2, 3, 4, 5, 6, 7};
for (auto chunk : data | std::views::chunk(3)) {
    for (int v : chunk) std::cout << v << ' ';
    std::cout << '|';  // 1 2 3 | 4 5 6 | 7 |
}
// views::slide：滑动窗口
for (auto window : data | std::views::slide(3)) { /* 每个 window 是 3 元素 view */ }
// ranges::to：view -> 容器
auto vec = (data | std::views::filter([](int n) { return n % 2; }))
           | std::ranges::to<std::vector>();
```

## 最佳实践

1. **优先使用 view 管道替代手写循环**：filter/transform/take 组合比嵌套循环更清晰，且零分配开销。
2. **需要物化时用 `ranges::to`**（C++23）或 `std::vector(view.begin(), view.end())`。
3. **避免对 view 重复迭代**：部分 view（如 `filter` 的结果）不满足 `forward_range`，只能遍历一次。
4. **确保 view 生命周期安全**：view 内部持有引用或迭代器，原始数据必须比 view 存活更久。
5. **使用 `std::ranges::` 算法版本**：签名更清晰，支持哨兵和投影。

## 常见陷阱

1. **悬挂引用**：view 保存的是引用或迭代器，原始数据销毁后继续使用 view 是未定义行为：
   ```cpp
   auto dangling() {
       std::vector v = {1, 2, 3};
       return v | std::views::filter([](int n) { return n > 1; });
       // UB：v 已销毁，view 内部迭代器无效
   }
   ```

2. **对临时 range 构造 view**：`auto v = getTemporaryVector() | std::views::transform(...)` 中临时对象在语句结束后销毁，view 持有悬空迭代器。

3. **不理解惰性语义**：view 管道不会立即执行，`auto lazy = data | std::views::transform(f) | std::views::filter(g);` 中 `f` 和 `g` 都没被调用，只有遍历 `lazy` 时才触发。

4. **view 不满足某些 range concept**：如 `filter_view` 不满足 `random_access_range`，即使底层 range 满足。

5. **`views::split` 返回的子 range 在 C++20 中不可直接构造 `std::string`**（C++23 `ranges::to` 可解决）。

6. **`iota` 无限范围不加 `take` 会导致无限循环**：
   ```cpp
   // 错误：无限循环
   for (int n : std::views::iota(1)) { /* ... */ }
   // 正确：加上界
   for (int n : std::views::iota(1) | std::views::take(100)) { /* ... */ }
   ```
