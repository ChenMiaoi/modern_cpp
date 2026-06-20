---
title: "std::weak_ptr 实现分析"
topic: internals
feature: weak-ptr
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/bits/shared_ptr_base.h"
source_llvm: "references/impl/llvm-project/libcxx/include/__memory/shared_ptr.h"
---

# std::weak_ptr 实现分析

> `std::weak_ptr` 是观察 `std::shared_ptr` 管理对象的弱引用，不增加引用计数。本文基于 GCC 和 LLVM 的源码，分析 weak_ptr 的内部实现。

---

## 一、核心概念

### 1.1 什么是 weak_ptr

weak_ptr 是一种不拥有对象的智能指针，观察 shared_ptr 管理的对象：

```cpp
// weak_ptr 的基本使用
shared_ptr<int> sp = make_shared<int>(42);
weak_ptr<int> wp = sp;  // 观察 sp 管理的对象

// 使用前需要 lock
if (auto locked = wp.lock()) {
    cout << *locked << endl;  // 安全访问
} else {
    cout << "对象已销毁" << endl;
}
```

### 1.2 weak_ptr vs shared_ptr

```
weak_ptr vs shared_ptr：

shared_ptr：
  · 拥有对象
  · 增加引用计数
  · 控制对象生命周期

weak_ptr：
  · 观察对象
  · 不增加引用计数
  · 不影响对象生命周期
  · 需要 lock() 才能访问
```

---

## 二、核心数据结构

### 2.1 弱引用计数

weak_ptr 与 shared_ptr 共享控制块，但使用独立的弱引用计数：

```
控制块布局：

┌─────────────────────────────────────┐
│ vptr（虚函数表指针）                 │
├─────────────────────────────────────┤
│ use_count（强引用计数）              │
├─────────────────────────────────────┤
│ weak_count（弱引用计数）             │
├─────────────────────────────────────┤
│ 删除器（可选）                       │
└─────────────────────────────────────┘

引用计数的语义：
  · use_count：有多少个 shared_ptr 拥有对象
  · weak_count：有多少个 weak_ptr 观察对象 + 1（如果 use_count > 0）
```

### 2.2 GCC (libstdc++) 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/shared_ptr_base.h:200

// 弱引用计数操作
void _M_weak_add_ref() noexcept {
    // 弱引用计数可以使用负值（用户不可观察）
    constexpr _Atomic_word __max = -1;
    if (__gnu_cxx::__exchange_and_add_dispatch(&_M_weak_count, 1) == __max)
        [[__unlikely__]] __builtin_trap();  // 溢出保护
}

void _M_weak_release() noexcept {
    // 使用 acq_rel 内存序，确保 dispose() 的结果在 destroy() 中可见
    _GLIBCXX_SYNCHRONIZATION_HAPPENS_BEFORE(&_M_weak_count);
    if (__gnu_cxx::__exchange_and_add_dispatch(&_M_weak_count, -1) == 1) {
        _GLIBCXX_SYNCHRONIZATION_HAPPENS_AFTER(&_M_weak_count);
        if (_Mutex_base<_Lp>::_S_need_barriers) {
            // 需要内存屏障：确保 dispose() 的结果在 destroy() 中可见
            __atomic_thread_fence(__ATOMIC_ACQ_REL);
        }
        _M_destroy();  // 销毁控制块
    }
}

// weak_count 的语义：
//   weak_count = weak_ptr 数量 + (use_count > 0 ? 1 : 0)
//   当 weak_count == 0 时，销毁控制块
```

### 2.3 LLVM (libc++) 的实现（源码分析）

```cpp
// 源码路径：references/impl/llvm-project/libcxx/include/__memory/shared_ptr.h

// LLVM 的弱引用计数操作
void __add_weak() _NOEXCEPT {
    __libcpp_atomic_refcount_increment(__weak_shared_owners_);
}

void __release_weak() _NOEXCEPT {
    if (__libcpp_atomic_refcount_decrement(__weak_shared_owners_) == -1) {
        // weak_count 归零，销毁控制块
        __on_zero_shared_weak();
    }
}
```
```

---

## 三、lock() 的实现

### 3.1 lock 的工作原理

```cpp
// lock 的伪代码
shared_ptr<T> lock() const noexcept {
    if (use_count() == 0) {
        return shared_ptr<T>();  // 返回空指针
    }
    // 增加引用计数（原子操作）
    // 如果在增加过程中 use_count 变为 0，返回空指针
    return shared_ptr<T>(*this);
}
```

### 3.2 竞态条件处理

```
lock() 的竞态条件：

线程 A：                        线程 B：
  sp.reset()                      wp.lock()
    ↓                               ↓
  use_count-- = 1                  use_count == 1?
    ↓                               ↓
  use_count == 0                   增加引用计数
    ↓                               ↓
  调用析构函数                     返回有效的 shared_ptr

关键：原子操作保证 use_count 的一致性
```

---

## 四、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ 弱引用计数             │ _M_weak_count        │ __weak_shared_owners_│
│ lock 实现              │ 原子操作             │ 原子操作             │
│ expired 检查           │ use_count() == 0     │ use_count() == 0     │
│ enable_shared_from_this│ 支持                 │ 支持                 │
│ owner_before           │ 支持                 │ 支持                 │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 五、enable_shared_from_this

### 5.1 什么是 enable_shared_from_this

enable_shared_from_this 允许对象在 shared_ptr 管理下安全地获取自身的 shared_ptr：

```cpp
class MyClass : public enable_shared_from_this<MyClass> {
public:
    shared_ptr<MyClass> get_self() {
        return shared_from_this();  // 安全获取自身的 shared_ptr
    }
};
```

### 5.2 实现原理

```
enable_shared_from_this 的实现：

1. 内部持有一个 weak_ptr
2. 构造函数中将 weak_ptr 绑定到 shared_ptr
3. shared_from_this() 调用 weak_ptr::lock()

内存布局：
┌─────────────────────────────────────┐
│ MyClass 对象                        │
│   ┌─────────────────────────────┐   │
│   │ weak_ptr<MyClass> __weak_this │  ← 内部 weak_ptr
│   └─────────────────────────────┘   │
└─────────────────────────────────────┘
```

---

## 六、最佳实践

```
weak_ptr 使用指南：

1. 观察者模式：
   · 观察者使用 weak_ptr
   · 被观察者使用 shared_ptr
   · 避免循环引用

2. 缓存：
   · 使用 weak_ptr 缓存对象
   · 对象销毁后自动失效

3. 避免悬垂指针：
   · 使用前先 lock()
   · 检查 lock() 是否成功

4. 不要存储 weak_ptr 到栈上对象：
   · weak_ptr 只能观察堆上对象
   · 栈上对象不需要 weak_ptr
```

---

## 延伸阅读

- [std::shared_ptr 实现](/internals/memory/shared-ptr) — shared_ptr 的详细实现
- [std::unique_ptr 实现](/internals/memory/unique-ptr) — 唯一所有权的智能指针
- [分配器模型](/internals/memory/allocator) — allocator 如何与智能指针交互
