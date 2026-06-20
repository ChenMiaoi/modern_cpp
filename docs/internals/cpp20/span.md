---
title: "std::span 实现分析"
topic: internals
feature: span
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/span"
source_llvm: "references/impl/llvm-project/libcxx/include/span"
---

# std::span 实现分析

> `std::span` 是 C++20 引入的非拥有一维数组视图。本文基于 GCC 和 LLVM 的源码，分析 span 的内部实现。

---

## 一、核心概念

### 1.1 什么是 span

span 是对连续内存的非拥有视图：

```cpp
// span 的基本使用
int arr[] = {1, 2, 3, 4, 5};
span<int> s(arr);

// 访问元素
cout << s[0] << endl;  // 1
cout << s.size() << endl;  // 5

// 子视图
span<int> sub = s.subspan(1, 3);  // {2, 3, 4}
```

### 1.2 核心数据结构

### 2.1 存储布局

```
span 的内存布局：

span<int>（动态大小）：
┌─────────────────────────────────────┐
│ _M_data（指针）                      │
├─────────────────────────────────────┤
│ _M_len（大小）                       │
└─────────────────────────────────────┘

span<int, 5>（静态大小）：
┌─────────────────────────────────────┐
│ _M_data（指针）                      │
└─────────────────────────────────────┘
```

### 2.2 GCC (libstdc++) 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/span

// span 的实现
template<typename _Tp, size_t _Extent = dynamic_extent>
class span {
    pointer _M_data;
    
    static constexpr size_type _S_extent = _Extent;
    
public:
    // 构造函数
    template<size_t _N>
    constexpr span(element_type (&__arr)[_N]) noexcept
    : _M_data(__arr) {}
    
    // 迭代器
    constexpr iterator begin() const noexcept {
        return iterator(_M_data);
    }
    
    constexpr iterator end() const noexcept {
        return iterator(_M_data + _S_extent);
    }
    
    // 大小
    constexpr size_type size() const noexcept { return _S_extent; }
    
    // 元素访问
    constexpr reference operator[](size_type __idx) const {
        return _M_data[__idx];
    }
    
    // 数据指针
    constexpr pointer data() const noexcept { return _M_data; }
    
    // 子视图
    constexpr span subspan(size_type __offset, size_type __count) const {
        return span(_M_data + __offset, __count);
    }
};
```

---

## 三、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ span                   │ 支持                 │ 支持                 │
│ dynamic_extent         │ 支持                 │ 支持                 │
│ 静态大小               │ 支持                 │ 支持                 │
│ subspan                │ 支持                 │ 支持                 │
│ first/last             │ 支持                 │ 支持                 │
│ as_bytes               │ 支持                 │ 支持                 │
│ constexpr              │ C++20                │ C++20                │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 四、最佳实践

```
span 使用指南：

1. 使用 span 作为函数参数：
   · 比指针+大小更安全
   · 比 vector 引用更通用

2. 注意生命周期：
   · span 不拥有内存
   · 确保底层数据有效

3. 使用 extents 指定大小：
   · span<int, 5> s(arr);
   · 编译期检查大小

4. 使用 subspan 创建视图：
   · 不拷贝数据
   · 轻量级操作
```

---

## 延伸阅读

- [std::vector 实现](/internals/containers/vector) — 拥有内存的容器
- [std::mdspan 实现](/internals/cpp23/mdspan) — 多维数组视图
- [Ranges 框架](/internals/algorithms/ranges) — span 与 ranges 的交互
