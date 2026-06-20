---
title: "迭代器体系实现分析"
topic: internals
feature: iterators
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/bits/stl_iterator_base_types.h"
source_llvm: "references/impl/llvm-project/libcxx/include/__iterator/"
---

# 迭代器体系实现分析

> 迭代器是 C++ 标准库的核心抽象，提供统一的容器访问接口。本文基于 GCC 和 LLVM 的源码，分析迭代器体系的内部实现。

---

## 一、核心概念

### 1.1 什么是迭代器

迭代器是访问容器元素的抽象接口：

```cpp
// 迭代器的基本使用
vector<int> v = {1, 2, 3, 4, 5};
for (auto it = v.begin(); it != v.end(); ++it) {
    cout << *it << endl;
}
```

### 1.2 迭代器类别

```
迭代器类别（从弱到强）：

1. Input Iterator：只读，单次前进
2. Output Iterator：只写，单次前进
3. Forward Iterator：读写，多次前进
4. Bidirectional Iterator：读写，可前进后退
5. Random Access Iterator：读写，随机访问
6. Contiguous Iterator：连续存储（C++20）
```

---

## 二、核心数据结构

### 2.1 iterator_traits（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/stl_iterator_base_types.h:154

// 迭代器特性（C++11 SFINAE 友好版本）
template<typename _Iterator, typename = __void_t<>>
struct __iterator_traits { };

// 偏特化：当迭代器有必要的嵌套类型时
template<typename _Iterator>
struct __iterator_traits<_Iterator,
                         __void_t<typename _Iterator::iterator_category,
                                  typename _Iterator::value_type,
                                  typename _Iterator::difference_type,
                                  typename _Iterator::pointer,
                                  typename _Iterator::reference>> {
    typedef typename _Iterator::iterator_category iterator_category;
    typedef typename _Iterator::value_type        value_type;
    typedef typename _Iterator::difference_type   difference_type;
    typedef typename _Iterator::pointer           pointer;
    typedef typename _Iterator::reference         reference;
};

// 主模板
template<typename _Iterator>
struct iterator_traits : public __iterator_traits<_Iterator> { };

// 指针特化
template<typename _Tp>
struct iterator_traits<_Tp*> {
    typedef random_access_iterator_tag iterator_category;
    typedef _Tp                         value_type;
    typedef ptrdiff_t                   difference_type;
    typedef _Tp*                        pointer;
    typedef _Tp&                        reference;
};

// const 指针特化
template<typename _Tp>
struct iterator_traits<const _Tp*> {
    typedef random_access_iterator_tag iterator_category;
    typedef _Tp                         value_type;
    typedef ptrdiff_t                   difference_type;
    typedef const _Tp*                  pointer;
    typedef const _Tp&                  reference;
};
```

### 2.2 迭代器标签（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/stl_iterator_base_types.h:94

// 迭代器类别标签（空类型，用于区分迭代器能力）
struct input_iterator_tag { };           // 只读，单次前进
struct output_iterator_tag { };          // 只写，单次前进
struct forward_iterator_tag : public input_iterator_tag { };      // 读写，多次前进
struct bidirectional_iterator_tag : public forward_iterator_tag { };  // 读写，可后退
struct random_access_iterator_tag : public bidirectional_iterator_tag { };  // 读写，随机访问
struct contiguous_iterator_tag : public random_access_iterator_tag { };    // 连续存储（C++20）

// 迭代器基类（C++17 已弃用）
template<typename _Category, typename _Tp, typename _Distance = ptrdiff_t,
         typename _Pointer = _Tp*, typename _Reference = _Tp&>
struct _GLIBCXX17_DEPRECATED iterator {
    typedef _Category  iterator_category;  // 迭代器类别
    typedef _Tp        value_type;         // 值类型
    typedef _Distance  difference_type;    // 距离类型
    typedef _Pointer   pointer;            // 指针类型
    typedef _Reference reference;          // 引用类型
};
```

### 2.2 GCC (libstdc++) 的实现

```cpp
// GCC 的迭代器标签
struct input_iterator_tag {};
struct output_iterator_tag {};
struct forward_iterator_tag : public input_iterator_tag {};
struct bidirectional_iterator_tag : public forward_iterator_tag {};
struct random_access_iterator_tag : public bidirectional_iterator_tag {};
```

---

## 三、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ iterator_traits        │ 完整                 │ 完整                 │
│ iterator_category      │ 完整                 │ 完整                 │
│ reverse_iterator       │ 完整                 │ 完整                 │
│ move_iterator          │ 完整                 │ 完整                 │
│ common_iterator        │ C++20                │ C++20                │
│ counted_iterator       │ C++20                │ C++20                │
│ unreachable_sentinel   │ C++20                │ C++20                │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 四、最佳实践

```
迭代器使用指南：

1. 优先使用范围 for：
   for (auto& x : container) { ... }

2. 使用 auto 推导迭代器类型：
   auto it = container.begin();

3. 使用 C++20 ranges：
   auto result = container | views::filter(pred);

4. 自定义迭代器时：
   · 继承 std::iterator
   · 定义所有必要类型
   · 保证迭代器有效性
```

---

## 延伸阅读

- [Ranges 框架](/internals/algorithms/ranges) — C++20 ranges 的实现
- [std::vector 实现](/internals/containers/vector) — vector 迭代器的实现
- [std::list 实现](/internals/containers/list) — list 迭代器的实现
