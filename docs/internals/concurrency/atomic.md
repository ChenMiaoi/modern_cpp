---
title: "std::atomic 实现分析"
topic: internals
feature: atomic
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/bits/atomic_base.h"
source_llvm: "references/impl/llvm-project/libcxx/include/__atomic/atomic.h"
---

# std::atomic 实现分析

> `std::atomic` 是 C++11 引入的原子操作类型，提供无锁的线程安全操作。本文基于 GCC 和 LLVM 的源码，分析 atomic 的内部实现。

---

## 一、核心概念

### 1.1 什么是原子操作

原子操作是不可分割的操作，要么完全执行，要么完全不执行，不会出现中间状态：

```
原子操作 vs 非原子操作：

非原子操作（可能被中断）：
  读取 → 修改 → 写入
  如果线程在"修改"步骤被中断，其他线程可能看到中间状态

原子操作（不可分割）：
  读取 + 修改 + 写入（作为一个整体）
  其他线程要么看到操作前的状态，要么看到操作后的状态
```

### 1.2 memory_order

C++11 定义了 6 种内存序：

```
memory_order_relaxed：只保证原子性，不保证顺序
memory_order_consume：（C++17 已弃用）
memory_order_acquire：后续读写不能重排到此操作之前
memory_order_release：之前的读写不能重排到此操作之后
memory_order_acq_rel：同时具有 acquire 和 release 语义
memory_order_seq_cst：全局顺序一致（最严格）
```

---

## 二、核心数据结构

### 2.1 GCC (libstdc++) 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/atomic_base.h

template<typename _IntTp>
struct __atomic_base {
    _Atomic_word _M_i;  // 原子整数存储
    
    // 原子加载（读取）
    _GLIBCXX_ALWAYS_INLINE _IntTp
    load(memory_order __m = memory_order_seq_cst) const noexcept {
        return __atomic_load_n(&_M_i, int(__m));
    }
    
    // 原子存储（写入）
    _GLIBCXX_ALWAYS_INLINE void
    store(_IntTp __i, memory_order __m = memory_order_seq_cst) noexcept {
        __atomic_store_n(&_M_i, __i, int(__m));
    }
    
    // 原子交换（读取旧值并写入新值）
    _GLIBCXX_ALWAYS_INLINE _IntTp
    exchange(_IntTp __i, memory_order __m = memory_order_seq_cst) noexcept {
        return __atomic_exchange_n(&_M_i, __i, int(__m));
    }
    
    // 原子比较交换（强版本）
    // 如果 *this == __i1，则 *this = __i2，返回 true
    // 否则 __i1 = *this，返回 false
    _GLIBCXX_ALWAYS_INLINE bool
    compare_exchange_strong(_IntTp& __i1, _IntTp __i2,
                            memory_order __m1 = memory_order_seq_cst,
                            memory_order __m2 = memory_order_seq_cst) noexcept {
        return __atomic_compare_exchange_n(&_M_i, &__i1, __i2, false,
                                           int(__m1), int(__m2));
    }
    
    // 原子比较交换（弱版本）
    // 可能会虚假失败（spurious failure），但更高效
    _GLIBCXX_ALWAYS_INLINE bool
    compare_exchange_weak(_IntTp& __i1, _IntTp __i2,
                          memory_order __m1 = memory_order_seq_cst,
                          memory_order __m2 = memory_order_seq_cst) noexcept {
        return __atomic_compare_exchange_n(&_M_i, &__i1, __i2, true,
                                           int(__m1), int(__m2));
    }
    
    // 原子加法
    _GLIBCXX_ALWAYS_INLINE _IntTp
    fetch_add(_IntTp __i, memory_order __m = memory_order_seq_cst) noexcept {
        return __atomic_fetch_add(&_M_i, __i, int(__m));
    }
    
    // 原子减法
    _GLIBCXX_ALWAYS_INLINE _IntTp
    fetch_sub(_IntTp __i, memory_order __m = memory_order_seq_cst) noexcept {
        return __atomic_fetch_sub(&_M_i, __i, int(__m));
    }
    
    // 原子按位与
    _GLIBCXX_ALWAYS_INLINE _IntTp
    fetch_and(_IntTp __i, memory_order __m = memory_order_seq_cst) noexcept {
        return __atomic_fetch_and(&_M_i, __i, int(__m));
    }
    
    // 原子按位或
    _GLIBCXX_ALWAYS_INLINE _IntTp
    fetch_or(_IntTp __i, memory_order __m = memory_order_seq_cst) noexcept {
        return __atomic_fetch_or(&_M_i, __i, int(__m));
    }
    
    // 原子按位异或
    _GLIBCXX_ALWAYS_INLINE _IntTp
    fetch_xor(_IntTp __i, memory_order __m = memory_order_seq_cst) noexcept {
        return __atomic_fetch_xor(&_M_i, __i, int(__m));
    }
    
    // 前缀递增
    _GLIBCXX_ALWAYS_INLINE _IntTp
    operator++() noexcept { return fetch_add(1) + 1; }
    
    // 后缀递增
    _GLIBCXX_ALWAYS_INLINE _IntTp
    operator++(int) noexcept { return fetch_add(1); }
    
    // 前缀递减
    _GLIBCXX_ALWAYS_INLINE _IntTp
    operator--() noexcept { return fetch_sub(1) - 1; }
    
    // 后缀递减
    _GLIBCXX_ALWAYS_INLINE _IntTp
    operator--(int) noexcept { return fetch_sub(1); }
};
```

### 2.2 atomic_flag 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/atomic_base.h

struct atomic_flag {
    __atomic_flag_data_type _M_i;
    
    // 测试并设置（test-and-set）
    _GLIBCXX_ALWAYS_INLINE bool
    test_and_set(memory_order __m = memory_order_seq_cst) noexcept {
        return __atomic_test_and_set(&_M_i, int(__m));
    }
    
    // 清除
    _GLIBCXX_ALWAYS_INLINE void
    clear(memory_order __m = memory_order_seq_cst) noexcept {
        __atomic_clear(&_M_i, int(__m));
    }
    
    // C++20: 等待
    _GLIBCXX_ALWAYS_INLINE void
    wait(bool __old, memory_order __m = memory_order_seq_cst) const noexcept {
        const __atomic_flag_data_type __v
            = __old ? __GCC_ATOMIC_TEST_AND_SET_TRUEVAL : 0;
        std::__atomic_wait_address_v(&_M_i, __v,
            [__m, this] { return __atomic_load_n(&_M_i, int(__m)); });
    }
    
    // C++20: 通知一个等待线程
    _GLIBCXX_ALWAYS_INLINE void
    notify_one() noexcept
    { std::__atomic_notify_address(&_M_i, false); }
    
    // C++20: 通知所有等待线程
    _GLIBCXX_ALWAYS_INLINE void
    notify_all() noexcept
    { std::__atomic_notify_address(&_M_i, true); }
};
```

### 2.2 LLVM (libc++) 的实现

```cpp
// 源码路径：references/impl/llvm-project/libcxx/include/__atomic/atomic.h

template <class _Tp>
class atomic {
    _Tp __a_value_;
    
public:
    // 原子加载
    _LIBCPP_HIDE_FROM_ABI _Tp load(memory_order __m = memory_order_seq_cst) const volatile noexcept {
        return __atomic_load(&__a_value_, __m);
    }
    
    // 原子存储
    _LIBCPP_HIDE_FROM_ABI void store(_Tp __d, memory_order __m = memory_order_seq_cst) volatile noexcept {
        __atomic_store(&__a_value_, __d, __m);
    }
    
    // 原子交换
    _LIBCPP_HIDE_FROM_ABI _Tp exchange(_Tp __d, memory_order __m = memory_order_seq_cst) volatile noexcept {
        return __atomic_exchange(&__a_value_, __d, __m);
    }
    
    // 原子比较交换
    _LIBCPP_HIDE_FROM_ABI bool compare_exchange_strong(_Tp& __e, _Tp __d,
                                                        memory_order __s = memory_order_seq_cst,
                                                        memory_order __f = memory_order_seq_cst) volatile noexcept {
        return __atomic_compare_exchange(&__a_value_, &__e, __d, false, __s, __f);
    }
};
```

---

## 三、lock-free 实现

### 3.1 什么是 lock-free

lock-free 是一种无锁编程技术，保证系统整体不会因为某个线程的暂停而停止前进：

```
lock-free 的保证：

1. 至少有一个线程在执行
   · 不会出现所有线程都被挂起的情况

2. 有限步内完成
   · 操作会在有限步内完成

3. 不使用互斥锁
   · 使用原子操作代替锁
   · 更好的性能和可扩展性
```

### 3.2 GCC 的 lock-free 实现

```cpp
// GCC 使用原子内建函数实现 lock-free

// x86 平台的原子操作
__atomic_load_n(&_M_i, __ATOMIC_SEQ_CST)
__atomic_store_n(&_M_i, __i, __ATOMIC_SEQ_CST)
__atomic_exchange_n(&_M_i, __i, __ATOMIC_SEQ_CST)
__atomic_compare_exchange_n(&_M_i, &__i1, __i2, false,
                            __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
```

### 3.3 LLVM 的 lock-free 实现

LLVM 使用类似的原子内建函数：

```cpp
// LLVM 使用 __atomic_* 内建函数
__atomic_load(&__a_value_, __m)
__atomic_store(&__a_value_, __d, __m)
__atomic_exchange(&__a_value_, __d, __m)
__atomic_compare_exchange(&__a_value_, &__e, __d, false, __s, __f)
```

---

## 四、memory_order 的实现

### 4.1 内存屏障

内存屏障（Memory Barrier）用于保证内存操作的顺序：

```
内存屏障的作用：

1. acquire 屏障（load）
   · 后续的读写操作不能重排到此操作之前
   · 用于同步获取数据

2. release 屏障（store）
   · 之前的读写操作不能重排到此操作之后
   · 用于同步发布数据

3. acq_rel 屏障（fetch_add）
   · 同时具有 acquire 和 release 语义
   · 用于读-修改-写操作

4. seq_cst 屏障
   · 全局顺序一致
   · 最严格的内存序
```

### 4.2 编译器指令

```cpp
// GCC 使用内建函数指定内存序
__atomic_load_n(&_M_i, __ATOMIC_SEQ_CST)
__atomic_store_n(&_M_i, __i, __ATOMIC_SEQ_CST)
__atomic_exchange_n(&_M_i, __i, __ATOMIC_SEQ_CST)
__atomic_compare_exchange_n(&_M_i, &__i1, __i2, false,
                            __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)

// LLVM 使用类似的方式
__atomic_load(&__a_value_, __m)
__atomic_store(&__a_value_, __d, __m)
__atomic_exchange(&__a_value_, __d, __m)
__atomic_compare_exchange(&__a_value_, &__e, __d, false, __s, __f)
```

---

## 五、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ 原子操作实现           │ __atomic_* 内建函数  │ __atomic_* 内建函数  │
│ 内存序支持             │ 完整                 │ 完整                 │
│ lock-free 支持         │ 平台相关             │ 平台相关             │
│ constexpr 支持         │ C++20                │ C++20                │
│ atomic_ref             │ C++20                │ C++20                │
│ atomic_wait/notify     │ C++20                │ C++20                │
│ floating_point atomic  │ C++20                │ C++20                │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 六、性能特征

```
std::atomic 的性能：

原子操作开销：
  · seq_cst：最慢（需要完整内存屏障）
  · acq_rel：较快（部分屏障）
  · relaxed：最快（无屏障）

典型延迟（x86）：
  · atomic_load (seq_cst): ~5-10 ns
  · atomic_store (seq_cst): ~5-10 ns
  · atomic_exchange: ~10-20 ns
  · atomic_compare_exchange: ~10-20 ns

与 mutex 对比：
  · atomic：无锁，更快
  · mutex：有锁，更安全（但更慢）
  · 能用 atomic 就不要用 mutex
```

---

## 延伸阅读

- [std::thread 实现](/internals/concurrency/thread) — 线程的实现
- [std::mutex 实现](/internals/concurrency/mutex) — 互斥锁的实现
- [内存模型](/topics/memory-model) — C++ 内存模型的详细解释
