---
title: "atomic wait/notify 实现原理"
topic: topics
feature: memory-model-atomic-wait-implementation
standard: C++
status_checked_at: 2026-06-02
---

# atomic wait/notify 实现原理

> C++20 引入了 `std::atomic::wait()` 和 `std::atomic::notify_one/all()`，为原子变量提供了高效的阻塞等待机制。本文深入分析这些接口在 Linux（futex）、Windows（WaitOnAddress）、macOS（ulock）上的实现原理，以及用户态方案如 parking_lot 的设计。

---

## 1. C++20 wait/notify 接口

### 1.1 基本用法

```cpp
#include <atomic>
#include <thread>

std::atomic<int> state{0};

// 等待线程
void waiter() {
    // 阻塞等待，直到 state != 0
    state.wait(0); // 期望值是 0，如果当前值 == 0 则阻塞

    // 或者带超时
    // state.wait(0, std::memory_order_seq_cst, 100ms);

    // 被唤醒后，可以安全地继续执行
    int val = state.load();
    // ...
}

// 通知线程
void notifier() {
    // 做一些工作...
    state.store(1);

    // 唤醒一个等待的线程
    state.notify_one();

    // 或者唤醒所有等待的线程
    // state.notify_all();
}
```

### 1.2 语义

```
state.wait(old):
  1. 原子地读取 state 的当前值
  2. 如果当前值 == old，则阻塞
  3. 被唤醒后返回（可能是 notify、虚假唤醒、或超时）
  4. 如果被唤醒且当前值 != old，则返回
     （自旋检查以避免丢失唤醒）

state.notify_one():
  唤醒至少一个在该原子变量上等待的线程

state.notify_all():
  唤醒所有在该原子变量上等待的线程

注意：
  · wait 可能有虚假唤醒（spurious wakeup）
  · 必须在循环中检查条件
  · 与 POSIX condvar 的语义类似
```

### 1.3 推荐用法模式

```cpp
// 推荐模式：在循环中等待
std::atomic<bool> ready{false};

// 等待方
while (!ready.load(std::memory_order_acquire)) {
    ready.wait(false, std::memory_order_relaxed);
    // wait 返回后重新检查条件
}

// 通知方
ready.store(true, std::memory_order_release);
ready.notify_one();
```

---

## 2. Linux：futex

### 2.1 futex 系统调用

```c
// futex = Fast Userspace muTEX
// Linux 特有的系统调用，用于实现用户态同步原语

#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

// futex 等待
long futex_wait(int* addr, int expected, const struct timespec* timeout) {
    return syscall(SYS_futex, addr, FUTEX_WAIT, expected, timeout, NULL, 0);
}

// futex 唤醒
long futex_wake(int* addr, int count) {
    return syscall(SYS_futex, addr, FUTEX_WAKE, count, NULL, NULL, 0);
}
```

### 2.2 futex 的工作原理

```
futex 的核心思想：用户态快速路径 + 内核态慢速路径

  ┌───────────────────────────────────────────────────────┐
  │ 用户态（快速路径）                                     │
  │   1. 原子地检查 *addr == expected                      │
  │   2. 如果相等，进入内核态等待                          │
  │   3. 如果不相等，直接返回（不需要系统调用）            │
  │                                                        │
  │ 内核态（慢速路径）                                     │
  │   1. 将当前线程加入等待队列                            │
  │   2. 让出 CPU（线程状态变为 TASK_INTERRUPTIBLE）       │
  │   3. 等待被唤醒                                        │
  │                                                        │
  │ futex_wake：                                           │
  │   1. 唤醒等待队列上的线程                              │
  │   2. 被唤醒的线程在用户态重新检查条件                  │
  └───────────────────────────────────────────────────────┘
```

### 2.3 用 futex 实现 atomic::wait

```cpp
// 简化版实现（仅 int 类型）
#include <atomic>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace atomic_wait_impl {

void futex_wait(std::atomic<int>* addr, int expected) {
    // 内核会原子地检查 *addr == expected
    // 如果不相等，futex 直接返回 EAGAIN
    syscall(SYS_futex,
            reinterpret_cast<int*>(addr),
            FUTEX_WAIT | FUTEX_PRIVATE_FLAG,
            expected,
            nullptr,  // no timeout
            nullptr,
            0);
}

void futex_wake_one(std::atomic<int>* addr) {
    syscall(SYS_futex,
            reinterpret_cast<int*>(addr),
            FUTEX_WAKE | FUTEX_PRIVATE_FLAG,
            1,  // 唤醒一个线程
            nullptr,
            nullptr,
            0);
}

void futex_wake_all(std::atomic<int>* addr) {
    syscall(SYS_futex,
            reinterpret_cast<int*>(addr),
            FUTEX_WAKE | FUTEX_PRIVATE_FLAG,
            INT_MAX,  // 唤醒所有线程
            nullptr,
            nullptr,
            0);
}

} // namespace atomic_wait_impl
```

### 2.4 FUTEX_WAIT_BITSET 与 FUTEX_WAKE_BITSET

```cpp
// 位集操作允许更精确的唤醒
// 可以只唤醒等待特定位模式的线程

// 等待特定位模式
syscall(SYS_futex, addr,
        FUTEX_WAIT_BITSET | FUTEX_PRIVATE_FLAG,
        expected, timeout, nullptr,
        bitmask);  // 32 位位掩码

// 唤醒特定位模式的线程
syscall(SYS_futex, addr,
        FUTEX_WAKE_BITSET | FUTEX_PRIVATE_FLAG,
        INT_MAX, nullptr, nullptr,
        bitmask);

// 用途：
// · 实现多种等待类型（读锁等待 vs 写锁等待）
// · 实现优先级唤醒
// · 在 condvar 实现中区分 signal 和 broadcast
```

---

## 3. Windows：WaitOnAddress

### 3.1 API 概述

```cpp
#include <windows.h>

// Windows 8+ 引入的 WaitOnAddress
// 功能上等价于 futex

BOOL WaitOnAddress(
    volatile VOID* Address,        // 等待的地址
    PVOID        CompareAddress,   // 比较的值
    SIZE_T       AddressSize,      // 地址大小（1, 2, 4, 8 字节）
    DWORD        dwMilliseconds    // 超时（INFINITE = 无限等待）
);

VOID WakeByAddressSingle(PVOID Address);  // 唤醒一个线程
VOID WakeByAddressAll(PVOID Address);     // 唤醒所有线程
```

### 3.2 实现原理

```
WaitOnAddress 的内部实现（Windows 内核）：

  ┌───────────────────────────────────────────────────────┐
  │ 1. 用户态快速检查：                                   │
  │    if (memcmp(Address, CompareAddress, Size) != 0)    │
  │        return TRUE;  // 值已改变，无需等待            │
  │                                                       │
  │ 2. 进入内核态（ntdll → ntoskrnl）                    │
  │    · 将线程加入一个基于哈希的等待桶                   │
  │    · 等待桶按 Address 哈希分组                        │
  │    · 线程进入等待状态                                 │
  │                                                       │
  │ 3. WakeByAddressSingle/All：                          │
  │    · 计算 Address 的哈希值                            │
  │    · 唤醒等待桶中的线程                               │
  └───────────────────────────────────────────────────────┘

  与 futex 的区别：
  · 支持 1/2/4/8 字节的任意大小（futex 只支持 int）
  · 没有 FUTEX_PRIVATE_FLAG 概念（Windows 进程模型不同）
  · 超时参数是毫秒（futex 是 timespec）
  · 内部使用不同的哈希策略
```

### 3.3 用 WaitOnAddress 实现 atomic::wait

```cpp
#include <windows.h>

namespace atomic_wait_impl {

void wait(std::atomic<int>* addr, int expected) {
    // WaitOnAddress 会原子地比较 *addr 与 expected
    WaitOnAddress(
        static_cast<volatile VOID*>(addr),
        &expected,
        sizeof(int),
        INFINITE);
}

void notify_one(std::atomic<int>* addr) {
    WakeByAddressSingle(addr);
}

void notify_all(std::atomic<int>* addr) {
    WakeByAddressAll(addr);
}

} // namespace atomic_wait_impl
```

---

## 4. macOS：ulock

### 4.1 ulock API

```cpp
// macOS 使用 ulock 系列系统调用
// 这是 Apple 私有的系统调用，未文档化

#include <sys/ulock.h>

// 等待操作
int __ulock_wait(uint32_t operation, void* addr, uint64_t value,
                 uint32_t timeout);

// 唤醒操作
int __ulock_wake(uint32_t operation, void* addr, uint64_t value);

// operation 类型：
// UL_COMPARE_AND_WAIT      — 比较并等待（类似 futex FUTEX_WAIT）
// UL_COMPARE_AND_WAIT_SHARED — 共享内存版本
// UL_UNFAIR_LOCK           — 不公平锁等待
// UL_UNFAIR_LOCK_WAIT      — 不公平锁等待（带优先级继承）
// UL_COMPARE_AND_WAIT64    — 64 位比较等待
```

### 4.2 实现原理

```
ulock 的实现与 futex 类似：

  · 用户态快速路径：原子比较 + 条件阻塞
  · 内核态慢速路径：线程等待队列
  · 支持优先级继承（priority inheritance）
  · 对 Mach 线程调度器有更好的集成

  macOS 上的其他同步机制：
  · pthread_mutex — 内部可能使用 ulock
  · dispatch_semaphore — GCD 层面的信号量
  · os_unfair_lock — Apple 推荐的低级锁（取代 OSSpinLock）
```

---

## 5. parking_lot：用户态实现

### 5.1 设计理念

```
parking_lot（Rust 生态，也有 C++ 移植）的核心理念：

  1. 全局哈希表将地址映射到等待队列
  2. 所有同步操作在用户态完成（除了线程挂起/唤醒）
  3. 不需要每个同步对象分配单独的内核资源
  4. 比 pthread_mutex/condvar 更小、更快

  ┌───────────────────────────────────────────────────────┐
  │ 全局哈希表（通常 256-1024 个桶）                      │
  │ ┌────────┬────────┬────────┬────────┬───────┐        │
  │ │ Bucket │ Bucket │ Bucket │ Bucket │  ...  │        │
  │ │   0    │   1    │   2    │   3    │       │        │
  │ └────────┴────────┴────────┴────────┴───────┘        │
  │       ↓                                               │
  │ 每个桶包含一个 mutex + 条件变量 + 等待者链表          │
  │                                                       │
  │ addr % NUM_BUCKETS → 找到对应桶                       │
  │ 桶内用链表管理所有在该地址上等待的线程                │
  └───────────────────────────────────────────────────────┘
```

### 5.2 等待流程

```
park(addr, expected):
  1. hash = hash_addr(addr)
  2. lock bucket[hash].mutex
  3. 检查 *addr == expected（在持锁状态下）
     · 如果不相等，解锁并返回
  4. 将当前线程加入 bucket[hash].waiters 链表
  5. 解锁 bucket[hash].mutex
  6. 调用平台原语挂起线程（Linux: futex, Windows: WaitOnAddress）
     或者用条件变量
  7. 被唤醒后从链表中移除自己

unpark_one(addr):
  1. hash = hash_addr(addr)
  2. lock bucket[hash].mutex
  3. 从 bucket[hash].waiters 中取出一个线程
  4. 解锁 bucket[hash].mutex
  5. 唤醒该线程
```

### 5.3 性能优势

```
parking_lot vs 原生同步：

  ┌──────────────────┬──────────────┬──────────────┐
  │                  │ parking_lot  │ pthread      │
  ├──────────────────┼──────────────┼──────────────┤
  │ mutex 大小       │ 1 字节       │ 40-56 字节   │
  │ condvar 大小     │ 0（无独立）  │ 48 字节      │
  │ 锁竞争时         │ 更短的自旋   │ 标准退避     │
  │ 虚假唤醒         │ 可控         │ 可能更多     │
  │ 内存分配         │ 无           │ 可能有       │
  │ 跨平台一致性     │ 高           │ 平台依赖     │
  └──────────────────┴──────────────┴──────────────┘

  关键优势：
  · mutex 只需 1 字节（用 2 位表示状态即可）
  · 无需为每个 mutex/condvar 预分配内核资源
  · 等待队列通过哈希表共享，内存效率高
```

### 5.4 与 futex 的关系

```
parking_lot 和 futex 不是互相替代的关系：

  · futex 是内核原语，提供"用户态快速路径 + 内核态挂起"
  · parking_lot 是用户态库，利用 futex/WaitOnAddress 作为底层挂起机制

  层次关系：
  ┌──────────────────────────────────────────┐
  │ parking_lot (用户态同步库)               │
  ├──────────────────────────────────────────┤
  │ futex / WaitOnAddress / ulock (OS 原语)  │
  ├──────────────────────────────────────────┤
  │ 内核线程调度器                           │
  └──────────────────────────────────────────┘

  parking_lot 可以用以下方式挂起线程：
  · Linux:   futex(FUTEX_WAIT)
  · Windows: WaitOnAddress
  · macOS:   __ulock_wait 或 pthread_cond
```

---

## 6. 实现对比

```
┌──────────────┬──────────────┬──────────────┬──────────────┬──────────────┐
│              │ futex        │ WaitOnAddress│ ulock        │ parking_lot  │
├──────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ 平台         │ Linux        │ Windows 8+   │ macOS        │ 跨平台       │
│ 粒度         │ int (4B)     │ 1/2/4/8B     │ 32/64-bit    │ 任意         │
│ 哈希表       │ 内核维护     │ 内核维护     │ 内核维护     │ 用户态全局   │
│ 超时支持     │ ✅ (timespec) │ ✅ (ms)       │ ✅ (μs)       │ ✅            │
│ 优先级继承   │ 部分         │ 有限         │ ✅            │ ❌            │
│ 虚假唤醒     │ 可能         │ 可能         │ 可能         │ 可控         │
│ 开销         │ 系统调用     │ 系统调用     │ 系统调用     │ 用户态       │
│ 大小         │ int*         │ void*        │ void*        │ 1-8 字节     │
└──────────────┴──────────────┴──────────────┴──────────────┴──────────────┘
```

---

## 7. std::atomic::wait 的标准实现结构

```
C++ 标准库实现 atomic::wait 的通用结构：

  template <typename T>
  void atomic<T>::wait(T old, memory_order mo) {
      // 1. 快速路径：检查值是否已经改变
      if (this->load(mo) != old) return;

      // 2. 短暂自旋（避免不必要的系统调用）
      for (int i = 0; i < SPIN_COUNT; ++i) {
          if (this->load(mo) != old) return;
          cpu_yield();  // x86: pause, ARM: yield
      }

      // 3. 进入阻塞等待
      while (this->load(mo) == old) {
          platform_wait(this, old);  // futex / WaitOnAddress / ulock
      }
  }

  template <typename T>
  void atomic<T>::notify_one() {
      platform_wake_one(this);  // futex_wake / WakeByAddressSingle / ulock_wake
  }

  自旋计数选择：
  · Linux libstdc++: SPIN_COUNT = 16
  · Linux libc++: SPIN_COUNT = 约 20
  · MSVC: SPIN_COUNT 取决于处理器数量
  · 目标：平衡响应延迟和系统调用开销
```

---

## 8. 虚假唤醒的处理

```cpp
// C++ 标准明确允许虚假唤醒
// 原因：实现可能在内核和用户态之间切换时产生竞态

// 正确模式：在循环中等待
std::atomic<int> state{0};

// 等待方
int current = state.load(std::memory_order_acquire);
while (current == 0) {
    state.wait(0, std::memory_order_relaxed);
    current = state.load(std::memory_order_acquire);
}

// 错误模式：假设 wait 返回时条件已满足
state.wait(0);  // 可能虚假唤醒
// 不能假设 state != 0，必须重新检查

// 虚假唤醒的来源：
// 1. futex: 其他线程 wake 了同一个 futex，但我们的条件不满足
// 2. WaitOnAddress: 超时、信号中断、哈希冲突
// 3. ulock: 内核调度器抢占
// 4. parking_lot: 哈希桶中的其他线程被唤醒
```

---

## 9. atomic wait 与 condition_variable 的区别

```cpp
// condition_variable 需要配合 mutex 使用
std::mutex mtx;
std::condition_variable cv;
bool ready = false;

// 等待方
{
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [] { return ready; });
    // mutex 已被重新获取
}

// 通知方
{
    std::lock_guard<std::mutex> lock(mtx);
    ready = true;
    cv.notify_one();
}

// atomic::wait 更轻量：
// · 不需要 mutex（避免了 mutex 的开销）
// · 不需要 predicate lambda
// · 直接在原子值上等待
// · 适合简单的"标志位"同步模式
// · 不适合需要保护复杂状态的场景（用 condvar）

// 选择指南：
// · 简单 flag/状态 → atomic::wait
// · 需要 mutex 保护的状态 → condition_variable
// · 一次性事件 → atomic::wait
// · 复杂条件（多个变量） → condition_variable
```

---

## 10. 性能特征

```
各实现的典型开销：

  操作                       futex        WaitOnAddress  parking_lot
  ──────────────────────────────────────────────────────────────────
  无争用（快速路径）         ~5 ns        ~5 ns          ~2 ns
  自旋后阻塞                ~500 ns      ~500 ns        ~300 ns
  阻塞+唤醒                 ~2-5 μs      ~2-5 μs        ~1-3 μs
  大量线程竞争              ~10-50 μs    ~10-50 μs      ~5-20 μs
  ──────────────────────────────────────────────────────────────────

  关键观察：
  · 无争用时 parking_lot 最快（纯用户态，无系统调用）
  · 有争用时差异不大（瓶颈在线程挂起/唤醒）
  · 自旋计数的选择显著影响延迟-吞吐量权衡
  · 超时等待比无限等待略慢（需要设置定时器）
```

---

## 11. 总结

```
atomic wait/notify 实现速查：
┌────────────────────────────────────────────────────────────────┐
│ 接口：wait(expected), notify_one(), notify_all()               │
│ 语义：类似 condvar 但不需要 mutex，直接在原子值上等待          │
│ 虚假唤醒：允许，必须在循环中检查条件                           │
│                                                                │
│ 平台实现：                                                     │
│ · Linux:   futex (FUTEX_WAIT / FUTEX_WAKE)                    │
│ · Windows: WaitOnAddress / WakeByAddress{Single,All}          │
│ · macOS:   __ulock_wait / __ulock_wake                        │
│ · 跨平台:  parking_lot (用户态哈希表 + 底层 OS 挂起)          │
│                                                                │
│ 选择建议：                                                     │
│ · 简单标志同步 → atomic::wait（最轻量）                       │
│ · 需要 mutex 保护复杂状态 → condition_variable                │
│ · 极端性能要求 → parking_lot（最小同步对象、最快快速路径）    │
│ · 需要优先级继承 → futex PI 或 ulock                          │
└────────────────────────────────────────────────────────────────┘
```
