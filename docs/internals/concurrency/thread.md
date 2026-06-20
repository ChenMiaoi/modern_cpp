---
title: "std::thread 实现分析"
topic: internals
feature: thread
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/bits/std_thread.h"
source_llvm: "references/impl/llvm-project/libcxx/include/__thread/thread.h"
---

# std::thread 实现分析

> `std::thread` 是 C++11 引入的线程抽象，封装了操作系统线程 API。本文基于 GCC 和 LLVM 的源码，分析 thread 的内部实现。

---

## 一、核心概念

### 1.1 什么是 thread

thread 是对操作系统线程的封装：

```cpp
// thread 的基本使用
void task() { cout << "Hello from thread" << endl; }

thread t(task);
t.join();  // 等待线程完成

// 使用 lambda
thread t2([]{ cout << "Lambda thread" << endl; });
t2.detach();  // 分离线程
```

---

## 二、核心数据结构

### 2.1 线程标识（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/std_thread.h

// 线程 ID
class thread::id {
    __gthread_t _M_thread;  // 平台相关的线程标识
public:
    id() noexcept : _M_thread() {}
    
    // 比较运算符
    bool operator==(const id& __other) const noexcept {
        return _M_thread == __other._M_thread;
    }
    
    bool operator!=(const id& __other) const noexcept {
        return _M_thread != __other._M_thread;
    }
    
    bool operator<(const id& __other) const noexcept {
        return _M_thread < __other._M_thread;
    }
};

// 获取当前线程 ID
namespace this_thread {
    thread::id get_id() noexcept {
        return thread::id(__gthread_self());
    }
    
    // 让出 CPU 时间片
    void yield() noexcept {
        __gthread_yield();
    }
    
    // 休眠
    template<typename _Rep, typename _Period>
    void sleep_for(const chrono::duration<_Rep, _Period>& __rel) {
        // 转换为纳秒
        auto __ns = chrono::duration_cast<chrono::nanoseconds>(__rel).count();
        struct timespec __ts;
        __ts.tv_sec = __ns / 1000000000;
        __ts.tv_nsec = __ns % 1000000000;
        nanosleep(&__ts, nullptr);
    }
}
```

### 2.2 GCC (libstdc++) 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/std_thread.h

// thread 类实现
class thread {
    __gthread_t _M_id;  // 线程 ID
    
public:
    // 默认构造（不拥有线程）
    thread() noexcept = default;
    
    // 构造函数：创建新线程
    template<typename _Callable, typename... _Args>
    explicit thread(_Callable&& __f, _Args&&... __args) {
        // 绑定参数
        auto __bound = __bind_simple(std::forward<_Callable>(__f),
                                     std::forward<_Args>(__args)...);
        
        // 创建线程
        int __err = __gthread_create(&_M_id, _M_entry, &__bound);
        if (__err != 0) {
            __throw_system_error(__err);
        }
    }
    
    // 移动构造
    thread(thread&& __other) noexcept : _M_id(__other._M_id) {
        __other._M_id = __gthread_t();
    }
    
    // 移动赋值
    thread& operator=(thread&& __other) {
        if (joinable()) {
            std::terminate();  // 如果当前线程可连接，终止程序
        }
        _M_id = __other._M_id;
        __other._M_id = __gthread_t();
        return *this;
    }
    
    // 析构函数
    ~thread() {
        if (joinable()) {
            std::terminate();  // 如果线程可连接，终止程序
        }
    }
    
    // 等待线程完成
    void join() {
        int __err = __gthread_join(_M_id, nullptr);
        if (__err != 0) {
            __throw_system_error(__err);
        }
        _M_id = __gthread_t();
    }
    
    // 分离线程
    void detach() {
        int __err = __gthread_detach(_M_id);
        if (__err != 0) {
            __throw_system_error(__err);
        }
        _M_id = __gthread_t();
    }
    
    // 检查是否可连接
    bool joinable() const noexcept {
        return _M_id != __gthread_t();
    }
    
    // 获取线程 ID
    id get_id() const noexcept {
        return id(_M_id);
    }
    
    // 获取硬件并发数
    static unsigned hardware_concurrency() noexcept {
        return __gthread_active_p();
    }
    
private:
    // 线程入口函数
    static void* _M_entry(void* __data) {
        auto __bound = static_cast<__bind_result*>(__data);
        (*__bound)();
        return nullptr;
    }
};
```

---

## 三、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ 线程实现               │ pthread 封装         │ pthread 封装         │
│ 线程 ID                │ 平台相关             │ 平台相关             │
│ join                   │ 支持                 │ 支持                 │
│ detach                 │ 支持                 │ 支持                 │
│ hardware_concurrency   │ 支持                 │ 支持                 │
│ jthread (C++20)        │ 支持                 │ 支持                 │
│ stop_token (C++20)     │ 支持                 │ 支持                 │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 四、最佳实践

```
thread 使用指南：

1. 优先使用 jthread（C++20）：
   · 自动 join
   · 支持协作式取消

2. 使用 RAII 管理线程：
   · 确保线程被 join 或 detach
   · 避免析构时线程仍在运行

3. 避免数据竞争：
   · 使用 mutex 保护共享数据
   · 使用 atomic 进行无锁操作

4. 使用线程池：
   · 避免频繁创建/销毁线程
   · 使用 async 或自定义线程池
```

---

## 延伸阅读

- [std::jthread 实现](/internals/concurrency/jthread) — C++20 jthread 的实现
- [std::mutex 实现](/internals/concurrency/mutex) — 互斥锁的实现
- [std::atomic 实现](/internals/concurrency/atomic) — 原子操作的实现
