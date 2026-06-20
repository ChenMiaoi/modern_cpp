---
title: "std::jthread 实现分析"
topic: internals
feature: jthread
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/jthread"
source_llvm: "references/impl/llvm-project/libcxx/include/__thread/jthread.h"
---

# std::jthread 实现分析

> `std::jthread` 是 C++20 引入的自动 join 的线程，支持协作式取消。本文基于 GCC 和 LLVM 的源码，分析 jthread 的内部实现。

---

## 一、核心概念

### 1.1 什么是 jthread

jthread 是 RAII 风格的线程，在析构时自动 join 或 request_stop：

```cpp
// jthread 的基本使用
jthread t([](stop_token stoken) {
    while (!stoken.stop_requested()) {
        // 执行工作
        this_thread::sleep_for(100ms);
    }
});

// 析构时自动 request_stop + join
```

### 1.2 jthread vs thread

```
jthread vs thread：

thread：
  · 需要手动 join 或 detach
  · 不支持协作式取消
  · 析构时如果 joinable 会 terminate

jthread：
  · 自动 join
  · 支持协作式取消
  · 析构时自动 request_stop + join
```

---

## 二、核心数据结构

### 2.1 stop_token（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/stop_token

// stop_token 用于协作式取消
class stop_token {
public:
    // 检查是否请求停止
    bool stop_requested() const noexcept {
        return _M_stop_state && _M_stop_state->_M_stop_requested();
    }
    
    // 检查是否可能停止
    bool stop_possible() const noexcept {
        return _M_stop_state && _M_stop_state->_M_stop_possible();
    }
    
    // 比较运算符
    friend bool operator==(const stop_token& __a, const stop_token& __b) noexcept {
        return __a._M_stop_state == __b._M_stop_state;
    }

private:
    shared_ptr<stop_state> _M_stop_state;
};

// stop_source 用于请求停止
class stop_source {
public:
    // 获取 stop_token
    stop_token get_token() const noexcept {
        return stop_token(_M_stop_state);
    }
    
    // 检查是否请求停止
    bool stop_requested() const noexcept {
        return _M_stop_state && _M_stop_state->_M_stop_requested();
    }
    
    // 检查是否可能停止
    bool stop_possible() const noexcept {
        return _M_stop_state && _M_stop_state->_M_stop_possible();
    }
    
    // 请求停止
    void request_stop() noexcept {
        if (_M_stop_state) {
            _M_stop_state->_M_request_stop();
        }
    }
    
    // 交换
    void swap(stop_source& __other) noexcept {
        std::swap(_M_stop_state, __other._M_stop_state);
    }

private:
    shared_ptr<stop_state> _M_stop_state;
};
```

### 2.2 stop_callback（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/stop_token

// stop_callback 用于注册取消回调
template<typename _Tp>
class stop_callback {
public:
    // 构造函数：注册回调
    template<typename _Fn>
    stop_callback(const stop_token& __token, _Fn&& __fn)
    : _M_stop_token(__token), _M_callback(std::forward<_Fn>(__fn)) {
        if (_M_stop_token.stop_possible()) {
            _M_stop_state = _M_stop_token._M_stop_state;
            _M_stop_state->_M_register_callback(this);
        }
    }
    
    // 析构函数：注销回调
    ~stop_callback() {
        if (_M_stop_state) {
            _M_stop_state->_M_unregister_callback(this);
        }
    }
    
    // 检查是否请求停止
    bool stop_requested() const noexcept {
        return _M_stop_token.stop_requested();
    }

private:
    stop_token _M_stop_token;
    _Tp _M_callback;
    shared_ptr<stop_state> _M_stop_state;
};
```

### 2.3 jthread 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/jthread

class jthread {
    thread _M_thread;
    stop_source _M_stop_source;
    
public:
    // 构造函数：创建线程
    template<typename _Callable, typename... _Args>
    explicit jthread(_Callable&& __f, _Args&&... __args) {
        // 创建线程，传递 stop_token
        _M_thread = thread(
            std::forward<_Callable>(__f),
            _M_stop_source.get_token(),
            std::forward<_Args>(__args)...);
    }
    
    // 析构函数：自动 request_stop + join
    ~jthread() {
        if (joinable()) {
            request_stop();
            join();
        }
    }
    
    // 移动构造
    jthread(jthread&& __other) noexcept
    : _M_thread(std::move(__other._M_thread)),
      _M_stop_source(std::move(__other._M_stop_source)) { }
    
    // 移动赋值
    jthread& operator=(jthread&& __other) noexcept {
        if (joinable()) {
            request_stop();
            join();
        }
        _M_thread = std::move(__other._M_thread);
        _M_stop_source = std::move(__other._M_stop_source);
        return *this;
    }
    
    // 请求停止
    void request_stop() {
        _M_stop_source.request_stop();
    }
    
    // 获取 stop_token
    stop_token get_stop_token() const noexcept {
        return _M_stop_source.get_token();
    }
    
    // 检查是否 joinable
    bool joinable() const noexcept {
        return _M_thread.joinable();
    }
    
    // join
    void join() {
        _M_thread.join();
    }
    
    // detach
    void detach() {
        _M_thread.detach();
    }
    
    // 获取线程 ID
    thread::id get_id() const noexcept {
        return _M_thread.get_id();
    }
    
    // 硬件并发数
    static unsigned hardware_concurrency() noexcept {
        return thread::hardware_concurrency();
    }
};
```

---

## 三、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ jthread                │ 支持                 │ 支持                 │
│ stop_token             │ 支持                 │ 支持                 │
│ stop_source            │ 支持                 │ 支持                 │
│ stop_callback          │ 支持                 │ 支持                 │
│ 自动 join              │ 支持                 │ 支持                 │
│ 协作式取消             │ 支持                 │ 支持                 │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 四、最佳实践

```
jthread 使用指南：

1. 优先使用 jthread 替代 thread：
   · 更安全（自动 join）
   · 支持协作式取消

2. 使用 stop_token 实现取消：
   void task(stop_token stoken) {
       while (!stoken.stop_requested()) {
           // 执行工作
       }
   }

3. 使用 stop_callback 注册取消回调：
   stop_callback cb(stoken, []{
       // 清理资源
   });

4. 注意析构顺序：
   · jthread 析构时会 request_stop + join
   · 确保任务能响应 stop 请求
```

---

## 延伸阅读

- [std::thread 实现](/internals/concurrency/thread) — 线程的实现
- [std::atomic 实现](/internals/concurrency/atomic) — 原子操作的实现
- [std::future 实现](/internals/concurrency/future) — 异步操作的实现
