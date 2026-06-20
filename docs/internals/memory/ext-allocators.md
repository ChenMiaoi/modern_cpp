---
title: "GNU 扩展分配器实现分析"
topic: internals
feature: ext-allocators
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/ext/"
source_llvm: "N/A"
---

# GNU 扩展分配器实现分析

> GCC 提供了多种扩展分配器，用于特定场景下的内存管理优化。本文基于 GCC 源码，分析 pool_allocator、bitmap_allocator 等扩展分配器的内部实现。

---

## 一、扩展分配器概览

### 1.1 GCC 扩展分配器列表

```
GCC 扩展分配器：

1. pool_allocator：
   · 基于内存池的分配器
   · 适合频繁分配/释放相同大小的对象

2. bitmap_allocator：
   · 基于位图的分配器
   · 使用位图管理固定大小的内存块

3. mt_allocator：
   · 多线程分配器
   · 为每个线程维护独立的内存池

4. malloc_allocator：
   · 基于 malloc 的分配器
   · 直接使用 malloc/free

5. debug_allocator：
   · 调试分配器
   · 用于检测内存问题
```

### 1.2 pool_allocator 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/ext/pool_allocator.h

template<typename _Tp>
class pool_allocator {
    // 内存池结构
    struct _Pool_block {
        _Pool_block* _M_next;
        char* _M_chunk;
        size_t _M_size;
    };
    
    // 空闲链表
    static _Tp* _M_free_list;
    static _Pool_block* _M_block_list;
    
public:
    _Tp* allocate(size_t __n) {
        if (__n == 1) {
            // 从空闲链表分配
            if (_M_free_list) {
                _Tp* __result = _M_free_list;
                _M_free_list = *reinterpret_cast<_Tp**>(_M_free_list);
                return __result;
            }
            // 分配新块
            return _M_allocate_block();
        }
        return static_cast<_Tp*>(::operator new(__n * sizeof(_Tp)));
    }
    
    void deallocate(_Tp* __p, size_t __n) {
        if (__n == 1) {
            // 归还到空闲链表
            *__reinterpret_cast<_Tp**>(__p) = _M_free_list;
            _M_free_list = __p;
        } else {
            ::operator delete(__p);
        }
    }
};
```

---

## 二、pool_allocator

### 2.1 实现原理

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/ext/pool_allocator.h

template<typename _Tp>
class pool_allocator {
    // 内存池结构
    struct _Pool_block {
        _Pool_block* _M_next;
        char* _M_chunk;
        size_t _M_size;
    };
    
    // 空闲链表
    static _Tp* _M_free_list;
    static _Pool_block* _M_block_list;
    
public:
    _Tp* allocate(size_t __n) {
        if (__n == 1) {
            // 从空闲链表分配
            if (_M_free_list) {
                _Tp* __result = _M_free_list;
                _M_free_list = *reinterpret_cast<_Tp**>(_M_free_list);
                return __result;
            }
            // 分配新块
            return _M_allocate_block();
        }
        return static_cast<_Tp*>(::operator new(__n * sizeof(_Tp)));
    }
    
    void deallocate(_Tp* __p, size_t __n) {
        if (__n == 1) {
            // 归还到空闲链表
            *__reinterpret_cast<_Tp**>(__p) = _M_free_list;
            _M_free_list = __p;
        } else {
            ::operator delete(__p);
        }
    }
};
```

### 2.2 内存布局

```
pool_allocator 的内存布局：

空闲链表：
┌──────────┐    ┌──────────┐    ┌──────────┐
│ 对象 1   │───→│ 对象 2   │───→│ 对象 3   │───→ nullptr
└──────────┘    └──────────┘    └──────────┘

块结构：
┌─────────────────────────────────────┐
│ _Pool_block                         │
│   _M_next → 下一个块                │
│   _M_chunk → 内存块起始地址         │
│   _M_size  → 块大小                 │
├─────────────────────────────────────┤
│ 对象 0 │ 对象 1 │ 对象 2 │ ...     │
└─────────────────────────────────────┘
```

---

## 三、bitmap_allocator

### 3.1 实现原理

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/ext/bitmap_allocator.h

template<typename _Tp>
class bitmap_allocator {
    // 位图管理内存块
    unsigned long* _M_bitmap;
    size_t _M_bitmap_size;
    void* _M_memory_pool;
    size_t _M_pool_size;
    
public:
    _Tp* allocate(size_t __n) {
        // 在位图中查找连续的空闲位
        size_t __offset = _M_find_free_bits(__n);
        if (__offset != ~size_t(0)) {
            // 标记为已使用
            _M_set_bits(__offset, __n);
            // 返回对应的内存地址
            return reinterpret_cast<_Tp*>(
                static_cast<char*>(_M_memory_pool) + __offset * sizeof(_Tp));
        }
        return nullptr;
    }
    
    void deallocate(_Tp* __p, size_t __n) {
        // 计算偏移量
        size_t __offset = (reinterpret_cast<char*>(__p) -
                          static_cast<char*>(_M_memory_pool)) / sizeof(_Tp);
        // 标记为空闲
        _M_clear_bits(__offset, __n);
    }
};
```

---

## 四、mt_allocator

### 4.1 实现原理

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/ext/mt_allocator.h

template<typename _Tp>
class mt_allocator {
    // 每个线程一个内存池
    struct _Thread_pool {
        _Tp* _M_free_list;
        size_t _M_count;
    };
    
    // 线程本地存储
    static __thread _Thread_pool* _M_thread_pools;
    
public:
    _Tp* allocate(size_t __n) {
        _Thread_pool* __pool = _M_get_thread_pool();
        if (__n == 1 && __pool->_M_free_list) {
            _Tp* __result = __pool->_M_free_list;
            __pool->_M_free_list = *reinterpret_cast<_Tp**>(__pool->_M_free_list);
            __pool->_M_count--;
            return __result;
        }
        return static_cast<_Tp*>(::operator new(__n * sizeof(_Tp)));
    }
};
```

---

## 五、malloc_allocator

### 5.1 实现原理

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/ext/malloc_allocator.h

template<typename _Tp>
class malloc_allocator {
public:
    _Tp* allocate(size_t __n) {
        void* __p = std::malloc(__n * sizeof(_Tp));
        if (!__p) std::__throw_bad_alloc();
        return static_cast<_Tp*>(__p);
    }
    
    void deallocate(_Tp* __p, size_t) {
        std::free(__p);
    }
};
```

---

## 六、性能对比

```
扩展分配器性能对比：

pool_allocator：
  · 分配：O(1)（空闲链表）
  · 释放：O(1)（归还链表）
  · 最适合：频繁分配/释放相同大小对象

bitmap_allocator：
  · 分配：O(n)（查找空闲位）
  · 释放：O(1)（清除位）
  · 最适合：批量分配/释放

mt_allocator：
  · 分配：O(1)（线程本地）
  · 释放：O(1)（线程本地）
  · 最适合：多线程环境

malloc_allocator：
  · 分配：O(n)（系统调用）
  · 释放：O(1)（系统调用）
  · 最适合：通用场景
```

---

## 延伸阅读

- [分配器模型](/internals/memory/allocator) — 标准分配器的实现
- [PMR 多态内存资源](/internals/memory/pmr) — PMR 的实现
- [EASTL 分配器模型](/libraries/eastl/allocator) — EASTL 的非模板分配器
