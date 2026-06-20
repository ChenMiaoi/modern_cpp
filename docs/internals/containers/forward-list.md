---
title: "std::forward_list 实现分析"
topic: internals
feature: forward-list
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/bits/forward_list.h"
source_llvm: "references/impl/llvm-project/libcxx/include/forward_list"
---

# std::forward_list 实现分析

> `std::forward_list` 是单向链表，提供 O(1) 的插入/删除操作。本文基于 GCC 和 LLVM 的源码，分析 forward_list 的内部实现。

---

## 一、核心数据结构

### 1.1 节点结构

```
forward_list 节点布局：

┌─────────────────────────────────────┐
│ _M_next（指向下一个节点）            │
├─────────────────────────────────────┤
│ _M_data（存储的数据）                │
└─────────────────────────────────────┘
```

### 1.2 GCC (libstdc++) 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/forward_list.h

// 节点结构
template<typename _Tp>
struct _Fwd_list_node : _Fwd_list_node_base {
    _Tp _M_data;
    
    // 获取数据指针
    _Tp* _M_valptr() noexcept {
        return std::__addressof(_M_data);
    }
    
    const _Tp* _M_valptr() const noexcept {
        return std::__addressof(_M_data);
    }
};

// before_begin 迭代器
struct _Fwd_list_iterator {
    _Fwd_list_node_base* _M_node;
    
    _Fwd_list_iterator& operator++() {
        _M_node = _M_node->_M_next;
        return *this;
    }
    
    _Fwd_list_iterator operator++(int) {
        _Fwd_list_iterator __tmp = *this;
        _M_node = _M_node->_M_next;
        return __tmp;
    }
    
    reference operator*() const {
        return *static_cast<_Fwd_list_node<_Tp>*>(_M_node)->_M_valptr();
    }
};
```

---

## 二、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ 节点结构               │ _List_node           │ __forward_list_node  │
│ before_begin           │ 支持                 │ 支持                 │
│ insert_after           │ 支持                 │ 支持                 │
│ erase_after            │ 支持                 │ 支持                 │
│ splice_after           │ 支持                 │ 支持                 │
│ resize                 │ 支持                 │ 支持                 │
│ before_begin 迭代器    │ 支持                 │ 支持                 │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 延伸阅读

- [std::list 实现](/internals/containers/list) — 双向链表
- [std::vector 实现](/internals/containers/vector) — 连续存储容器
