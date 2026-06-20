---
title: "std::unique_ptr 实现分析"
topic: internals
feature: unique-ptr
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/bits/unique_ptr.h"
source_llvm: "references/impl/llvm-project/libcxx/include/__memory/unique_ptr.h"
---

# std::unique_ptr 实现分析

> `std::unique_ptr` 是 C++ 中最轻量的智能指针，通过独占所有权实现零开销抽象。本文基于 GCC 和 LLVM 的源码，分析 unique_ptr 的内部实现。

---

## 一、核心数据结构

### 1.1 GCC (libstdc++) 的实现（源码分析）

GCC 使用 `tuple<pointer, deleter>` 存储指针和删除器：

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/unique_ptr.h:140

template <typename _Tp, typename _Dp>
class __uniq_ptr_impl {
    template <typename _Up, typename _Ep, typename = void>
    struct _Ptr {
        using type = _Up*;
    };

    // 如果删除器有 pointer 类型，使用它；否则使用 _Up*
    template <typename _Up, typename _Ep>
    struct _Ptr<_Up, _Ep, __void_t<typename remove_reference<_Ep>::type::pointer>> {
        using type = typename remove_reference<_Ep>::type::pointer;
    };

public:
    using pointer = typename _Ptr<_Tp, _Dp>::type;
    
    // 删除器约束：必须是函数对象或左值引用，不能是指针或右值引用
    static_assert(!is_rvalue_reference<_Dp>::value,
                  "unique_ptr's deleter type must be a function object type"
                  " or an lvalue reference type");

    __uniq_ptr_impl() = default;
    __uniq_ptr_impl(pointer __p) : _M_t() { _M_ptr() = __p; }
    
    template<typename _Del>
    __uniq_ptr_impl(pointer __p, _Del&& __d)
    : _M_t(__p, std::forward<_Del>(__d)) { }

    // 移动构造：转移所有权
    __uniq_ptr_impl(__uniq_ptr_impl&& __u) noexcept
    : _M_t(std::move(__u._M_t))
    { __u._M_ptr() = nullptr; }

    // 移动赋值
    __uniq_ptr_impl& operator=(__uniq_ptr_impl&& __u) noexcept {
        reset(__u.release());
        _M_deleter() = std::forward<_Dp>(__u._M_deleter());
        return *this;
    }

    pointer&   _M_ptr() noexcept { return std::get<0>(_M_t); }
    pointer    _M_ptr() const noexcept { return std::get<0>(_M_t); }
    _Dp&       _M_deleter() noexcept { return std::get<1>(_M_t); }
    const _Dp& _M_deleter() const noexcept { return std::get<1>(_M_t); }

    // 重置指针：销毁旧对象，设置新指针
    void reset(pointer __p) noexcept {
        const pointer __old_p = _M_ptr();
        _M_ptr() = __p;
        if (__old_p)
            _M_deleter()(__old_p);
    }

    // 释放所有权：返回指针，置空当前指针
    pointer release() noexcept {
        pointer __p = _M_ptr();
        _M_ptr() = nullptr;
        return __p;
    }

    void swap(__uniq_ptr_impl& __rhs) noexcept {
        using std::swap;
        swap(this->_M_ptr(), __rhs._M_ptr());
        swap(this->_M_deleter(), __rhs._M_deleter());
    }

private:
    tuple<pointer, _Dp> _M_t;  // 核心：pointer + deleter
};
```

### 1.2 移动语义控制（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/unique_ptr.h:228

// 根据删除器的可移动性，选择性地启用/禁用移动操作
template <typename _Tp, typename _Dp,
          bool = is_move_constructible<_Dp>::value,
          bool = is_move_assignable<_Dp>::value>
struct __uniq_ptr_data : __uniq_ptr_impl<_Tp, _Dp> {
    using __uniq_ptr_impl<_Tp, _Dp>::__uniq_ptr_impl;
    __uniq_ptr_data(__uniq_ptr_data&&) = default;
    __uniq_ptr_data& operator=(__uniq_ptr_data&&) = default;
};

// 删除器不可移动赋值时，禁用移动赋值
template <typename _Tp, typename _Dp>
struct __uniq_ptr_data<_Tp, _Dp, true, false> : __uniq_ptr_impl<_Tp, _Dp> {
    using __uniq_ptr_impl<_Tp, _Dp>::__uniq_ptr_impl;
    __uniq_ptr_data(__uniq_ptr_data&&) = default;
    __uniq_ptr_data& operator=(__uniq_ptr_data&&) = delete;
};

// 删除器不可移动构造时，禁用移动构造
template <typename _Tp, typename _Dp>
struct __uniq_ptr_data<_Tp, _Dp, false, true> : __uniq_ptr_impl<_Tp, _Dp> {
    using __uniq_ptr_impl<_Tp, _Dp>::__uniq_ptr_impl;
    __uniq_ptr_data(__uniq_ptr_data&&) = delete;
    __uniq_ptr_data& operator=(__uniq_ptr_data&&) = default;
};
```

### 1.2 LLVM (libc++) 的实现

LLVM 使用 `compressed_pair` 存储指针和删除器：

```cpp
// 源码路径：references/impl/llvm-project/libcxx/include/__memory/unique_ptr.h

template <class _Tp, class _Dp>
class unique_ptr {
    compressed_pair<pointer, _Dp> __ptr_;
    
    pointer& __get_pointer() noexcept { return __ptr_.first(); }
    _Dp& __get_deleter() noexcept { return __ptr_.second(); }
};
```

```
LLVM 的 compressed_pair 使用 EBO：

空删除器时：
┌─────────────────────────────────────┐
│ pointer (8 字节)                     │
└─────────────────────────────────────┘
sizeof(unique_ptr<T>) = 8 字节

有状态删除器时：
┌─────────────────────────────────────┐
│ pointer (8 字节)                     │
├─────────────────────────────────────┤
│ deleter (大小取决于删除器)           │
└─────────────────────────────────────┘
```

---

## 二、移动语义

### 2.1 移动构造函数

```cpp
// GCC 的移动构造
__uniq_ptr_impl(__uniq_ptr_impl&& __u) noexcept
: _M_t(std::move(__u._M_t))
{ __u._M_ptr() = nullptr; }  // 置空源指针

// LLVM 的移动构造
unique_ptr(unique_ptr&& __u) noexcept
    : __ptr_(std::__private_constructor_tag{}, __u.__get_pointer(), std::move(__u.__get_deleter())) {
    __u.__get_pointer() = nullptr;
}
```

### 2.2 移动赋值运算符

```cpp
// GCC 的移动赋值
__uniq_ptr_impl& operator=(__uniq_ptr_impl&& __u) noexcept {
    reset(__u.release());                    // 释放旧对象，获取新对象
    _M_deleter() = std::forward<_Dp>(__u._M_deleter());  // 移动删除器
    return *this;
}
```

---

## 三、删除器

### 3.1 默认删除器

```cpp
// GCC 的默认删除器
template<typename _Tp>
struct default_delete {
    constexpr default_delete() noexcept = default;
    
    void operator()(_Tp* __ptr) const {
        static_assert(!is_void<_Tp>::value, "can't delete pointer to incomplete type");
        static_assert(sizeof(_Tp) > 0, "can't delete pointer to incomplete type");
        delete __ptr;
    }
};

// 数组特化
template<typename _Tp>
struct default_delete<_Tp[]> {
    void operator()(_Up* __ptr) const {
        delete [] __ptr;
    }
};
```

### 3.2 自定义删除器

```cpp
// 自定义删除器示例
struct FileDeleter {
    void operator()(FILE* fp) const {
        if (fp) fclose(fp);
    }
};

unique_ptr<FILE, FileDeleter> open_file(const char* name) {
    return unique_ptr<FILE, FileDeleter>(fopen(name, "r"));
}
```

---

## 四、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ 内部存储               │ tuple<ptr, deleter>  │ compressed_pair      │
│ EBO 实现               │ tuple 内置           │ compressed_pair      │
│ constexpr 支持         │ C++23                │ C++23                │
│ 三向比较               │ C++20                │ C++20                │
│ trivially_relocatable  │ 不支持               │ 支持                 │
│ array cookie 检测      │ 无                   │ 有                   │
│ sizeof (默认删除器)    │ 8 字节               │ 8 字节               │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 五、最佳实践

```
unique_ptr 使用指南：

1. 优先使用 make_unique：
   auto p = std::make_unique<T>(args...);

2. 函数返回 unique_ptr：
   unique_ptr<T> create() { return std::make_unique<T>(); }

3. 自定义删除器：
   · 用 lambda 捕获状态
   · 用函数对象
   · 用函数指针（有额外开销）

4. 数组支持：
   unique_ptr<T[]> arr(new T[10]);

5. 不要拷贝 unique_ptr：
   · 使用 std::move 转移所有权
   · 使用引用传递避免拷贝
```

---

## 延伸阅读

- [std::shared_ptr 实现](/internals/memory/shared-ptr) — 共享所有权的智能指针
- [std::weak_ptr 实现](/internals/memory/weak-ptr) — 弱引用的实现
- [分配器模型](/internals/memory/allocator) — allocator 如何与智能指针交互
