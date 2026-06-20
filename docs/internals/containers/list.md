---
title: "std::list 实现分析"
topic: internals
feature: list
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/bits/stl_list.h"
source_llvm: "references/impl/llvm-project/libcxx/include/list"
---

# std::list 实现分析

> `std::list` 是双向链表，提供 O(1) 的插入/删除操作。本文基于 GCC 和 LLVM 的源码，分析 list 的内部实现。

---

## 一、核心数据结构

### 1.1 节点结构

```
list 节点布局：

┌─────────────────────────────────────┐
│ _M_prev（指向前一个节点）            │
├─────────────────────────────────────┤
│ _M_next（指向下一个节点）            │
├─────────────────────────────────────┤
│ _M_data（存储的数据）                │
└─────────────────────────────────────┘
```

### 1.2 GCC (libstdc++) 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/stl_list.h

// 节点结构
template<typename _Tp>
struct _List_node : __detail::_List_node_header {
    _List_node* _M_next;
    _List_node* _M_prev;
    _Tp _M_data;
    
    // 获取数据指针
    _Tp* _M_valptr() noexcept {
        return std::__addressof(_M_data);
    }
    
    const _Tp* _M_valptr() const noexcept {
        return std::__addressof(_M_data);
    }
};

// 哨兵节点
struct _List_node_header : _List_node_base {
    size_type _M_node_count;
    
    void _M_init() {
        _M_node_count = 0;
        _M_base._M_next = this;
        _M_base._M_prev = this;
    }
};
```

---

## 二、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ 节点结构               │ _List_node           │ __list_node          │
│ 哨兵节点               │ 支持                 │ 支持                 │
│ splice                 │ 支持                 │ 支持                 │
│ remove                 │ 支持                 │ 支持                 │
│ unique                 │ 支持                 │ 支持                 │
│ merge                  │ 支持                 │ 支持                 │
│ sort                   │ 支持                 │ 支持                 │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 延伸阅读

- [std::forward_list 实现](/internals/containers/forward-list) — 单向链表
- [std::vector 实现](/internals/containers/vector) — 连续存储容器
