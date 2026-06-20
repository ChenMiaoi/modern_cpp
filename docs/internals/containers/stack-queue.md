---
title: "std::stack/queue 适配器实现分析"
topic: internals
feature: stack-queue
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/bits/stl_stack.h"
source_llvm: "references/impl/llvm-project/libcxx/include/stack"
---

# std::stack/queue 适配器实现分析

> `std::stack` 和 `std::queue` 是容器适配器，提供特定的接口。本文基于 GCC 和 LLVM 的源码，分析它们的内部实现。

---

## 一、核心概念

### 1.1 什么是容器适配器

容器适配器封装底层容器，提供特定接口：

```
stack：后进先出（LIFO）
  · push：入栈
  · pop：出栈
  · top：查看栈顶

queue：先进先出（FIFO）
  · push：入队
  · pop：出队
  · front：查看队首
  · back：查看队尾
```

### 1.2 GCC (libstdc++) 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/stl_stack.h

// stack 的实现
template<typename _Tp, typename _Sequence = deque<_Tp>>
class stack {
protected:
    _Sequence c;  // 底层容器
    
public:
    // 入栈
    void push(const value_type& __x) {
        c.push_back(__x);
    }
    
    void push(value_type&& __x) {
        c.push_back(std::move(__x));
    }
    
    // 出栈
    void pop() {
        c.pop_back();
    }
    
    // 查看栈顶
    reference top() {
        return c.back();
    }
    
    // 检查是否为空
    bool empty() const { return c.empty(); }
    
    // 获取大小
    size_type size() const { return c.size(); }
};

// queue 的实现
template<typename _Tp, typename _Sequence = deque<_Tp>>
class queue {
protected:
    _Sequence c;  // 底层容器
    
public:
    // 入队
    void push(const value_type& __x) {
        c.push_back(__x);
    }
    
    // 出队
    void pop() {
        c.pop_front();
    }
    
    // 查看队首
    reference front() { return c.front(); }
    
    // 查看队尾
    reference back() { return c.back(); }
    
    // 检查是否为空
    bool empty() const { return c.empty(); }
    
    // 获取大小
    size_type size() const { return c.size(); }
};
```

---

## 二、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ stack                  │ 容器适配器           │ 容器适配器           │
│ queue                  │ 容器适配器           │ 容器适配器           │
│ priority_queue         │ 容器适配器           │ 容器适配器           │
│ 默认底层容器           │ deque               │ deque               │
│ 支持自定义容器         │ 支持                 │ 支持                 │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 延伸阅读

- [std::deque 实现](/internals/containers/deque) — 默认底层容器
- [std::vector 实现](/internals/containers/vector) — 另一种底层容器
