---
title: "std::condition_variable 实现分析"
topic: internals
feature: condition-variable
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/std/condition_variable"
source_llvm: "references/impl/llvm-project/libcxx/include/__condition_variable/condition_variable.h"
---

# std::condition_variable 实现分析

> `std::condition_variable` 是 C++11 引入的条件变量，用于线程间的同步通信。本文基于 GCC 和 LLVM 的源码，分析 condition_variable 的内部实现。

---

## 一、核心概念

### 1.1 什么是条件变量

条件变量允许线程在某个条件满足时等待或通知：

```cpp
// 条件变量的使用
mutex mtx;
condition_variable cv;
bool ready = false;

// 等待线程
unique_lock<mutex> lock(mtx);
cv.wait(lock, []{ return ready; });  // 等待条件满足

// 通知线程
{
    lock_guard<mutex> lock(mtx);
    ready = true;
}
cv.notify_one();  // 通知一个等待线程
```

---

## 二、核心数据结构

### 2.1 GCC (libstdc++) 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/std/condition_variable

class condition_variable {
    __gthread_cond_t _M_cond = __GTHREAD_COND_INIT;
    
public:
    condition_variable() noexcept = default;
    ~condition_variable() noexcept = default;
    
    // 禁止拷贝
    condition_variable(const condition_variable&) = delete;
    condition_variable& operator=(const condition_variable&) = delete;
    
    // 等待（阻塞当前线程）
    void wait(unique_lock<mutex>& __lock) {
        // 释放锁 -> 等待通知 -> 重新获取锁
        __gthread_cond_wait(&_M_cond, __lock.mutex()->native_handle());
    }
    
    // 带谓词的等待（避免虚假唤醒）
    template<typename _Predicate>
    void wait(unique_lock<mutex>& __lock, _Predicate __pred) {
        while (!__pred()) {
            wait(__lock);
        }
    }
    
    // 带超时的等待
    template<typename _Rep, typename _Period>
    cv_status wait_for(unique_lock<mutex>& __lock,
                       const chrono::duration<_Rep, _Period>& __rel) {
        // 转换为绝对时间
        auto __abs = chrono::steady_clock::now() + __rel;
        return __wait_until_impl(__lock, __abs);
    }
    
    // 带谓词和超时的等待
    template<typename _Rep, typename _Period, typename _Predicate>
    bool wait_for(unique_lock<mutex>& __lock,
                  const chrono::duration<_Rep, _Period>& __rel,
                  _Predicate __pred) {
        while (!__pred()) {
            if (wait_for(__lock, __rel) == cv_status::timeout) {
                return __pred();  // 超时后最后一次检查
            }
        }
        return true;
    }
    
    // 通知一个等待线程
    void notify_one() noexcept {
        __gthread_cond_signal(&_M_cond);
    }
    
    // 通知所有等待线程
    void notify_all() noexcept {
        __gthread_cond_broadcast(&_M_cond);
    }
};
```

### 2.2 cv_status 枚举（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/std/condition_variable

// 条件变量等待状态
enum class cv_status {
    timeout,   // 超时
    no_timeout // 正常唤醒
};
```

### 2.2 LLVM (libc++) 的实现

```cpp
// 源码路径：references/impl/llvm-project/libcxx/include/__condition_variable/condition_variable.h

class condition_variable {
    pthread_cond_t __cv_;
    
public:
    void wait(unique_lock<mutex>& __lock) {
        int __r = pthread_cond_wait(&__cv_, __lock.mutex()->native_handle());
        // 错误处理
    }
    
    void notify_one() noexcept {
        pthread_cond_signal(&__cv_);
    }
    
    void notify_all() noexcept {
        pthread_cond_broadcast(&__cv_);
    }
};
```

---

## 三、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ condition_variable     │ pthread_cond         │ pthread_cond         │
│ wait                   │ 支持                 │ 支持                 │
│ wait_for               │ 支持                 │ 支持                 │
│ wait_until             │ 支持                 │ 支持                 │
│ notify_one             │ 支持                 │ 支持                 │
│ notify_all             │ 支持                 │ 支持                 │
│ native_handle          │ 支持                 │ 支持                 │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 四、最佳实践

```
条件变量使用指南：

1. 总是配合 mutex 使用：
   · wait 前加锁
   · notify 前加锁

2. 使用谓词版本的 wait：
   · 避免虚假唤醒
   · 更简洁

3. 选择合适的 notify：
   · notify_one：唤醒一个线程
   · notify_all：唤醒所有线程

4. 注意条件变量的生命周期：
   · 确保等待线程能访问到条件变量
   · 避免悬垂引用
```

---

## 延伸阅读

- [std::mutex 实现](/internals/concurrency/mutex) — 互斥锁的实现
- [std::thread 实现](/internals/concurrency/thread) — 线程的实现
- [std::future 实现](/internals/concurrency/future) — 异步操作的实现
