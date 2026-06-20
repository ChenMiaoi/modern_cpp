---
title: "std::future/promise 实现分析"
topic: internals
feature: future
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/std/future"
source_llvm: "references/impl/llvm-project/libcxx/include/future"
---

# std::future/promise 实现分析

> `std::future` 和 `std::promise` 是 C++11 引入的异步编程工具，用于在不同线程间传递结果。本文基于 GCC 和 LLVM 的源码，分析 future/promise 的内部实现。

---

## 一、核心概念

### 1.1 什么是 future/promise

future 表示一个异步操作的结果，promise 是设置这个结果的接口：

```cpp
// future/promise 的基本使用
promise<int> p;
future<int> f = p.get_future();

// 在另一个线程中设置结果
thread t([&p]{
    p.set_value(42);
});

// 获取结果
cout << f.get() << endl;  // 阻塞等待结果
t.join();
```

---

## 二、核心数据结构

### 2.1 shared_state（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/std/future

// future/promise 共享的状态
template<typename _Tp>
struct _State_base {
    // 状态枚举
    enum _Status { __unfinished, __ready, __broken };
    
    _Status _M_status = __unfinished;  // 当前状态
    _Tp _M_result;                     // 存储结果
    condition_variable _M_cond;        // 条件变量
    mutex _M_mutex;                    // 互斥锁
    
    // 检查是否就绪
    bool _M_ready() const {
        lock_guard<mutex> __lock(_M_mutex);
        return _M_status == __ready;
    }
    
    // 设置结果（由 promise 调用）
    void _M_set_result(_Tp __result) {
        lock_guard<mutex> __lock(_M_mutex);
        _M_result = std::move(__result);
        _M_status = __ready;
        _M_cond.notify_all();  // 通知所有等待线程
    }
    
    // 设置异常
    void _M_set_exception(exception_ptr __exception) {
        lock_guard<mutex> __lock(_M_mutex);
        _M_exception = __exception;
        _M_status = __broken;
        _M_cond.notify_all();
    }
    
    // 获取结果（阻塞等待）
    _Tp _M_get_result() {
        unique_lock<mutex> __lock(_M_mutex);
        // 等待状态变为 ready 或 broken
        _M_cond.wait(__lock, [this]{
            return _M_status != __unfinished;
        });
        
        if (_M_status == __broken) {
            std::rethrow_exception(_M_exception);
        }
        return std::move(_M_result);
    }
    
    // 获取结果（带超时）
    template<typename _Rep, typename _Period>
    future_status _M_wait_for(const chrono::duration<_Rep, _Period>& __rel) {
        unique_lock<mutex> __lock(_M_mutex);
        if (_M_cond.wait_for(__lock, __rel, [this]{
            return _M_status != __unfinished;
        })) {
            return _M_status == __ready ? future_status::ready 
                                        : future_status::deferred;
        }
        return future_status::timeout;
    }
    
private:
    exception_ptr _M_exception;  // 存储异常
};
```

### 2.2 promise 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/std/future

template<typename _Tp>
class promise {
    shared_ptr<_State_base<_Tp>> _M_state;
    
public:
    promise() : _M_state(make_shared<_State_base<_Tp>>()) { }
    
    // 获取 future
    future<_Tp> get_future() {
        return future<_Tp>(_M_state);
    }
    
    // 设置值
    void set_value(const _Tp& __value) {
        _M_state->_M_set_result(__value);
    }
    
    void set_value(_Tp&& __value) {
        _M_state->_M_set_result(std::move(__value));
    }
    
    // 设置异常
    void set_exception(exception_ptr __exception) {
        _M_state->_M_set_exception(__exception);
    }
    
    // 移动构造
    promise(promise&& __other) noexcept
    : _M_state(std::move(__other._M_state)) { }
};
```

---

## 三、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ shared_state           │ _State_base          │ __shared_state       │
│ future                 │ 支持                 │ 支持                 │
│ promise                │ 支持                 │ 支持                 │
│ packaged_task         │ 支持                 │ 支持                 │
│ async                  │ 支持                 │ 支持                 │
│ future_error           │ 支持                 │ 支持                 │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 四、最佳实践

```
future/promise 使用指南：

1. 优先使用 async：
   auto f = async([]{ return 42; });

2. 使用 packaged_task 包装回调：
   packaged_task<int()> task([]{ return 42; });
   auto f = task.get_future();
   thread t(std::move(task));

3. 使用 future::wait_for 超时等待：
   auto status = f.wait_for(chrono::seconds(1));

4. 注意异常处理：
   · promise::set_exception 可以传递异常
   · future::get 会重新抛出异常
```

---

## 延伸阅读

- [std::jthread 实现](/internals/concurrency/jthread) — C++20 jthread 的实现
- [std::thread 实现](/internals/concurrency/thread) — 线程的实现
- [std::mutex 实现](/internals/concurrency/mutex) — 互斥锁的实现
