---
title: "std::array 实现分析"
topic: internals
feature: array
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/array"
source_llvm: "references/impl/llvm-project/libcxx/include/array"
---

# std::array 实现分析

> `std::array` 是固定大小的数组容器，提供 STL 容器接口。本文基于 GCC 和 LLVM 的源码，分析 array 的内部实现。

---

## 一、核心数据结构

### 1.1 存储布局

```
array 的存储布局：

array<int, 4>：
┌─────────────────────────────────────┐
│ _M_elems[0]                         │
├─────────────────────────────────────┤
│ _M_elems[1]                         │
├─────────────────────────────────────┤
│ _M_elems[2]                         │
├─────────────────────────────────────┤
│ _M_elems[3]                         │
└─────────────────────────────────────┘

与 C 数组相同
```

### 1.2 GCC (libstdc++) 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/array

// array 的实现
template<typename _Tp, std::size_t _Nm>
struct array {
    // 存储类型
    struct _AT_Type {
        _Tp _M_elems[_Nm];
    };
    
    _AT_Type _M_elems;
    
    // 元素访问
    constexpr reference operator[](size_type __n) noexcept {
        return _M_elems._M_elems[__n];
    }
    
    constexpr const_reference operator[](size_type __n) const noexcept {
        return _M_elems._M_elems[__n];
    }
    
    // 迭代器
    constexpr iterator begin() noexcept {
        return iterator(&_M_elems._M_elems[0]);
    }
    
    constexpr iterator end() noexcept {
        return iterator(&_M_elems._M_elems[_Nm]);
    }
    
    // 大小
    constexpr size_type size() const noexcept { return _Nm; }
    
    // 容量
    constexpr size_type max_size() const noexcept { return _Nm; }
    
    // 是否为空
    constexpr bool empty() const noexcept { return false; }
    
    // 访问第一个元素
    constexpr reference front() noexcept {
        return _M_elems._M_elems[0];
    }
    
    // 访问最后一个元素
    constexpr reference back() noexcept {
        return _M_elems._M_elems[_Nm - 1];
    }
    
    // 获取数据指针
    constexpr _Tp* data() noexcept {
        return _M_elems._M_elems;
    }
};
```

---

## 二、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ 存储实现               │ _M_elems 数组        │ _Elems 数组          │
│ constexpr 支持         │ C++14/17/20          │ C++14/17/20          │
│ aggregate              │ 支持                 │ 支持                 │
│ get\<N\>               │ 支持                 │ 支持                 │
│ tuple_size             │ 支持                 │ 支持                 │
│ 三向比较               │ C++20                │ C++20                │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 延伸阅读

- [std::vector 实现](/internals/containers/vector) — 动态数组
- [std::tuple 实现](/internals/utilities/tuple) — 异构容器
