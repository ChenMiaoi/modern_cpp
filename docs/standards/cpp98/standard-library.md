---
title: "C++98 标准库"
topic: unknown
feature: standard-library
standard: N/A
status_checked_at: 2026-06-02
---
# C++98 标准库

## STL 容器

| 容器 | 类型 | 底层实现 | 说明 |
|------|------|---------|------|
| `vector` | 序列 | 动态数组 | 连续内存，随机访问 O(1) |
| `deque` | 序列 | 分段数组 | 两端插入 O(1) |
| `list` | 序列 | 双向链表 | 任意位置插入 O(1) |
| `set` / `multiset` | 关联 | 红黑树 | 有序集合 |
| `map` / `multimap` | 关联 | 红黑树 | 有序键值对 |
| `stack` | 容器适配器 | 默认基于 `deque` | LIFO |
| `queue` | 容器适配器 | 默认基于 `deque` | FIFO |
| `priority_queue` | 容器适配器 | 默认基于 `vector` | 优先队列 |
| `bitset` | 特殊 | 固定位集合 | 位操作 |

## 迭代器

五类迭代器层次（能力递增）：

1. **Input Iterator** — 只读，单次遍历
2. **Output Iterator** — 只写，单次遍历
3. **Forward Iterator** — 可读写，多次前向遍历
4. **Bidirectional Iterator** — 双向遍历
5. **Random Access Iterator** — 随机访问

## 算法

常用算法示例：

```cpp
// 排序
std::sort(vec.begin(), vec.end());

// 查找
auto it = std::find(vec.begin(), vec.end(), 42);

// 变换（C++98 需要手写函数对象或函数指针）
struct Double {
    int operator()(int x) const { return x * 2; }
};
std::transform(src.begin(), src.end(), dst.begin(), Double());
// 注意：C++11 起可写为 [](int x){ return x*2; }

// 累加
int sum = std::accumulate(vec.begin(), vec.end(), 0);

// 删除-擦除惯用法
vec.erase(std::remove(vec.begin(), vec.end(), value), vec.end());
```

## 函数对象

C++98 通过函数对象（functor）实现可传递的"回调"：

```cpp
struct Greater {
    bool operator()(int a, int b) const { return a > b; }
};

std::sort(vec.begin(), vec.end(), Greater());
```

标准库提供了 `std::less`、`std::greater` 等预定义函数对象，以及 `std::bind1st`/`std::bind2nd` 适配器（C++11 被 `std::bind` 和 lambda 取代）。

## 字符串与 I/O

- **`std::string`**：动态字符串，支持拼接、查找、子串等操作
- **`<iostream>`**：`cin`/`cout`/`cerr`
- **`<fstream>`**：文件 I/O
- **`<sstream>`**：字符串流

## `auto_ptr`（已废弃）

C++98 的智能指针 `auto_ptr` 有致命的设计缺陷——拷贝时会转移所有权，这使得它在容器中使用时会导致悬空指针。C++11 用 `unique_ptr` 彻底替代了它。
