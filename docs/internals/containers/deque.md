---
title: "std::deque 实现分析"
topic: internals
feature: deque
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/bits/stl_deque.h"
source_llvm: "references/impl/llvm-project/libcxx/include/deque"
---

# std::deque 实现分析

> `std::deque` 是分段连续存储容器，提供 O(1) 的头部/尾部插入。本文基于 GCC 和 LLVM 的源码，分析 deque 的内部实现。

---

## 一、核心数据结构

### 1.1 中控器

```
deque 的中控器布局：

中控器（map）：
┌──────┬──────┬──────┬──────┬──────┐
│ 指针 │ 指针 │ 指针 │ 指针 │ 指针 │
└──┬───┴──┬───┴──┬───┴──┬───┴──┬───┘
   │      │      │      │      │
   ▼      ▼      ▼      ▼      ▼
┌──────┬──────┬──────┬──────┬──────┐
│ 缓冲 │ 缓冲 │ 缓冲 │ 缓冲 │ 缓冲 │
│ 区 0 │ 区 1 │ 区 2 │ 区 3 │ 区 4 │
└──────┴──────┴──────┴──────┴──────┘
```

### 1.2 GCC (libstdc++) 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/stl_deque.h

// deque 的数据结构
struct _Deque_impl_data {
    _Tp** _M_map;           // 中控器
    size_type _M_map_size;  // 中控器大小
    pointer _M_start;       // 第一个元素
    pointer _M_finish;      // 最后一个元素的下一个位置
};

// 迭代器
struct _Deque_iterator {
    pointer _M_cur;     // 当前元素
    pointer _M_first;   // 当前缓冲区的起始
    pointer _M_last;    // 当前缓冲区的结束
    _Map_pointer _M_node;  // 指向中控器
    
    // 移动到下一个元素
    void _M_set_node(_Map_pointer __new_node) {
        _M_node = __new_node;
        _M_first = *__new_node;
        _M_last = _M_first + _S_buffer_size();
    }
};
```

---

## 二、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ 中控器                 │ _Map_pointer         │ __map_pointer        │
│ 缓冲区大小             │ 512 字节             │ 可配置               │
│ 迭代器                 │ _Deque_iterator      │ __deque_iterator     │
│ push_back              │ 支持                 │ 支持                 │
│ push_front             │ 支持                 │ 支持                 │
│ insert                 │ 支持                 │ 支持                 │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 延伸阅读

- [std::vector 实现](/internals/containers/vector) — 连续存储容器
- [std::list 实现](/internals/containers/list) — 链表容器
