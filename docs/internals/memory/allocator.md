---
title: "分配器模型实现分析"
topic: internals
feature: allocator
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/bits/allocator.h"
source_llvm: "references/impl/llvm-project/libcxx/include/__memory/allocator.h"
---

# 分配器模型实现分析

> C++ 的分配器（Allocator）是容器与内存管理之间的抽象层。标准分配器 `std::allocator` 看似简单，但其设计影响了整个标准库的内存管理策略。本文基于 GCC (libstdc++) 和 LLVM (libc++) 的源码，深入分析分配器模型的实现。

---

## 一、分配器的基本概念

### 1.1 什么是分配器

分配器是容器与内存分配之间的抽象层：

```
容器 → allocator → 内存分配器 → 操作系统

例如 vector<int>：
  vector<int> v;
  v.push_back(42);
  
  // 内部调用链：
  // 1. vector 调用 allocator_traits<Alloc>::allocate()
  // 2. allocator_traits 调用 Alloc::allocate()
  // 3. 默认 allocator 调用 ::operator new()
  // 4. operator new 调用 malloc()
```

### 1.2 分配器的接口要求

```cpp
// 分配器必须满足的要求：
template <typename T>
class allocator {
public:
    using value_type = T;
    
    // 分配内存
    T* allocate(size_t n);
    
    // 释放内存
    void deallocate(T* p, size_t n);
    
    // 其他可选操作...
};
```

---

## 二、GCC (libstdc++) 的实现

### 2.1 std::allocator 实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/allocator.h

template<typename _Tp>
class allocator {
public:
    typedef _Tp                 value_type;
    typedef size_t              size_type;
    typedef ptrdiff_t           difference_type;

    // C++20: propagate_on_container_move_assignment
    using propagate_on_container_move_assignment = true_type;
    
    // C++23: is_always_equal
    using is_always_equal = true_type;

    allocator() _GLIBCXX_NOEXCEPT { }

    template<typename _Tp1>
    allocator(const allocator<_Tp1>&) _GLIBCXX_NOEXCEPT { }

    // 分配内存
    __attribute__((__malloc__))
    _Tp* allocate(size_type __n, const void* = nullptr) {
        if (__n > this->max_size())
            std::__throw_bad_array_new_length();
        return static_cast<_Tp*>(::operator new(__n * sizeof(_Tp)));
    }

    // 释放内存
    void deallocate(_Tp* __p, size_type) _GLIBCXX_NOEXCEPT
    { ::operator delete(__p); }
};
```

### 2.2 allocator_traits（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/alloc_traits.h

template<typename _Alloc>
struct allocator_traits {
    // 如果 allocator 没有定义这些类型，使用默认值
    using value_type = typename _Alloc::value_type;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using size_type = typename _Alloc::size_type;
    using difference_type = typename _Alloc::difference_type;
    
    // 分配内存
    static pointer allocate(_Alloc& __a, size_type __n) {
        return __a.allocate(__n);
    }
    
    // 释放内存
    static void deallocate(_Alloc& __a, pointer __p, size_type __n) {
        __a.deallocate(__p, __n);
    }
    
    // 构造对象（C++20 前）
    template<typename _Ptr, typename... _Args>
    static void construct(_Alloc& __a, _Ptr __p, _Args&&... __args) {
        ::new ((void*)__p) typename pointer_traits<_Ptr>::element_type(
            std::forward<_Args>(__args)...);
    }
    
    // 析构对象（C++20 前）
    template<typename _Ptr>
    static void destroy(_Alloc& __a, _Ptr __p) {
        __p->~element_type();
    }
    
    // 最大可分配大小
    static size_type max_size(const _Alloc& __a) noexcept {
        return __a.max_size();
    }
    
    // 获取 allocator（用于传播）
    static _Alloc& select_container_allocator(_Alloc& __a) noexcept {
        return __a;
    }
};
```

### 2.2 allocator_traits

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/alloc_traits.h

template<typename _Alloc>
struct allocator_traits {
    // 如果 allocator 没有定义这些类型，使用默认值
    using value_type = typename _Alloc::value_type;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using size_type = typename _Alloc::size_type;
    using difference_type = typename _Alloc::difference_type;
    
    // 分配内存
    static pointer allocate(_Alloc& __a, size_type __n) {
        return __a.allocate(__n);
    }
    
    // 释放内存
    static void deallocate(_Alloc& __a, pointer __p, size_type __n) {
        __a.deallocate(__p, __n);
    }
    
    // 构造对象
    template<typename _Ptr, typename... _Args>
    static void construct(_Alloc& __a, _Ptr __p, _Args&&... __args) {
        __a.construct(__p, std::forward<_Args>(__args)...);
    }
    
    // 析构对象
    template<typename _Ptr>
    static void destroy(_Alloc& __a, _Ptr __p) {
        __a.destroy(__p);
    }
};
```

---

## 三、LLVM (libc++) 的实现

### 3.1 std::allocator 实现

```cpp
// 源码路径：references/impl/llvm-project/libcxx/include/__memory/allocator.h

template <class _Tp>
class allocator {
public:
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef _Tp value_type;
    typedef true_type propagate_on_container_move_assignment;
    typedef true_type is_always_equal;

    _LIBCPP_CONSTEXPR_SINCE_CXX20 allocator() _NOEXCEPT = default;

    template <class _Up>
    _LIBCPP_CONSTEXPR_SINCE_CXX20 allocator(const allocator<_Up>&) _NOEXCEPT {}

    // 分配内存
    [[__nodiscard__]] _LIBCPP_CONSTEXPR_SINCE_CXX20 _Tp* allocate(size_t __n) {
        static_assert(sizeof(_Tp) >= 0, "cannot allocate memory for an incomplete type");
        if (__n > allocator_traits<allocator>::max_size(*this))
            std::__throw_bad_array_new_length();
        if (__libcpp_is_constant_evaluated()) {
            return static_cast<_Tp*>(::operator new(__n * sizeof(_Tp)));
        } else {
            return std::__libcpp_allocate<_Tp>(__element_count(__n));
        }
    }

    // C++23: allocate_at_least
    [[nodiscard]] _LIBCPP_CONSTEXPR_SINCE_CXX20 allocation_result<_Tp*>
    allocate_at_least(size_t __n) {
        static_assert(sizeof(_Tp) >= 0, "cannot allocate memory for an incomplete type");
        return {allocate(__n), __n};
    }

    // 释放内存
    _LIBCPP_CONSTEXPR_SINCE_CXX20 void deallocate(_Tp* __p, size_t __n) _NOEXCEPT {
        std::__libcpp_deallocate((void*)__p, __n * sizeof(_Tp), _LIBCPP_ALIGNOF(_Tp));
    }
};
```

### 3.2 constexpr 支持

LLVM 的 allocator 支持 constexpr 上下文：

```cpp
// 在 constexpr 上下文中使用 operator new
if (__libcpp_is_constant_evaluated()) {
    return static_cast<_Tp*>(::operator new(__n * sizeof(_Tp)));
} else {
    // 运行时使用自定义分配路径
    return std::__libcpp_allocate<_Tp>(__element_count(__n));
}
```

---

## 四、EBO（空基类优化）

### 4.1 什么是 EBO

空基类优化（Empty Base Optimization）允许空基类不占用额外空间：

```cpp
// 没有 EBO：
struct Container {
    Allocator alloc_;  // 1 字节（空类的最小大小）
    T* data_;
    size_t size_;
};
// sizeof(Container) = 24 字节（8 + 8 + 8，但 allocator 占 1 字节 + 7 字节填充）

// 有 EBO：
struct Container : Allocator {  // 继承空 allocator
    T* data_;
    size_t size_;
};
// sizeof(Container) = 16 字节（allocator 不占空间）
```

### 4.2 GCC 的 EBO 实现

```cpp
// GCC 使用继承实现 EBO
struct _Vector_impl
    : public _Tp_alloc_type,  // 继承 allocator（EBO）
      public _Vector_impl_data
{ };

// 对于空 allocator：
// sizeof(_Vector_impl) == sizeof(_Vector_impl_data)
// allocator 不占空间
```

### 4.3 LLVM 的 EBO 实现

```cpp
// LLVM 使用 compressed_pair 实现 EBO
compressed_pair<allocator_type, pointer> __alloc_;

// compressed_pair 内部使用 EBO：
// 空 allocator 不占空间
// sizeof(compressed_pair<empty, T>) == sizeof(T)
```

---

## 五、分配器的传播

### 5.1 容器操作中的分配器传播

```
容器操作中的分配器传播规则：

1. 移动构造：
   · propagate_on_container_move_assignment = true
   · 移动分配器（不移动数据）

2. 拷贝构造：
   · 分配器可能传播（取决于 allocator_traits）
   · 默认使用分配器的拷贝构造

3. 交换：
   · 如果 propagate_on_container_swap = true
   · 交换分配器
   · 否则行为未定义

4. 赋值：
   · 如果 propagate_on_container_move_assignment = true
   · 移动分配器
   · 否则使用旧分配器
```

### 5.2 GCC 的传播实现

```cpp
// GCC 的 allocator traits
template<typename _Alloc>
struct allocator_traits {
    // 默认传播规则
    using propagate_on_container_move_assignment = false_type;
    using propagate_on_container_copy_assignment = false_type;
    using propagate_on_container_swap = false_type;
    
    // std::allocator 的特化
    template<typename _Tp>
    struct allocator_traits<allocator<_Tp>> {
        using propagate_on_container_move_assignment = true_type;
        using is_always_equal = true_type;
    };
};
```

### 5.3 LLVM 的传播实现

```cpp
// LLVM 的 allocator traits
template <class _Alloc>
struct allocator_traits {
    // 默认传播规则
    typedef false_type propagate_on_container_move_assignment;
    typedef false_type propagate_on_container_copy_assignment;
    typedef false_type propagate_on_container_swap;
};

// std::allocator 的特化
template <>
class allocator {
public:
    typedef true_type propagate_on_container_move_assignment;
    typedef true_type is_always_equal;
};
```

---

## 六、PMR（Polymorphic Memory Resource）

### 6.1 什么是 PMR

PMR 是 C++17 引入的多态内存资源：

```cpp
// PMR 分配器
template<typename T>
class polymorphic_allocator {
    memory_resource* resource_;
    
public:
    T* allocate(size_t n) {
        return static_cast<T*>(resource_->allocate(n * sizeof(T), alignof(T)));
    }
    
    void deallocate(T* p, size_t n) {
        resource_->deallocate(p, n * sizeof(T), alignof(T));
    }
};

// 内存资源基类
class memory_resource {
public:
    virtual void* allocate(size_t bytes, size_t alignment) = 0;
    virtual void deallocate(void* p, size_t bytes, size_t alignment) = 0;
    virtual bool do_is_equal(const memory_resource& other) const noexcept = 0;
};
```

### 6.2 PMR 的优势

```
PMR 的优势：

1. 运行时多态：
   · 可以在运行时切换内存分配策略
   · 不需要模板参数

2. 共享内存资源：
   · 多个容器可以共享同一个 memory_resource
   · 减少内存碎片

3. 预定义资源：
   · monotonic_buffer_resource：快速分配，不释放
   · synchronized_pool_resource：线程安全的池分配
   · unsynchronized_pool_resource：单线程池分配
```

---

## 七、GNU 扩展分配器

### 7.1 GCC 的扩展分配器

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/ext/

// 1. pool_allocator：内存池分配
// 适合频繁分配/释放相同大小的对象

// 2. bitmap_allocator：位图分配
// 使用位图管理固定大小的内存块

// 3. mt_allocator：多线程分配
// 为每个线程维护独立的内存池

// 4. malloc_allocator：基于 malloc 的分配
// 直接使用 malloc/free
```

### 7.2 EASTL 分配器

```cpp
// EASTL 使用非模板分配器设计

class allocator {
public:
    void* allocate(size_t n, int flags = 0);
    void* allocate(size_t n, size_t alignment, size_t offset, int flags = 0);
    void deallocate(void* p, size_t n);
};

// 优势：
// · 非模板化，减少代码膨胀
// · 支持对齐分配
// · 支持调试名称
```

---

## 八、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ allocator 实现         │ 简单封装 operator new│ 支持 constexpr       │
│ EBO 实现               │ 继承                 │ compressed_pair      │
│ constexpr 支持         │ C++20                │ C++20                │
│ allocate_at_least      │ C++23                │ C++23                │
│ PMR 支持               │ 完整                 │ 完整                 │
│ 扩展分配器             │ pool/bitmap/mt 等    │ 无                   │
│ trivial_abi            │ 不支持               │ 支持                 │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 九、最佳实践

```
分配器使用指南：

1. 优先使用默认 allocator：
   · std::allocator 是最通用的选择
   · 编译器会优化掉空 allocator 的开销

2. 使用 make_shared/make_unique：
   · 减少内存分配次数
   · 更好的缓存局部性

3. 使用 PMR 实现多态分配：
   · monotonic_buffer 适合临时分配
   · pool_resource 适合频繁分配/释放

4. 自定义分配器时：
   · 实现 allocator_traits 要求的所有类型
   · 考虑 propagate_on_container_* 语义
   · 支持 rebind（C++20 前）

5. 性能优化：
   · 预分配容器空间（reserve）
   · 使用内存池减少碎片
   · 避免不必要的分配/释放
```

---

## 延伸阅读

- [PMR 多态内存资源](/internals/memory/pmr) — PMR 的详细实现
- [GNU 扩展分配器](/internals/memory/ext-allocators) — pool/bitmap/mt 分配器
- [内存操作基础设施](/internals/memory/operations) — uninitialized_copy/fill 等底层操作
- [EASTL 分配器模型](/libraries/eastl/allocator) — EASTL 的非模板分配器设计
