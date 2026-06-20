---
title: "协程编译 Lowering 实现分析"
topic: internals
feature: coroutines
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/std/coroutine"
source_llvm: "references/impl/llvm-project/libcxx/include/__coroutine/"
---

# 协程编译 Lowering 实现分析

> C++20 协程是一种支持异步编程的语言特性，编译器将协程函数转换为状态机（lowering）。本文基于 GCC 和 LLVM 的源码，分析协程的编译器实现。

---

## 一、核心概念

### 1.1 什么是协程

协程是可以暂停和恢复执行的函数：

```cpp
// 协程示例
Task<int> async_fetch(int id) {
    auto data = co_await fetch_from_db(id);  // 暂停点
    co_return data.value();  // 返回值
}

// 调用协程
auto task = async_fetch(42);
int result = co_await task;  // 等待完成
```

### 1.2 协程的组成

```
协程的组成：

1. promise_type：
   · 定义协程的行为
   · 管理协程状态
   · 处理返回值和异常

2. coroutine_handle：
   · 持有协程状态的句柄
   · 用于恢复/销毁协程
   · 可以传递给其他线程

3. 协程帧（coroutine frame）：
   · 存储协程的局部变量
   · 存储 promise 对象
   · 存储暂停点信息
```

---

## 二、协程帧布局

### 2.1 协程帧结构

```
协程帧的内存布局：

┌─────────────────────────────────────┐
│ 虚函数表指针（vptr）                 │  ← 用于协程操作
├─────────────────────────────────────┤
│ 引用计数（可选）                     │  ← 用于共享所有权
├─────────────────────────────────────┤
│ promise 对象                        │  ← promise_type 的实例
├─────────────────────────────────────┤
│ 局部变量存储                        │  ← 协程的局部变量
├─────────────────────────────────────┤
│ 暂停点信息                          │  ← 哪里暂停的
└─────────────────────────────────────┘
```

### 2.2 GCC 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/std/coroutine

// GCC 使用 __builtin_coro_* 内建函数

// 分配协程帧
void* __builtin_coro_alloc(size_t size) {
    // 分配协程帧内存
    return ::operator new(size);
}

// 初始化协程帧
void __builtin_coro_init(void* frame, void* promise) {
    // 初始化 promise 对象
    // 设置协程帧的初始状态
}

// 暂停协程
void __builtin_coro_suspend(bool final) {
    // 如果是最终暂停：协程完成，可以销毁
    // 如果是中间暂停：协程挂起，等待恢复
}

// 恢复协程
void __builtin_coro_resume(void* handle) {
    // 从暂停点恢复执行
    // 调用协程帧的状态机
}

// 销毁协程
void __builtin_coro_destroy(void* handle) {
    // 销毁协程帧
    // 调用 promise 的析构函数
    // 释放内存
}

// 获取 promise 对象
void* __builtin_coro_promise(void* frame, size_t alignment, bool from) {
    // 从协程帧获取 promise 对象
    return (char*)frame + promise_offset;
}
```

### 2.3 LLVM 的实现（源码分析）

```cpp
// 源码路径：references/impl/llvm-project/libcxx/include/__coroutine/

// LLVM 使用类似的内建函数

// coroutine_handle 的实现
template <class _Promise>
class coroutine_handle : public coroutine_handle<void> {
public:
    // 获取 promise 对象
    _Promise& promise() const {
        return *static_cast<_Promise*>(
            __builtin_coro_promise(__ptr_, alignof(_Promise), true));
    }
    
    // 从 promise 创建协程帧
    static coroutine_handle from_promise(_Promise& __promise) {
        void* __frame = __builtin_coro_promise(
            &__promise, alignof(_Promise), false);
        return coroutine_handle::from_address(__frame);
    }
    
    // 恢复协程
    void resume() const {
        __builtin_coro_resume(__ptr_);
    }
    
    // 销毁协程
    void destroy() const {
        __builtin_coro_destroy(__ptr_);
    }
    
    // 检查是否完成
    bool done() const {
        return __builtin_coro_done(__ptr_);
    }
};
```

### 2.4 promise_type 的实现（源码分析）

```cpp
// promise_type 必须实现的接口

struct promise_type {
    // 返回协程句柄
    coroutine_handle<> get_return_object() {
        return coroutine_handle<>::from_promise(*this);
    }
    
    // 初始暂停点
    suspend_always initial_suspend() { return {}; }
    
    // 最终暂停点
    suspend_always final_suspend() noexcept { return {}; }
    
    // 返回值
    void return_void() { }
    
    // 异常处理
    void unhandled_exception() {
        std::terminate();
    }
};

// suspend_always：总是暂停
struct suspend_always {
    bool await_ready() { return false; }  // 不准备就绪
    void await_suspend(coroutine_handle<>) { }  // 暂停
    void await_resume() { }  // 恢复时什么都不做
};

// suspend_never：从不暂停
struct suspend_never {
    bool await_ready() { return true; }   // 准备就绪
    void await_suspend(coroutine_handle<>) { }  // 不会调用
    void await_resume() { }  // 恢复时什么都不做
};
```

---

## 三、协程的 Lowering 过程

### 3.1 编译器处理流程

```
协程的编译流程：

1. 识别协程函数
   · 包含 co_await, co_yield, co_return 的函数
   · 确定 promise_type

2. 生成协程帧
   · 计算帧大小
   · 分配内存
   · 初始化 promise

3. 转换为状态机
   · 每个暂停点对应一个状态
   · 生成 switch 语句
   · 每个状态包含暂停点前后的代码

4. 生成恢复函数
   · 恢复时执行对应状态
   · 处理 co_await 的等待
   · 处理 co_return 的返回
```

### 3.2 状态机示例

```
原始代码：
Task<int> fetch(int id) {
    auto data = co_await db.query(id);
    co_return data.value();
}

Lowering 后的状态机：
switch (state) {
case 0:  // 初始状态
    state = 1;
    db.query(id);  // 发起异步操作
    co_return;     // 暂停
case 1:  // 数据到达
    data = get_result();
    state = 2;
    co_return data.value();
case 2:  // 完成
    destroy_frame();
}
```

---

## 四、GCC (libstdc++) 的实现

### 4.1 coroutine_handle

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/std/coroutine

template <typename _Promise>
struct coroutine_handle {
    constexpr void* address() const noexcept { return _M_fr_ptr; }
    
    constexpr static coroutine_handle from_address(void* __a) noexcept {
        coroutine_handle __self;
        __self._M_fr_ptr = __a;
        return __self;
    }
    
    constexpr explicit operator bool() const noexcept {
        return bool(_M_fr_ptr);
    }
    
    bool done() const noexcept { return __builtin_coro_done(_M_fr_ptr); }
    void resume() const { __builtin_coro_resume(_M_fr_ptr); }
    void destroy() const { __builtin_coro_destroy(_M_fr_ptr); }
    
protected:
    void* _M_fr_ptr;  // 协程帧指针
};
```

### 4.2 coroutine_traits

```cpp
// GCC 使用 coroutine_traits 确定 promise_type
template <typename _Result, typename... _ArgTypes>
struct coroutine_traits : __coroutine_traits_impl<_Result> {};

// 如果 Result 有 promise_type 成员
template <typename _Result>
struct __coroutine_traits_impl<_Result, __void_t<typename _Result::promise_type>> {
    using promise_type = typename _Result::promise_type;
};
```

---

## 五、LLVM (libc++) 的实现

### 5.1 coroutine_handle

LLVM 的实现与 GCC 类似：

```cpp
// 源码路径：references/impl/llvm-project/libcxx/include/__coroutine/coroutine_handle.h

template <class _Promise>
class coroutine_handle : public coroutine_handle<void> {
public:
    // 与 GCC 类似的操作
    constexpr void* address() const noexcept { return __ptr_; }
    
    static coroutine_handle from_address(void* __a) noexcept {
        coroutine_handle __self;
        __self.__ptr_ = __a;
        return __self;
    }
    
    _LIBCPP_HIDE_FROM_ABI bool done() const {
        return __builtin_coro_done(__ptr_);
    }
    
    _LIBCPP_HIDE_FROM_ABI void resume() const {
        __builtin_coro_resume(__ptr_);
    }
    
    _LIBCPP_HIDE_FROM_ABI void destroy() const {
        __builtin_coro_destroy(__ptr_);
    }
    
    _Promise& promise() const {
        return *static_cast<_Promise*>(
            __builtin_coro_promise(__ptr_, alignof(_Promise), true));
    }
};
```

### 5.2 noop_coroutine

LLVM 支持 `noop_coroutine`，一个不执行任何操作的协程：

```cpp
// noop_coroutine_handle
constexpr coroutine_handle<noop_coroutine_promise>
noop_coroutine_handle::noop_coroutine() noexcept {
    return coroutine_handle<noop_coroutine_promise>::from_address(
        __builtin_coro_resume);
}
```

---

## 六、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ 协程内建函数           │ __builtin_coro_*     │ __builtin_coro_*     │
│ coroutine_handle       │ 完整                 │ 完整                 │
│ coroutine_traits       │ 完整                 │ 完整                 │
│ noop_coroutine         │ C++20                │ C++20                │
│ symmetric_transfer     │ 支持                 │ 支持                 │
│ HALO 优化              │ 部分支持             │ 完整支持             │
│ 异常处理               │ 完整                 │ 完整                 │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 七、性能特征

```
协程的性能：

内存开销：
  · 协程帧：通常 64-256 字节
  · promise 对象：取决于类型
  · 局部变量：取决于函数

时间开销：
  · 暂停/恢复：~10-100 ns
  · 比函数调用慢 5-10 倍
  · 比线程切换快 10-100 倍

优化技巧：
  · 使用 noop_coroutine 避免不必要的暂停
  · 使用 symmetric_transfer 减少栈使用
  · 使用 HALO（Heap Allocation eLision Optimization）避免堆分配
```

---

## 延伸阅读

- [std::jthread 实现](/internals/concurrency/jthread) — 协程与线程的交互
- [std::generator 实现](/internals/cpp23/generator) — 协程的实际应用
- [std::expected 实现](/internals/cpp23/expected) — 错误处理与协程
