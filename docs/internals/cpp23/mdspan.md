---
title: "std::mdspan 实现分析"
topic: internals
feature: mdspan
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/std/mdspan"
source_llvm: "references/impl/llvm-project/libcxx/include/__mdspan/"
---

# std::mdspan 实现分析

> `std::mdspan` 是 C++23 引入的多维数组视图，提供对连续内存的多维访问。本文基于 GCC 和 LLVM 的源码，分析 mdspan 的内部实现。

---

## 一、核心概念

### 1.1 什么是 mdspan

mdspan 是对连续内存的多维视图：

```cpp
// mdspan 的基本使用
int data[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
mdspan<int, extents<size_t, 3, 4>> m(data);

// 访问元素
int val = m[1, 2];  // 第 1 行第 2 列
```

### 1.2 核心数据结构

### 2.1 extents（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/std/mdspan

// extents 定义维度
template<size_t... Extents>
class extents {
    size_t extents_[sizeof...(Extents)];
    
public:
    // 获取维度大小
    template<size_t _Np>
    constexpr size_t extent() const noexcept {
        return extents_[_Np];
    }
    
    // 获取总大小
    constexpr size_t extent_size() const noexcept {
        return (Extents * ...);
    }
};
```

### 2.2 layout（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/std/mdspan

// 行优先布局
struct layout_right {
    template<typename _Extents>
    class mapping {
        _Extents extents_;
        
    public:
        // 计算线性偏移
        constexpr size_t operator()(size_t __i, size_t __j) const noexcept {
            return __i * extents_.extent<1>() + __j;
        }
    };
};

// 列优先布局
struct layout_left {
    template<typename _Extents>
    class mapping {
        _Extents extents_;
        
    public:
        constexpr size_t operator()(size_t __i, size_t __j) const noexcept {
            return __i + __j * extents_.extent<0>();
        }
    };
};
```

---

## 三、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ mdspan                 │ 支持                 │ 支持                 │
│ extents                │ 支持                 │ 支持                 │
│ layout_right           │ 支持                 │ 支持                 │
│ layout_left            │ 支持                 │ 支持                 │
│ layout_stride          │ 支持                 │ 支持                 │
│ submdspan              │ C++26                │ C++26                │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 延伸阅读

- [std::span 实现](/standards/cpp20/span) — 一维数组视图
- [std::vector 实现](/internals/containers/vector) — 动态数组
