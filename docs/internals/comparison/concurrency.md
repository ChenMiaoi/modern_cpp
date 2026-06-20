---
title: "并发实现对比"
topic: internals
feature: comparison-concurrency
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/bits/atomic_base.h"
source_llvm: "references/impl/llvm-project/libcxx/include/__atomic/"
---

# 并发实现对比

> libstdc++ 和 libc++ 在并发原语实现上有差异。本文对比两者的实现策略。

---

## 一、atomic 对比

### 1.1 GCC (libstdc++) 的 atomic（源码分析）

```
libstdc++ atomic：

实现方式：
  · 使用 __atomic_* 内建函数
  · 支持双字原子优化
  · 支持三种锁策略

关键源码：
  · atomic_base.h：__atomic_base 类定义
  · atomic_lockfree_defines.h：lock-free 属性

性能特点：
  · 在支持的平台上 lock-free
  · 双字原子操作优化
```

### 1.2 LLVM (libc++) 的 atomic（源码分析）

```
libc++ atomic：

实现方式：
  · 使用 __atomic_* 内建函数
  · 支持 trivial_abi
  · 支持 constexpr

关键源码：
  · __atomic/atomic.h：atomic 类定义
  · __atomic/atomic_sync.h：同步操作

性能特点：
  · 与 libstdc++ 类似
  · 但更现代的实现
```

---

## 二、mutex 对比

```
mutex 对比：

libstdc++：
  · 使用 pthread_mutex
  · 支持 timed_mutex

libc++：
  · 使用 pthread_mutex
  · 支持 timed_mutex
```

---

## 三、thread 对比

```
thread 对比：

libstdc++：
  · 使用 pthread_create
  · 支持 jthread

libc++：
  · 使用 pthread_create
  · 支持 jthread
```

---

## 四、future/promise 对比

```
future/promise 对比：

libstdc++：
  · shared_state 使用 mutex
  · 支持 packaged_task

libc++：
  · shared_state 使用 mutex
  · 支持 packaged_task
```

---

## 延伸阅读

- [std::atomic 实现](/internals/concurrency/atomic) — atomic 的详细实现
- [std::mutex 实现](/internals/concurrency/mutex) — mutex 的详细实现
- [std::thread 实现](/internals/concurrency/thread) — thread 的详细实现
