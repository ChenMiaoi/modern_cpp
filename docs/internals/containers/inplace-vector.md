---
title: "std::inplace_vector 实现分析"
topic: internals
feature: inplace-vector
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/std/inplace_vector"
source_llvm: "N/A"
---

# std::inplace_vector 实现分析

> `std::inplace_vector` 是 C++26 引入的固定容量 vector，存储在栈上。本文基于 GCC 的源码，分析 inplace_vector 的内部实现。

---

## 一、核心概念

### 1.1 什么是 inplace_vector

inplace_vector 是固定容量的 vector，不使用堆分配：

```cpp
// inplace_vector 的基本使用
inplace_vector<int, 10> v;  // 最多 10 个元素
v.push_back(1);
v.push_back(2);

// 如果超过容量，抛异常
// v.push_back(11);  // 抛 length_error
```

### 1.2 核心数据结构

```
inplace_vector 的内存布局：

inplace_vector<int, 4>：
┌─────────────────────────────────────┐
│ _M_size（当前大小）                  │
├─────────────────────────────────────┤
│ _M_buf[0]                           │
├─────────────────────────────────────┤
│ _M_buf[1]                           │
├─────────────────────────────────────┤
│ _M_buf[2]                           │
├─────────────────────────────────────┤
│ _M_buf[3]                           │
└─────────────────────────────────────┘

全部在栈上
```

### 1.3 GCC (libstdc++) 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/std/inplace_vector

// inplace_vector 的实现
template<typename _Tp, size_t _Nm>
class inplace_vector {
    static_assert(_Nm > 0, "capacity must be > 0");
    
    // 存储空间
    alignas(_Tp) unsigned char _M_buf[_Nm * sizeof(_Tp)];
    size_t _M_size = 0;
    
public:
    // 类型定义
    using value_type = _Tp;
    using size_type = size_t;
    using reference = _Tp&;
    using const_reference = const _Tp&;
    using pointer = _Tp*;
    using const_pointer = const _Tp*;
    
    // 默认构造函数
    constexpr inplace_vector() = default;
    
    // 析构函数
    ~inplace_vector() {
        clear();
    }
    
    // 元素访问
    constexpr reference operator[](size_type __n) noexcept {
        return reinterpret_cast<pointer>(_M_buf)[__n];
    }
    
    constexpr const_reference operator[](size_type __n) const noexcept {
        return reinterpret_cast<const_pointer>(_M_buf)[__n];
    }
    
    // 迭代器
    constexpr pointer begin() noexcept {
        return reinterpret_cast<pointer>(_M_buf);
    }
    
    constexpr pointer end() noexcept {
        return reinterpret_cast<pointer>(_M_buf) + _M_size;
    }
    
    // 容量
    constexpr size_type size() const noexcept { return _M_size; }
    static constexpr size_type capacity() noexcept { return _Nm; }
    constexpr bool empty() const noexcept { return _M_size == 0; }
    constexpr bool full() const noexcept { return _M_size == _Nm; }
    
    // 元素修改
    constexpr reference push_back(const _Tp& __x) {
        if (full()) __throw_length_error("inplace_vector::push_back");
        auto* __p = new (reinterpret_cast<pointer>(_M_buf) + _M_size) _Tp(__x);
        ++_M_size;
        return *__p;
    }
    
    constexpr reference push_back(_Tp&& __x) {
        if (full()) __throw_length_error("inplace_vector::push_back");
        auto* __p = new (reinterpret_cast<pointer>(_M_buf) + _M_size) _Tp(std::move(__x));
        ++_M_size;
        return *__p;
    }
    
    template<typename... _Args>
    constexpr reference emplace_back(_Args&&... __args) {
        if (full()) __throw_length_error("inplace_vector::emplace_back");
        auto* __p = new (reinterpret_cast<pointer>(_M_buf) + _M_size) _Tp(std::forward<_Args>(__args)...);
        ++_M_size;
        return *__p;
    }
    
    // 清空
    constexpr void clear() noexcept {
        for (size_type __i = 0; __i < _M_size; ++__i) {
            reinterpret_cast<pointer>(_M_buf)[__i].~_Tp();
        }
        _M_size = 0;
    }
};
```

---

## 二、核心数据结构

### 2.1 存储布局

```
inplace_vector 的内存布局：

inplace_vector<int, 4>：
┌─────────────────────────────────────┐
│ _M_size（当前大小）                  │
├─────────────────────────────────────┤
│ _M_buf[0]                           │
├─────────────────────────────────────┤
│ _M_buf[1]                           │
├─────────────────────────────────────┤
│ _M_buf[2]                           │
├─────────────────────────────────────┤
│ _M_buf[3]                           │
└─────────────────────────────────────┘

全部在栈上
```

---

## 三、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ inplace_vector         │ 支持                 │ 实验中               │
│ 固定容量               │ 支持                 │ 支持                 │
│ 栈上存储               │ 支持                 │ 支持                 │
│ push_back              │ 支持                 │ 支持                 │
│ emplace_back           │ 支持                 │ 支持                 │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 延伸阅读

- [std::vector 实现](/internals/containers/vector) — 动态容量 vector
- [std::array 实现](/internals/containers/array) — 固定大小数组
