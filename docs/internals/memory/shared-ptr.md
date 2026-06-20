---
title: "std::shared_ptr 实现分析"
topic: internals
feature: shared-ptr
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/bits/shared_ptr_base.h"
source_llvm: "references/impl/llvm-project/libcxx/include/__memory/shared_ptr.h"
---

# std::shared_ptr 实现分析

> `std::shared_ptr` 是 C++ 中最复杂的智能指针之一，其实现涉及原子引用计数、控制块、弱引用、删除器等多个组件。本文基于 GCC (libstdc++) 和 LLVM (libc++) 的源码，深入分析 shared_ptr 的内部实现。

---

## 一、核心概念：控制块

### 1.1 什么是控制块

`shared_ptr` 的核心是**控制块**（control block），它管理：
- **强引用计数**（use_count）：有多少个 `shared_ptr` 拥有对象
- **弱引用计数**（weak_count）：有多少个 `weak_ptr` 观察对象
- **删除器**（deleter）：如何销毁对象
- **分配器**（allocator）：如何分配/释放控制块内存

```
shared_ptr 的内存布局：

┌─────────────────────────────────────┐
│ shared_ptr 对象                      │
│   ┌─────────────────────────────┐   │
│   │ ptr ──────────────────────────────→ 堆上对象
│   └─────────────────────────────┘   │
│   ┌─────────────────────────────┐   │
│   │ control_block ──────────────────→ 堆上控制块
│   └─────────────────────────────┘   │
└─────────────────────────────────────┘

控制块布局：
┌─────────────────────────────────────┐
│ vptr（虚函数表指针）                 │  ← 用于多态销毁
├─────────────────────────────────────┤
│ use_count（强引用计数）              │
├─────────────────────────────────────┤
│ weak_count（弱引用计数）             │
├─────────────────────────────────────┤
│ deleter（删除器，可选）              │
├─────────────────────────────────────┤
│ allocator（分配器，可选）            │
└─────────────────────────────────────┘
```

### 1.2 控制块的类型

```
控制块的三种来源：

1. shared_ptr(new T)
   → _Sp_counted_ptr（最简单，无删除器/分配器）

2. shared_ptr(new T, deleter)
   → _Sp_counted_ptr_inplace 或 _Sp_counted_ptr（带删除器）

3. make_shared<T>(args...)
   → _Sp_counted_ptr_inplace（对象和控制块在同一内存块）
```

---

## 二、GCC (libstdc++) 的实现

### 2.1 控制块基类

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/shared_ptr_base.h

template<_Lock_policy _Lp = __default_lock_policy>
class _Sp_counted_base : public _Mutex_base<_Lp>
{
public:
    _Sp_counted_base() noexcept
    : _M_use_count(1), _M_weak_count(1) { }

    virtual ~_Sp_counted_base() noexcept { }

    // 当 use_count 归零时调用，释放对象资源
    virtual void _M_dispose() noexcept = 0;

    // 当 weak_count 归零时调用，释放控制块
    virtual void _M_destroy() noexcept
    { delete this; }

    // 获取删除器指针（用于 dynamic_cast）
    virtual void* _M_get_deleter(const std::type_info&) noexcept = 0;

private:
    _Atomic_word _M_use_count;     // #shared
    _Atomic_word _M_weak_count;    // #weak + (#shared != 0)
};
```

### 2.2 原子引用计数（源码分析）

GCC 使用原子操作实现引用计数：

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/shared_ptr_base.h

// 三种锁策略
enum _Lock_policy {
    _S_single = 0,  // 单线程（无锁）
    _S_mutex  = 1,  // 互斥锁
    _S_atomic = 2   // 原子操作（默认）
};

// 原子操作实现
template<>
inline void _Sp_counted_base<_S_atomic>::_M_add_ref_copy() {
    __atomic_add_fetch(&_M_use_count, 1, __ATOMIC_RELAXED);
}

template<>
inline void _Sp_counted_base<_S_atomic>::_M_release() noexcept {
    if (__atomic_add_fetch(&_M_use_count, -1, __ATOMIC_ACQ_REL) == 0) {
        _M_dispose();  // 释放对象
        _M_weak_release();  // 减少弱引用计数
    }
}

// 双字原子优化（当条件满足时）
template<>
inline void _Sp_counted_base<_S_atomic>::_M_release() noexcept {
    constexpr bool __lock_free = __atomic_always_lock_free(sizeof(long long), 0);
    constexpr bool __double_word = sizeof(long long) == 2 * sizeof(_Atomic_word);
    
    if constexpr (__lock_free && __double_word && __aligned) {
        // 一次原子操作同时更新 use_count 和 weak_count
        auto __both_counts = reinterpret_cast<long long*>(&_M_use_count);
        if (__atomic_load_n(__both_counts, __ATOMIC_ACQUIRE) == __unique_ref) {
            // 两个计数都是 1，直接销毁
            _M_weak_count = _M_use_count = 0;
            _M_dispose();
            _M_destroy();
            return;
        }
    }
    // 回退到普通路径
    if (__atomic_add_fetch(&_M_use_count, -1, __ATOMIC_ACQ_REL) == 0) {
        _M_release_last_use();
    }
}
```

### 2.3 控制块的优化

GCC 使用**双字原子操作**优化引用计数：

```cpp
// 当 sizeof(long long) == 2 * sizeof(_Atomic_word) 时
// 可以用一次原子操作同时更新 use_count 和 weak_count

if constexpr (__lock_free && __double_word && __aligned) {
    auto __both_counts = reinterpret_cast<long long*>(&_M_use_count);
    
    // 检查是否是最后一个共享引用（use_count=1, weak_count=1）
    if (__atomic_load_n(__both_counts, __ATOMIC_ACQUIRE) == __unique_ref) {
        // 一次性将两个计数都设为 0
        _M_weak_count = _M_use_count = 0;
        _M_dispose();
        _M_destroy();
        return;
    }
}
```

---

## 三、LLVM (libc++) 的实现

### 3.1 控制块结构

LLVM 使用不同的控制块类层次：

```cpp
// 源码路径：references/impl/llvm-project/libcxx/include/__memory/shared_ptr.h

// shared_ptr(new T) 使用的控制块
template <class _Tp, class _Dp, class _Alloc>
class __shared_ptr_pointer : public __shared_weak_count {
    _LIBCPP_COMPRESSED_TRIPLE(_Tp, __ptr_, _Dp, __deleter_, _Alloc, __alloc_);

public:
    __shared_ptr_pointer(_Tp __p, _Dp __d, _Alloc __a)
        : __ptr_(__p), __deleter_(std::move(__d)), __alloc_(std::move(__a)) {}

private:
    void __on_zero_shared() _NOEXCEPT override {
        __deleter_(__ptr_);      // 调用删除器
        __deleter_.~_Dp();       // 销毁删除器
    }

    void __on_zero_shared_weak() _NOEXCEPT override {
        // 释放控制块内存
        _Al __a(__alloc_);
        __alloc_.~_Alloc();
        __a.deallocate(_PTraits::pointer_to(*this), 1);
    }
};
```

### 3.2 make_shared 的优化

LLVM 的 `__shared_ptr_emplace` 将对象和控制块放在同一内存块：

```cpp
// 源码路径：references/impl/llvm-project/libcxx/include/__memory/shared_ptr.h

template <class _Tp, class _Alloc>
struct __shared_ptr_emplace : __shared_weak_count {
    struct _Storage {
        struct _Data {
            _LIBCPP_COMPRESSED_PAIR(_Alloc, __alloc_, __value_type, __elem_);
        };
        _ALIGNAS_TYPE(_Data) char __buffer_[sizeof(_Data)];
        
        _Alloc* __get_alloc() _NOEXCEPT {
            return std::addressof(reinterpret_cast<_Data*>(__buffer_)->__alloc_);
        }
        
        __value_type* __get_elem() _NOEXCEPT {
            return std::addressof(reinterpret_cast<_Data*>(__buffer_)->__elem_);
        }
    };

    _Storage __storage_;
};
```

```
make_shared 的内存布局：

普通 shared_ptr(new T)：
┌──────────────────┐      ┌──────────────────┐
│ 控制块            │ ───→ │ T 对象            │
│ (引用计数+删除器) │      │                   │
└──────────────────┘      └──────────────────┘
两次分配：控制块 + 对象

make_shared<T>(args...)：
┌────────────────────────────────────────────┐
│ 控制块 + T 对象（在同一内存块）              │
│ ┌──────────────┬──────────────────────────┐│
│ │ 引用计数      │ T 对象                   ││
│ └──────────────┴──────────────────────────┘│
└────────────────────────────────────────────┘
一次分配：控制块 + 对象合并
```

---

## 四、引用计数的生命周期

### 4.1 共享引用的生命周期

```
shared_ptr 引用计数变化：

shared_ptr<A> sp1(new A);  // use_count = 1
    │
    ▼
shared_ptr<A> sp2 = sp1;   // use_count = 2
    │
    ▼
sp1.reset();                // use_count = 1
    │
    ▼
sp2.reset();                // use_count = 0 → 调用 delete
```

### 4.2 弱引用的生命周期

```
weak_ptr 引用计数变化：

shared_ptr<A> sp(new A);   // use_count = 1, weak_count = 1
    │
    ▼
weak_ptr<A> wp = sp;       // use_count = 1, weak_count = 2
    │
    ▼
sp.reset();                 // use_count = 0 → 对象被销毁
                            // weak_count = 1（控制块仍存在）
    │
    ▼
wp.lock();                  // 返回 nullptr（对象已销毁）
    │
    ▼
wp.reset();                 // weak_count = 0 → 控制块被销毁
```

---

## 五、线程安全

### 5.1 shared_ptr 的线程安全保证

```
shared_ptr 的线程安全模型：

1. 同一个 shared_ptr 对象：
   · 多个线程同时读取 → 安全
   · 一个线程读，一个线程写 → 数据竞争（UB）
   · 多个线程同时写 → 数据竞争（UB）

2. 不同的 shared_ptr 对象（指向同一对象）：
   · 多个线程同时读引用计数 → 安全（原子操作）
   · 一个线程拷贝/销毁，另一个线程读引用计数 → 安全

3. 对象本身的访问：
   · 不保证线程安全
   · 需要用户自己加锁
```

### 5.2 GCC 的原子操作实现

```cpp
// GCC 使用三种原子操作策略：

// 1. 单线程模式（_S_single）
// 无原子操作，适合单线程环境

// 2. 互斥锁模式（_S_mutex）
// 使用 mutex 保护，适合不支持原子操作的平台

// 3. 原子操作模式（_S_atomic，默认）
// 使用 __atomic_* 内建函数
template<>
inline void _Sp_counted_base<_S_atomic>::_M_add_ref_copy() {
    __atomic_add_fetch(&_M_use_count, 1, __ATOMIC_RELAXED);
}
```

---

## 六、make_shared vs shared_ptr(new T)

### 6.1 性能对比

```
性能对比：

shared_ptr<T>(new T)：
  · 两次内存分配（控制块 + 对象）
  · 两次指针解引用（控制块 → 对象）
  · 更多缓存未命中

make_shared<T>(args...)：
  · 一次内存分配（控制块 + 对象合并）
  · 更好的缓存局部性
  · 更小的内存开销

make_shared 的优势：
  · 内存分配次数：1 vs 2
  · 内存碎片：更少
  · 缓存友好性：更好
  · 控制块大小：更小（无额外指针）
```

### 6.2 何时不能使用 make_shared

```
不能使用 make_shared 的情况：

1. 需要自定义删除器：
   shared_ptr<T> sp(new T, custom_deleter);

2. 需要使用自定义分配器：
   shared_ptr<T> sp(allocator_arg, alloc, new T);

3. 需要从原始指针创建（已有对象）：
   T* raw = get_existing_object();
   shared_ptr<T> sp(raw);  // 不能用 make_shared

4. 对象大小超过 make_shared 的限制：
   // 某些实现对 make_shared 的对象大小有限制
```

---

## 七、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ 控制块基类             │ _Sp_counted_base     │ __shared_weak_count  │
│ 锁策略                 │ 模板参数 _Lp         │ 内置原子操作         │
│ 双字原子优化           │ 有（当条件满足时）   │ 无                   │
│ compressed_pair        │ 无                   │ 有（存储指针+删除器）│
│ make_shared 实现       │ _Sp_counted_ptr_     │ __shared_ptr_emplace │
│                        │ inplace              │                      │
│ allocator 支持         │ 完整支持             │ 完整支持             │
│ trivial_abi            │ 不支持               │ 支持（_LIBCPP_ABI_  │
│                        │                      │ ENABLE_SHARED_PTR_   │
│                        │                      │ TRIVIAL_ABI）        │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 八、最佳实践

```
shared_ptr 使用指南：

1. 优先使用 make_shared：
   auto sp = std::make_shared<T>(args...);

2. 避免循环引用：
   · 使用 weak_ptr 打破循环
   · 观察者模式用 weak_ptr

3. 注意线程安全：
   · 不要跨线程修改同一个 shared_ptr
   · 使用 atomic<shared_ptr> 或加锁

4. 注意性能：
   · shared_ptr 有额外开销（控制块 + 原子操作）
   · 能用 unique_ptr 就不要用 shared_ptr

5. 注意删除器：
   · 自定义删除器会影响控制块大小
   · make_shared 不支持自定义删除器
```

---

## 延伸阅读

- [std::unique_ptr 实现](/internals/memory/unique-ptr) — 更轻量的智能指针
- [std::weak_ptr 实现](/internals/memory/weak-ptr) — 弱引用的实现
- [分配器模型](/internals/memory/allocator) — allocator 如何与智能指针交互
- [智能指针对比](/internals/comparison/smart-pointers) — libstdc++ vs libc++ 的差异
