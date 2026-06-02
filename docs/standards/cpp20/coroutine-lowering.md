---
title: "协程编译器 Lowering：帧布局、分配、对称转移与 HALO 验证"
topic: cpp20
feature: coroutine-lowering
standard: C++20
status_checked_at: 2026-06-02
standard_refs:
  - draft: N4861
    clause: "[dcl.fct.def.coroutine]"
  - draft: N4861
    clause: "[coroutine.handle]"
  - draft: N4861
    clause: "[coroutine.traits]"
proposals:
  - P0057R9
  - P0620R0
  - P0913R0
  - P1477R4
  - P2008R0
exercises: []
solutions: []
---

# 协程编译器 Lowering（内部机制）

## 概述

协程函数（含 `co_await`、`co_yield` 或 `co_return` 的函数）在编译时经历一次彻底的 **lowering（降级）变换**：编译器将原始协程体拆分为多个代码段，在堆（或 HALO 优化后的栈）上分配一个协程帧（coroutine frame），用状态机驱动执行。理解这个 lowering 过程是正确实现自定义 promise 类型、排查协程性能问题和验证 HALO 优化的前提。

本文深入编译器视角，覆盖：协程函数如何被拆分、协程帧的内存布局、`operator new` 分配路径与 `get_return_object_on_allocation_failure`、通过 `coroutine_traits` 发现 promise 类型、`await_transform` 自定义 `co_await`、异常处理链、`final_suspend` 的 UB、destroy 时机与帧所有权、对称转移的尾调用验证、HALO 优化的可控验证，以及前后 IR 对比示例。

## 协程函数的 Lowering：编译器如何拆分

一个协程函数体被编译器转换为以下结构（伪代码）：

```
// 原始协程
Task<int> foo(int x) {
    int local = x + 1;
    co_await some_op();
    co_return local * 2;
}

// 编译器 lowering 后的逻辑结构
Task<int> foo(int x) {
    // 1. 分配协程帧
    void* frame = operator new(coroutine_frame_size);

    // 2. 初始化帧内各字段
    auto& promise = construct_promise_in(frame);
    auto return_obj = promise.get_return_object();  // 可能挂起!
    frame->suspend_index = 0;        // 初始状态
    frame->param_x = x;              // 参数拷贝到帧
    // frame->local_local 未初始化

    // 3. initial_suspend
    auto init_await = promise.initial_suspend();
    if (!init_await.await_ready()) {
        init_await.await_suspend(handle);
        // 挂起，等待 resume()
    }
    // -- resume 进入点 --

    // 4. switch(fsm_index) 驱动状态机
    switch (frame->suspend_index) {
    case 0: goto resume_0;
    case 1: goto resume_1;
    }

resume_0:
    // 协程体第1段：initial_suspend 之后、第一个 co_await 之前
    frame->local_local = frame->param_x + 1;

    // 5. co_await some_op()
    {
        auto awaiter = get_awaiter(some_op());
        if (!awaiter.await_ready()) {
            frame->suspend_index = 1;       // 保存恢复点
            frame->current_await = &awaiter; // 保存当前 awaiter
            awaiter.await_suspend(handle);
            return;                          // 从函数返回
        }
        awaiter.await_resume();
    }
    // fall through to resume_1 if not suspended

resume_1:
    // 协程体第2段：co_await 之后
    // co_return local * 2
    promise.return_value(frame->local_local * 2);
    goto final_suspend;

final_suspend:
    auto final_await = promise.final_suspend();
    // final_await 的 await_ready() 必须返回 false
    final_await.await_suspend(handle);
    // 不返回——由对称转移或手动 destroy 处理
}
```

关键点：
- 参数在协程入口处被**拷贝到协程帧**（非引用捕获时），避免栈销毁后悬空
- `suspend_index` 是编译器生成的枚举，每个挂起点对应一个状态
- 编译器通常使用 **switch + computed goto** 而非真正的 `goto`，以跨平台兼容

## 协程帧内存布局

协程帧（coroutine frame）是编译器管理的运行时数据结构。典型布局如下：

```
协程帧内存布局（以 Clang x86-64 为例，简化）
┌─────────────────────────────────┐  ← 帧起始地址
│  vptr（如果 promise 有虚析构）   │  8 bytes
├─────────────────────────────────┤
│  coroutine_handle 指向帧的 ptr  │  8 bytes（由编译器管理）
├─────────────────────────────────┤
│  suspend_index (状态机寄存器)    │  4 bytes
├─────────────────────────────────┤
│  Promise object                │  sizeof(Promise)
│    ├─ get_return_object() 结果  │
│    ├─ promise 自身字段           │
│    └─ 异常指针 (exception_ptr)  │
├─────────────────────────────────┤
│  函数参数副本                   │  各参数大小之和（对齐填充）
│    ├─ param_x: int             │
│    └─ param_y: string (如有)   │
├─────────────────────────────────┤
│  局部变量存储                   │  各局部变量的最大活跃域
│    ├─ local_local: int         │
│    └─ temp: ...                │
├─────────────────────────────────┤
│  当前 awaiter 存储              │  sizeof(最大 awaiter)
│    └─ current_await: Awaiter   │
└─────────────────────────────────┘

总大小：编译器在编译期计算，作为参数传递给 operator new
```

编译器如何确定帧大小：

```cpp
// 编译器内部计算（概念性伪代码）
size_t frame_size =
    sizeof(__coroutine_frame_header)   // suspend_index + handle 指针
  + sizeof(Promise)                    // promise_type 实例
  + align_and_size(params...)          // 参数副本
  + max_active_locals_size(...)        // 局部变量的活跃域分析
  + max_awaiters_size(...)             // 所有 co_await 表达式的最大 awaiter
  + padding_for_alignment;
```

**注意**：局部变量跨挂起点才需要存入帧。只在两个挂起点之间活跃的临时变量可以保持在寄存器中，由编译器的 liveness 分析决定。

## 分配路径：operator new 与自定义分配

编译器在协程入口生成如下分配代码：

```cpp
// 编译器生成的分配逻辑（概念性）
void* __coroutine_frame = nullptr;
try {
    __coroutine_frame = ::operator new(frame_size);
} catch (...) {
    // 如果 promise_type 定义了 get_return_object_on_allocation_failure()
    // 则不抛异常，直接调用该函数返回
    return Promise::get_return_object_on_allocation_failure();
}
```

### get_return_object_on_allocation_failure

这是一个可选的静态成员函数。当 promise 类型定义了它，编译器将使用 **nothrow 版本**的 `operator new`，分配失败时调用此函数而非抛 `std::bad_alloc`：

```cpp
struct LazyTask {
    struct promise_type {
        static LazyTask get_return_object_on_allocation_failure() noexcept {
            return LazyTask{nullptr};  // 返回无效句柄
        }
        // ... 其他成员
    };
};
```

**何时使用**：嵌入式系统或实时场景中禁用异常时，需要这个路径来避免链接 `throw`。

### 自定义 operator new

协程帧还支持在 promise 或返回类型上定义 `operator new`：

```cpp
struct PooledTask {
    struct promise_type {
        // 优先于全局 operator new
        static void* operator new(std::size_t size) {
            return pool::allocate(size);  // 自定义内存池
        }
        static void operator delete(void* ptr, std::size_t size) noexcept {
            pool::deallocate(ptr, size);
        }
        // ...
    };
};
```

编译器按以下顺序查找分配函数：
1. 返回类型（`Task`）的 `operator new`
2. promise 类型的 `operator new`
3. 全局 `::operator new`

## promise_type 的发现：coroutine_traits

编译器通过 `std::coroutine_traits<ReturnType, Params...>::promise_type` 查找 promise 类型。默认实现：

```cpp
// std 命名空间中的默认 trait
template <typename R, typename... Args>
struct coroutine_traits {
    using promise_type = typename R::promise_type;
};

// R 就是协程的返回类型
// Args 是参数类型（用于特化场景）
```

**特化场景**：当返回类型没有 `promise_type` 嵌套成员时，可以特化 `coroutine_traits`：

```cpp
// 为 std::future<int> 提供协程支持
template <typename... Args>
struct std::coroutine_traits<std::future<int>, Args...> {
    struct promise_type {
        std::promise<int> p;
        std::future<int> get_return_object() { return p.get_future(); }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_value(int v) { p.set_value(v); }
        void unhandled_exception() { p.set_exception(std::current_exception()); }
    };
};
```

这样 `std::future<int> my_coro(...)` 自动成为协程，无需修改 `std::future` 本身。

## await_transform：自定义 co_await 行为

如果 promise 类型定义了 `await_transform`，编译器会将 `co_await expr` 重写为 `co_await promise.await_transform(expr)`。这是控制 `co_await` 语义的强力钩子：

```cpp
struct SwitchTask {
    struct promise_type {
        // 所有 co_await 表达式都经过此函数
        auto await_transform(std::coroutine_handle<> h) {
            struct SwitchAwaiter {
                std::coroutine_handle<> target;
                bool await_ready() noexcept { return false; }
                // 对称转移：直接跳转到 target
                std::coroutine_handle<> await_suspend(
                    std::coroutine_handle<>) noexcept {
                    return target;
                }
                void await_resume() noexcept {}
            };
            return SwitchAwaiter{h};
        }

        // 禁止直接 co_await 其他 awaitable
        void await_transform(auto&) = delete;

        // ... 其他成员
    };
};
```

**典型应用**：
- **EagerlyEvaluated awaitable**：将 `co_await` 限制为特定 awaitable 类型
- **Dispatcher**：确保协程在特定线程恢复（如 UI 线程）
- **禁止 co_await**：`await_transform(auto&) = delete` 使所有 `co_await` 编译失败

```cpp
// 实际应用：限制 co_await 的类型
struct SafeTask {
    struct promise_type {
        // 只允许 co_await 已知安全的 awaitable
        template <typename T>
        auto await_transform(SafeAwaitable<T> a) { return a; }

        // 其他类型一律编译错误
        template <typename T>
            requires (!is_safe_awaitable_v<T>)
        void await_transform(T&&) = delete;
        // ...
    };
};
```

## 异常处理：unhandled_exception 与 await_resume 重抛

协程中的异常传播分两条路径：

### 路径一：协程体内的异常

```cpp
void co_await_work() {
    auto awaiter = get_awaitable(expr);
    try {
        if (!awaiter.await_ready()) {
            suspend_index = N;
            current_await = &awaiter;
            awaiter.await_suspend(handle);
            return;
        }
    } catch (...) {
        promise.unhandled_exception();  // 捕获到 promise
        goto final_suspend;
    }
    awaiter.await_resume();  // 可能抛异常 → unhandled_exception
}
```

`unhandled_exception()` 的典型实现：

```cpp
struct promise_type {
    std::exception_ptr ex_;

    void unhandled_exception() {
        ex_ = std::current_exception();  // 存储而非终止
    }

    // 在 await_resume 中重抛
    void rethrow_if_failed() {
        if (ex_) std::rethrow_exception(ex_);
    }
};
```

### 路径二：恢复后在调用者侧重抛

```cpp
// Task 的 await_resume 在调用者侧重抛协程中存储的异常
struct Task {
    struct Awaiter {
        std::coroutine_handle<promise_type> h;
        bool await_ready() noexcept { return false; }
        void await_suspend(std::coroutine_handle<> caller) { /* ... */ }
        int await_resume() {
            if (h.promise().ex_)
                std::rethrow_exception(h.promise().ex_);
            return h.promise().result;
        }
    };
};
```

**链条总结**：协程体抛异常 → `unhandled_exception()` 存储 → `final_suspend()` 挂起 → 调用者恢复后通过 `await_resume()` 重抛。

## final_suspend 与 UB

`final_suspend()` 返回的 awaiter 的 `await_ready()` **必须返回 `false`**（即必须挂起）。如果返回 `true`：

```cpp
// 危险示例——不要这样做
std::suspend_never final_suspend() noexcept { return {}; }
// 等价于 await_ready() == true → 协程帧立即销毁
// 但 coroutine_handle 可能仍被持有 → 悬空句柄 → UB
```

当 `final_suspend` 不挂起时，编译器在协程体执行完毕后**立即销毁协程帧**。任何后续对 `coroutine_handle` 的操作（`resume()`、`done()`、`promise()`）都是未定义行为。

**正确做法**：`final_suspend` 始终返回 `suspend_always` 或自定义 awaiter 且 `await_ready()` 为 `false`。由协程的拥有者在适当时机显式调用 `handle.destroy()`。

## Destroy 时机与帧所有权

协程帧的生命周期由谁管理：

```
协程帧所有权模型
─────────────────────────────────────────────────────
1. 编译器在协程入口分配帧，返回 return_object 给调用者
2. return_object 通常持有 coroutine_handle（非拥有型裸指针语义）
3. 谁负责 destroy？
   a. 返回对象的 RAII 析构函数（最常见）
   b. 调用者在 final_suspend 之后手动调用
   c. 对称转移链的末端调用
─────────────────────────────────────────────────────
```

RAII 包装器示例：

```cpp
template <typename T>
class OwnedTask {
public:
    using promise_type = /* ... */;
    ~OwnedTask() {
        if (handle_ && handle_.done())
            handle_.destroy();  // 仅在最终挂起后销毁
    }
    OwnedTask(OwnedTask&& o) noexcept : handle_(o.handle_) {
        o.handle_ = nullptr;
    }
    OwnedTask(const OwnedTask&) = delete;
private:
    std::coroutine_handle<promise_type> handle_{};
};
```

**销毁约束**：
- 只能在协程**挂起状态**下调用 `destroy()`
- 在协程运行中（非挂起）调用 `destroy()` 是 UB
- `destroy()` 后句柄变为空悬状态，任何操作都是 UB

## 对称转移与尾调用验证

### 问题：嵌套 resume 导致栈溢出

```
A.resume()        // 栈帧 +1
  └→ B.resume()   // 栈帧 +2
      └→ C.resume() // 栈帧 +3
          └→ ...    // N 个协程 → 栈深度 O(N)
```

### 解决：对称转移（Symmetric Transfer）

`await_suspend` 返回 `coroutine_handle<>` 时，编译器将其优化为**尾调用**（tail call），栈深度恒定：

```cpp
// await_suspend 返回 coroutine_handle → 尾调用
struct FinalAwaiter {
    std::coroutine_handle<> continuation;
    bool await_ready() noexcept { return false; }
    std::coroutine_handle<> await_suspend(
        std::coroutine_handle<>) noexcept {
        return continuation ? continuation : std::noop_coroutine();
    }
};
```

**尾调用验证**：编译器（Clang/MSVC）将对称转移编译为 `llvm.coro.resume` + `tail call` 指令。可以通过以下方式验证：

```bash
# Clang: 查看 IR 中的 tail 标记
clang++ -std=c++20 -O2 -S -emit-llvm -o - coroutine.cpp | grep "tail call"
# 应看到：
#   tail call void @llvm.coro.resume(ptr %cont)
```

**`noop_coroutine()`**：返回一个特殊的协程句柄，其 `resume()` 为空操作。用作对称转移链的终止哨兵，避免调用 `destroy()` 后返回到已销毁的帧。

### 栈深度证明

```
对称转移栈行为：
A.resume()                    // 栈帧 +1
  └→ tail call B (对称转移)   // 栈帧不变（复用 A 的栈帧）
      └→ tail call C          // 栈帧不变
          └→ tail call A.cont // 栈帧不变
              └→ return       // 栈帧 -1

结果：无论 N 个协程链多长，栈深度始终为 O(1)
```

## HALO（Heap Allocation eLision Optimization）

HALO 允许编译器将协程帧从堆分配提升到调用者的栈帧中。**这不是标准保证，而是优化**。

### HALO 生效条件

编译器需要证明：
1. 协程帧的生命周期完全嵌套在调用者的生命周期内
2. 不存在协程帧逃逸到调用者之外的路径
3. `coroutine_handle` 未被存储到调用者栈外的任何位置

```cpp
// HALO 友好：生命周期嵌套
Generator<int> range(int lo, int hi) {
    for (int i = lo; i < hi; ++i) co_yield i;
}
void use() {
    for (int v : range(0, 10)) { /* 帧可提升到此处的栈 */ }
}

// HALO 不友好：帧逃逸
std::vector<Generator<int>> gens;
void spawn() {
    gens.push_back(range(0, 100));  // 帧必须堆分配——生命周期超出函数
}
```

### 可控验证：分配计数器

用自定义 `operator new` 追踪协程帧的实际分配：

```cpp
#include <atomic>
#include <cstdio>

inline std::atomic<int> g_coroutine_allocs{0};

struct TrackedAlloc {
    struct promise_type {
        static void* operator new(std::size_t size) {
            g_coroutine_allocs.fetch_add(1, std::memory_order_relaxed);
            return ::operator new(size);
        }
        static void operator delete(void* p, std::size_t sz) noexcept {
            g_coroutine_allocs.fetch_sub(1, std::memory_order_relaxed);
            ::operator delete(p, sz);
        }
        // ... promise 成员
    };
};

// 验证
void test_halo() {
    auto before = g_coroutine_allocs.load();
    {
        auto gen = range(0, 5);  // 期望 HALO → 不触发 operator new
        for (int v : gen) { (void)v; }
    }
    auto after = g_coroutine_allocs.load();
    std::printf("coroutine allocs: %d (HALO %s)\n",
                after - before, (after == before ? "hit" : "miss"));
}
```

```bash
# 编译并验证
clang++ -std=c++20 -O2 -fcoroutines -o halo_test halo_test.cpp
./halo_test
# 期望输出: coroutine allocs: 0 (HALO hit)
```

**实际经验**：
- Clang 在 `-O1` 及以上对简单 generator 支持 HALO
- MSVC 需要 `/O2` 且协程体足够简单
- GCC 的 HALO 支持在 14.x 仍不完善
- 复杂协程（捕获 shared_ptr、嵌套 co_await）通常不会触发 HALO

## Before/After IR：简单协程的 LLVM IR

源代码：

```cpp
#include <coroutine>

struct Task {
    struct promise_type {
        Task get_return_object() {
            return {std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() { throw; }
    };
    std::coroutine_handle<promise_type> handle;
};

Task simple_coro(int x) {
    int local = x * 2;
    co_await std::suspend_always{};
    co_return;
}
```

**Lowering 后的 LLVM IR（简化核心结构）**：

```llvm
; 协程入口函数
define void @_Z11simple_coroi(i32 %x, ptr sret(%struct.Task) %result) {
entry:
  ; 1. 分配协程帧
  %frame = call ptr @llvm.coro.begin(...)
  ; 等价于 %frame = call noalias ptr @_Znwm(i64 <frame_size>)

  ; 2. 存储参数到帧
  %param_ptr = getelementptr inbounds %coroutine.Frame, ptr %frame, i32 0, i32 3
  store i32 %x, ptr %param_ptr

  ; 3. 构造 promise
  ; ... promise 初始化代码 ...

  ; 4. get_return_object
  call void @_ZN4TaskC1EPNSt16coroutine_handleINS_12promise_typeEEE(...)

  ; 5. initial_suspend
  %init = call i8 @llvm.coro.suspend(...)
  switch i8 %init, label %suspend [
    i8 0, label %body       ; 恢复后进入协程体
    i8 1, label %cleanup    ; 初始挂起
  ]

body:
  ; 6. 协程体第1段：int local = x * 2
  %x_val = load i32, ptr %param_ptr
  %local_val = mul i32 %x_val, 2
  %local_ptr = getelementptr inbounds %coroutine.Frame, ptr %frame, i32 0, i32 4
  store i32 %local_val, ptr %local_ptr

  ; 7. co_await std::suspend_always{} → 无条件挂起
  %suspend1 = call i8 @llvm.coro.suspend(...)
  switch i8 %suspend1, label %suspend [
    i8 0, label %resume1
    i8 1, label %cleanup
  ]

resume1:
  ; 8. co_return → 跳转到 final_suspend
  br label %final

final:
  ; 9. final_suspend
  %suspend_final = call i8 @llvm.coro.suspend(...)
  switch i8 %suspend_final, label %suspend [
    i8 0, label %unreachable
    i8 1, label %cleanup
  ]

cleanup:
  ; 10. 帧的销毁（仅在 destroy() 时执行）
  call i1 @llvm.coro.end(ptr %frame, i1 false, ...)
  br label %suspend

suspend:
  ret void

unreachable:
  unreachable
}
```

**关键 LLVM 内联函数（intrinsic）**：

| Intrinsic | 作用 |
|-----------|------|
| `@llvm.coro.begin` | 开始协程，完成帧分配 |
| `@llvm.coro.suspend` | 标记挂起点，返回 0（恢复）/1（销毁）/2（挂起） |
| `@llvm.coro.end` | 标记协程结束 |
| `@llvm.coro.id` | 协程标识，包含 promise 和分配元数据 |
| `@llvm.coro.destroy` | 销毁协程帧 |
| `@llvm.coro.resume` | 恢复协程执行 |
| `@llvm.coro.done` | 检查是否在 final_suspend |

## 总结

```
协程 Lowering 全流程
──────────────────────────────────────────
源代码: co_await / co_yield / co_return
    ↓
编译器: 识别为协程 → 查找 coroutine_traits
    ↓
提取:   promise_type → 帧大小计算 → 状态机生成
    ↓
入口:   operator new → 初始化帧 → get_return_object
    ↓
初始:   initial_suspend → await_ready? → 挂起/继续
    ↓
循环:   switch(suspend_index) → 协程体段 → co_await → 挂起
    ↓
结束:   return_value/void → final_suspend → 挂起
    ↓
销毁:   handle.destroy() → 帧内成员逆序析构 → operator delete
──────────────────────────────────────────
```

掌握这些内部机制后，实现自定义 promise 类型时可以：
- 精确控制帧分配（自定义 `operator new`）
- 通过 `await_transform` 统一 `co_await` 语义
- 验证 HALO 优化是否生效
- 确保对称转移避免栈溢出
- 正确处理异常传播链
