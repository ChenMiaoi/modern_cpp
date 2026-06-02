---
title: range-v3 Actions 与 Sentinels
topic: libraries
feature: actions-sentinels
standard: C++20
status_checked_at: 2026-06-02
exercises: []
solutions: []
---
# range-v3 Actions 与 Sentinels

## Actions：Eager 操作

与 Views 的惰性求值不同，Actions 直接修改容器：

```cpp
using namespace ranges;

std::vector<int> v = {3, 1, 4, 1, 5, 9};

// Actions 是 eager 的——直接修改 v
v |= actions::sort | actions::unique;
// v 现在是 {1, 3, 4, 5, 9}

v |= actions::transform([](int x) { return x * 2; });
// v 现在是 {2, 6, 8, 10, 18}
```

Actions 的管道运算符 `|=` 与 Views 的 `|` 不同：`|=` 原地修改左操作数，返回引用。

## Sentinel：哨兵概念

Sentinel 是 range-v3 的关键创新——`end()` 不一定是迭代器，可以是任何与迭代器可比较的类型：

```cpp
// 查找 null 终止字符串的 end
auto sv = ranges::subrange(ptr, nullptr);  // 迭代器 + nullptr sentinel

// 截断到最大长度
auto sv2 = ranges::subrange(ptr, ptr + max_len);
```

Sentinel 的价值：某些序列的终止条件是"值等于哨兵"（如 null 终止字符串），而不是"到达某个位置"。Sentinel 概念允许编译器优化终止检查——对于 null 终止字符串，哨兵比较只需一次指针解引用和比较，比传统迭代器的双指针比较更高效。

```cpp
template <class S, class I>
concept sentinel_for = semiregular<S> && input_or_output_iterator<I> &&
    weakly_equality_comparable_with<S, I>;
```
