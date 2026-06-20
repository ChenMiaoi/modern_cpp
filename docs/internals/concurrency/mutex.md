---
title: "std::mutex 实现分析"
topic: internals
feature: mutex
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/bits/std_mutex.h"
source_llvm: "references/impl/llvm-project/libcxx/include/__mutex/mutex.h"
---

# std::mutex 实现分析

> `std::mutex` 是 C++11 引入的互斥锁，用于保护共享数据。本文基于 GCC 和 LLVM 的源码，分析 mutex 的内部实现。

---

## 一、核心概念

### 1.1 什么是 mutex

mutex 是一种同步原语，确保同一时间只有一个线程可以访问共享数据：

```cpp
// mutex 的基本使用
mutex mtx;
int shared_data = 0;

void increment() {
    lock_guard<mutex> lock(mtx);  // 加锁
    shared_data++;                // 临界区
}  // 自动解锁
```

---

## 二、核心数据结构

### 2.1 GCC (libstdc++) 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/std_mutex.h

class mutex {
    __gthread_mutex_t _M_mutex = __GTHREAD_MUTEX_INIT;
    
public:
    constexpr mutex() noexcept = default;
    mutex(const mutex&) = delete;
    mutex& operator=(const mutex&) = delete;
    
    // 加锁
    void lock() {
        __gthread_mutex_lock(&_M_mutex);
    }
    
    // 尝试加锁（非阻塞）
    bool try_lock() noexcept {
        return __gthread_mutex_trylock(&_M_mutex) == 0;
    }
    
    // 解锁
    void unlock() noexcept {
        __gthread_mutex_unlock(&_M_mutex);
    }
    
    // 获取原生句柄（用于高级操作）
    native_handle_type native_handle() noexcept {
        return _M_mutex;
    }
};
```

### 2.2 lock_guard 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/std_mutex.h

template<typename _Mutex>
class lock_guard {
public:
    typedef _Mutex mutex_type;
    
    // 显式构造：加锁
    explicit lock_guard(mutex_type& __m) : _M_mutex(__m) {
        _M_mutex.lock();
    }
    
    // 带标签构造：假设已经加锁
    lock_guard(mutex_type& __m, defer_lock_t) noexcept : _M_mutex(__m) { }
    
    // 拷贝构造：删除
    lock_guard(const lock_guard&) = delete;
    lock_guard& operator=(const lock_guard&) = delete;
    
    // 析构：解锁
    ~lock_guard() {
        _M_mutex.unlock();
    }
    
private:
    mutex_type& _M_mutex;
};
```

### 2.3 unique_lock 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/unique_lock.h

template<typename _Mutex>
class unique_lock {
public:
    typedef _Mutex mutex_type;
    
    // 默认构造
    unique_lock() noexcept : _M_mutex(nullptr), _M_owns(false) { }
    
    // 显式构造：加锁
    explicit unique_lock(mutex_type& __m) : _M_mutex(&__m), _M_owns(false) {
        lock();
    }
    
    // 延迟构造：不加锁
    unique_lock(mutex_type& __m, defer_lock_t) noexcept
    : _M_mutex(&__m), _M_owns(false) { }
    
    // 已加锁构造
    unique_lock(mutex_type& __m, try_to_lock_t)
    : _M_mutex(&__m), _M_owns(__m.try_lock()) { }
    
    // 移动构造
    unique_lock(unique_lock&& __u) noexcept
    : _M_mutex(__u._M_mutex), _M_owns(__u._M_owns) {
        __u._M_mutex = nullptr;
        __u._M_owns = false;
    }
    
    // 析构
    ~unique_lock() {
        if (_M_owns) unlock();
    }
    
    // 加锁
    void lock() {
        _M_mutex->lock();
        _M_owns = true;
    }
    
    // 尝试加锁
    bool try_lock() {
        _M_owns = _M_mutex->try_lock();
        return _M_owns;
    }
    
    // 解锁
    void unlock() {
        _M_mutex->unlock();
        _M_owns = false;
    }
    
    // 检查是否拥有锁
    bool owns_lock() const noexcept { return _M_owns; }
    
    // 显式转换为 bool
    explicit operator bool() const noexcept { return _M_owns; }
    
    // 获取 mutex
    mutex_type* mutex() const noexcept { return _M_mutex; }
    
private:
    mutex_type* _M_mutex;
    bool _M_owns;
};
```

---

## 三、锁守卫

### 3.1 lock_guard

```cpp
// lock_guard 的实现
template<typename _Mutex>
class lock_guard {
    _Mutex& _M_mutex;
    
public:
    explicit lock_guard(_Mutex& __m) : _M_mutex(__m) {
        _M_mutex.lock();
    }
    
    ~lock_guard() {
        _M_mutex.unlock();
    }
};
```

### 3.2 unique_lock

```cpp
// unique_lock 的实现
template<typename _Mutex>
class unique_lock {
    _Mutex* _M_mutex;
    bool _M_owns;
    
public:
    explicit unique_lock(_Mutex& __m) : _M_mutex(&__m), _M_owns(false) {
        lock();
    }
    
    ~unique_lock() {
        if (_M_owns) unlock();
    }
    
    void lock() {
        _M_mutex->lock();
        _M_owns = true;
    }
    
    void unlock() {
        _M_mutex->unlock();
        _M_owns = false;
    }
};
```

---

## 四、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ mutex 实现             │ pthread_mutex        │ pthread_mutex        │
│ lock_guard             │ 支持                 │ 支持                 │
│ unique_lock            │ 支持                 │ 支持                 │
│ recursive_mutex        │ 支持                 │ 支持                 │
│ timed_mutex            │ 支持                 │ 支持                 │
│ shared_mutex           │ C++17                │ C++17                │
│ once_flag              │ 支持                 │ 支持                 │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 五、最佳实践

```
mutex 使用指南：

1. 优先使用 lock_guard：
   · RAII 风格
   · 自动解锁

2. 使用 unique_lock 需要灵活控制时：
   · 可以延迟加锁
   · 可以手动解锁

3. 使用 shared_mutex 实现读写锁：
   · 多读单写场景
   · 提高并发度

4. 避免死锁：
   · 固定加锁顺序
   · 使用 try_lock
   · 使用 lock() 同时锁多个 mutex
```

---

## 延伸阅读

- [std::atomic 实现](/internals/concurrency/atomic) — 原子操作的实现
- [std::condition_variable 实现](/internals/concurrency/condition-variable) — 条件变量的实现
- [std::thread 实现](/internals/concurrency/thread) — 线程的实现
