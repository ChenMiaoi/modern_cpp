---
title: Folly Future/Promise
topic: libraries
feature: future-promise
standard: N/A
status_checked_at: 2026-06-02
implementation:
  folly:
    paths:
      - references/impl/folly/folly/futures/Future.h
      - references/impl/folly/folly/futures/Promise.h
      - references/impl/folly/folly/futures/Core.h
    symbols:
      - Core
      - Future
      - SemiFuture
      - Promise
      - Try
exercises: []
solutions: []
---
# Folly Future/Promise：Core + Executor 异步模型

> 源码路径：`references/impl/folly/folly/futures/Future.h`, `Promise.h`, `Core.h`

## Core 共享状态

```
Promise<T>  ──写入──→  Core<T>  ←──读取──  Future<T>
                         │
                    Try<T> result       (值或异常)
                    callback            (continuation)
                    Executor*           (调度器)
```

```cpp
template <typename T>
struct Core {
  Try<T> result_;                    // 值或异常
  std::function<void(Try<T>&&)> callback_;
  Executor* executor_;
  std::atomic<State> state_;         // 四状态机
  std::shared_ptr<RequestContext> context_;
};
```

## 四状态机

```
Core<T> 状态机完整转换图：

                         Promise::setValue()
                    ┌─────────────────────────────────┐
                    │                                 ▼
              ┌─────────┐   Future::thenValue()  ┌──────────────┐
              │         │ ─────────────────────→ │              │
              │  Start  │                        │ OnlyCallback │
              │         │ ─────────────────────→ │              │
              └─────────┘                        └──────┬───────┘
                    │    Promise::setValue()              │
                    │                                 ▼
                    │                           ┌──────────────┐
                    │                           │     Done     │ ← callback(result)
                    │                           │   (terminal) │
                    │                           └──────────────┘
                    │                                 ▲
              ┌──────────────┐                        │
              │              │   Future::thenValue()  │
              │  OnlyResult  │ ─────────────────────→ ┘
              │              │
              └──────────────┘

路径 1（Promise 先设值）：Start → OnlyResult → Done
路径 2（Future 先注册 callback）：Start → OnlyCallback → Done

无论哪方先到，callback 都保证被触发——状态机消除竞态
```

## SemiFuture vs Future

```cpp
// SemiFuture：未绑定 Executor，不能调用 .then()
SemiFuture<int> sf = makeSemiFuture(42);
Future<int> f = std::move(sf).via(&executor);  // 必须显式选择 Executor

// Future：已绑定 Executor，可链式 continuation
auto result = std::move(f)
    .thenValue([](int v) { return v * 2; })
    .thenValue([](int v) { return v + 1; });
```

**为什么区分？** SemiFuture 防止用户意外在错误的 Executor 上执行 continuation。必须显式选择 Executor。

## Executor 抽象

```cpp
class Executor {
public:
  virtual void add(Func&& f) = 0;  // 提交任务到执行器
};

// 典型实现：
// - CPUThreadPoolExecutor：线程池
// - IOThreadPoolExecutor：I/O 线程池
// - InlineExecutor：在当前线程内执行（测试用）
```

## 与 std::future 的对比

| 维度 | Folly Future | std::future |
|------|-------------|------------|
| Continuation | **.thenValue() 链式** | 无（必须阻塞 get()） |
| 异常传播 | 自动（Try\<T\> 包裹） | get() 时 rethrow |
| Executor | **显式绑定** | 无 |
| SemiFuture | **防止误用** | 无 |
| 性能 | 避免 shared_state 原子操作 | 引用计数 shared_state |

## 用户 API

用户通常通过 `Promise<T>`、`SemiFuture<T>`、`Future<T>` 与 `.via()/.thenValue()` 链式接口接触这套模型；现有正文已经直接进入 `Core<T>` 共享状态。

## 标准语义

### 与 `std::future`/`std::promise` 的核心语义差异

| 维度 | `std::future` / `std::promise` | Folly Future / Promise |
|------|-------------------------------|----------------------|
| **continuation 模型** | 无原生 continuation；只能阻塞 `.get()` 或轮询 `.wait_for()` | `.thenValue()` / `.thenTry()` 链式 monadic 组合，结果就绪时由 executor 异步触发 callback |
| **executor 绑定** | 隐式——shared_state 由标准库管理，用户无法控制 continuation 在哪个线程执行 | **显式**：`SemiFuture` 本身无 executor，必须通过 `.via(executor)` 转为 `Future` 后才能挂 continuation；executor 决定 callback 的执行上下文 |
| **lazy vs eager** | `std::async` 默认 eager（立即启动），`std::promise` 手动 fulfill | `SemiFuture`/`defer` 系列 lazy——continuation 在 executor 设置后才可能调度；`Future` 通过 `.via()` 显式选择调度时机 |
| **异常传播** | 存入 shared_state，`.get()` 时 rethrow；若 future 析构前未 get 则异常被静默丢弃 | `Try<T>` 统一封装值/异常；异常沿 continuation 链自动传播，未捕获异常最终到达链尾；`Promise` 析构时若未 fulfill 则 future 收到 `BrokenPromise` |
| **SemiFuture / Future 分离** | 无此概念 | `SemiFuture`（无 executor，支持 `defer*`）→ `Future`（有 executor，支持 `then*`）。防止用户意外在错误的 executor 上执行 continuation |
| **取消/中断** | `std::stop_token`（C++20）仅通知机制 | `future.raise(exception)` → `promise.setInterruptHandler(fn)` 双向通道；`cancel()` 是 `raise(FutureCancellation())` 的便捷方法 |
| **共享语义** | `std::shared_future` 允许多个消费者 | Folly Future **不可复制**；一个 `Promise` 只能产生一个 `SemiFuture`/`Future`，保证单消费者模型的正确性 |
| **void 表示** | `std::future<void>` / `std::promise<void>` 合法 | `void` 不合法；用 `folly::Unit` 代替（`Future<Unit>` / `Promise<Unit>`） |

### executor 绑定的关键语义

1. **`.via()` 是单向门**：`SemiFuture::via(exec)` 返回 `Future`，原 `SemiFuture` 失效（`valid() == false`）。后续所有 `.thenValue()` continuation 都在该 executor 上执行。
2. **executor 可切换**：在 `Future` 链中再次调用 `.via(exec2)` 可切换后续 continuation 的执行上下文，类似 Unix pipe 中的 `|`。
3. **inline 优化**：若前一个 callback 的完成 executor 与下一个 continuation 的目标 executor 相同，Folly 可以 inline 执行（`thenValueInline` / `OnlyCallbackAllowInline` 状态），避免一次 executor 调度开销。
4. **`defer*` vs `then*`**：`SemiFuture::deferValue()` 的 continuation 延迟到消费者调用 `.get()` 或设置 executor 时才执行，不强制绑定 executor；`Future::thenValue()` 要求已有 executor。

## 对象布局

上文已经给出 `Core<T>` 的关键字段；后续补 `Promise`/`Future` 句柄与共享 `Core` 之间的所有权关系图。

## 核心源码路径

本文开头已给出 `Future.h`、`Promise.h`、`Core.h`；后续补 continuation 注册到 executor 调度的入口链。

### `Try<T>` — 值/异常统一封装

```cpp
template <typename T>
class Try {
  union { T value_; exception_wrapper exception_; };
  enum { EMPTY, VALUE, EXCEPTION } state_;
};
```

- `Try<T>` 是 `Core<T>` 的 result 类型，始终三态之一：**空**（未设置）、**持有值**、**持有异常**。
- `void` 特化不存在；`Try<Unit>` 表示无值完成。
- 提供 `hasValue()` / `hasException()` / `value()` / `exception()` 访问器；调用 `value()` 若当前为异常则 rethrow。
- `makeTryWith(func)` 捕获 `func()` 的返回值或抛出的异常构造 `Try`。

### `Core<T>` — 共享状态核心

```cpp
// CoreBase: T-independent 部分（减少模板实例化开销）
class CoreBase {
  Callback callback_;                      // folly::Function<void(CoreBase&, KeepAlive<>&&, exception_wrapper*)>
  std::atomic<State> state_;               // 六态 FSM
  std::atomic<unsigned char> attached_;    // 引用计数（Promise + Future 各一位）
  std::atomic<unsigned char> callbackReferences_;
  KeepAliveOrDeferred executor_;           // tagged union: KeepAlive | DeferredExecutor
  Context context_;                        // shared_ptr<RequestContext>
  std::atomic<uintptr_t> interrupt_;       // 中断 FSM（低位编码状态，高位存指针）
  CoreBase* proxy_;                        // proxy 链
};

template <typename T>
class Core final : private ResultHolder<T>, public CoreBase {
  // ResultHolder<T> 提供 union { Try<T> result_; }，控制布局
  // 确保 result_ 与 vtable 指针、callback_ 在同一缓存行
};
```

关键工厂方法：
- `Core::make()` → 初始状态 `Start`
- `Core::make(Try<T>&&)` → 初始状态 `OnlyResult`（值已就绪）
- `Core::make(in_place, args...)` → 原地构造 result

### `Promise<T>` — 生产端句柄

```cpp
template <class T>
class Promise {
  Core* core_;        // 指向共享 Core
  bool retrieved_;    // getSemiFuture/getFuture 是否已调用
};
```

核心方法：
- `getSemiFuture()` → 返回 `SemiFuture<T>`，共享同一 `Core`；只能调用一次。
- `setValue(v)` / `setException(ew)` / `setTry(Try<T>&&)` → fulfill promise，触发 FSM 转移。
- `setInterruptHandler(fn)` → 注册中断回调，与 `future.raise()` 配合。
- 析构时若 `valid() && !isFulfilled()`，自动以 `BrokenPromise` 异常 fulfill 关联 future。

### `SemiFuture<T>` — 消费端（无 executor）

- 继承 `FutureBase<T>`，不持有 executor。
- 核心方法：`deferValue(f)` / `defer(f)` / `deferError(f)` / `deferEnsure(f)` — 注册延迟 continuation。
- `.via(executor)` → 转为 `Future<T>`，绑定 executor。
- `.get()` → 阻塞当前线程等待结果。
- `.wait()` → 阻塞等待，但不移动结果。

### `Future<T>` — 消费端（有 executor）

- 继承 `FutureBase<T>`，持有 executor。
- 核心方法：`thenValue(f)` / `thenTry(f)` / `thenError(tag, f)` / `ensure(f)`。
- `.then(f)` 是 `.thenTry(f)` 的别名。
- 每个 `.then*()` 返回新的 `Future<R>`（monadic map），原 `Future` 失效。
- 若 continuation 返回 `Future<R>`，自动 unwrap（flatMap 语义）。

### `.via()` — executor 绑定

```cpp
// SemiFuture → Future
Future<T> SemiFuture<T>::via(Executor::KeepAlive<> executor) &&;
// Future → Future（切换 executor）
Future<T> Future<T>::via(Executor::KeepAlive<> executor) &&;
```

`.via()` 的作用：
1. 将 executor 写入 `Core::executor_`。
2. 如果 result 已就绪（`OnlyResult`），立即通过 executor 调度 callback。
3. 如果 callback 已注册（`OnlyCallback`），等待 result 到达后在新 executor 上调度。

### `.thenValue()` — continuation 注册

```cpp
template <typename F>
Future<R> Future<T>::thenValue(F&& func) &&;
```

1. 从当前 `Core`（旧）创建新 `Core<R>`（新），新 Core 的 executor 继承自旧 Core。
2. 在旧 Core 上安装 callback：收到 `Try<T>` 后，提取值传递给 `func`，结果写入新 Core。
3. 若旧 Core 已是 `OnlyResult`，callback 立即执行。
4. 旧 `Future` 的 `core_` 被置空（`valid() == false`），新 `Future` 持有新 Core。

## 关键算法

### 核心路径一：`setValue` / `setResult` — 生产者写入结果

```
Promise::setValue(v)
  → Core::setResult(Try<T>(std::move(v)))
    → placement-new Try<T> 到 Core::result_
    → Core::setResult_(completingKA)
      → state_.compare_exchange_strong(expected=Start, desired=OnlyResult)
        如果成功（Start → OnlyResult）：
          result 已存储，无 callback → 终态，等待 callback 到达
        如果 expected 是 OnlyCallback 或 OnlyCallbackAllowInline：
          → state = Done
          → doCallback()：在 executor 上调度 callback(result)
        如果 expected 是 Proxy：
          → proxyCallback()：将 result 转发到 proxy Core
```

关键点：`setResult` 是一次性操作，调用两次触发 undefined behavior（源码注释明确标注）。

### 核心路径二：continuation 安装 — 消费者注册 callback

```
Future::thenValue(func)
  → 创建新 Core<R>（状态 Start）
  → 构造 callback lambda：
      收到 (Try<T>&&) → 提取值 → 调用 func(value) → 写入新 Core<R>
  → 旧 Core::setCallback(callback, context, allowInline)
    → state_.compare_exchange_strong(expected=Start, desired=OnlyCallback)
      如果成功（Start → OnlyCallback）：
        callback 已存储，等待 result
      如果 expected 是 OnlyResult：
        → state = Done
        → doCallback()：立即执行 callback(result)
      如果 expected 是 Proxy：
        → 通过 proxy 链传递 callback
  → 返回 Future<R>(新 Core)
```

### 核心路径三：executor 提交 — doCallback 调度

```
Core::doCallback(completingKA, priorState)
  → 检查 completingKA 与 executor_ 是否为同一 executor
    → 如果相同 且 priorState == OnlyCallbackAllowInline：
        inline 执行 callback（避免 executor hop）
    → 否则：
        executor_->add([callback, result]() { callback(result); })
        → executor 负责在适当线程执行
```

### 核心路径四：异常传播

```
Promise::setException(ew)
  → setTry(Try<T>(exception_wrapper(ew)))
    → 正常 setValue 路径，但 Try 持有异常

continuation 内部抛异常：
  → callback 中 func(value) 抛出 E
  → catch → Try<R>(exception_wrapper(E)) 写入新 Core<R>
  → 异常自动沿链传播，直到被 thenError() 捕获或到达链尾
```

### Proxy 链机制

```
Core A (Proxy) ──proxy_──→ Core B (实际状态)

当 A 的 result 被设置为 Proxy 状态时：
- 后续对 A 的 callback 操作会通过 walkProxyChain() 透传到 B
- A 的 doCallback 将 callback 转发到 B 执行
- 用于 Future 分裂（split）和 FutureSplitter 场景
```

## ABI 约束

### 模板头文件内联模型

Folly futures 的核心类型（`Core<T>`、`Future<T>`、`SemiFuture<T>`、`Promise<T>`、`Try<T>`）**全部以模板头文件实现**，无独立编译的 `.so`/`.dll` 导出符号（`Core<Unit>` 有 `extern template` 限制实例化，见 `FOLLY_USE_EXTERN_FUTURE_UNIT`）。

**含义：**
- 不存在跨 DSO 的 ABI 边界——每个链接单元独立实例化所有模板。
- **ABI 不稳定性等同于 API 不稳定性**：字段重排、`State` 枚举值变更、`Callback` 签名变更、`KeepAliveOrDeferred` 布局变更都会导致 ODR 违反和静默数据损坏。
- 升级 Folly 版本后必须全量重编译所有使用 futures 的目标文件。

### 布局敏感的关键类型

| 类型 | 布局风险 |
|------|---------|
| `State` 枚举 | `uint8_t` 位掩码，值硬编码在 FSM 的 CAS 操作中。新增/重排枚举值改变比较逻辑 |
| `CoreBase::interrupt_` | `uintptr_t` 低位 2 bit 编码状态机（`InterruptMask = 0x3`），高位存指针。指针对齐假设平台特定 |
| `KeepAliveOrDeferred` | tagged union（`State::Deferred` / `State::KeepAlive`），手动管理 union 生命周期 |
| `ResultHolder<T>` | `union { Try<T> result_; }` 用于控制构造时机；与 vtable 指针共享缓存行 |
| `Callback`（即 `folly::Function<...>`） | `folly::Function` 有自己的 inline buffer 大小和 SBO 阈值 |

### `Core<Unit>` 的 extern template

```cpp
#if FOLLY_USE_EXTERN_FUTURE_UNIT
extern template class Core<folly::Unit>;
#endif
```

`Core<Unit>` 是唯一有 `extern template` 声明的实例化。这限制了 `Unit` 特化的显式实例化到 `Core.cpp` 编译单元，减少二进制体积。但这仅是编译优化，不提供 ABI 稳定性保证。

### 与 `std::future` ABI 的对比

`std::future`/`std::promise` 在 libstdc++/libc++ 中有稳定的 shared_state 布局（通常位于 `<bits/shared_ptr_base.h>` 或 `<__future>` 内），跨 DSO 使用时由标准库 .so 保证 ABI 兼容。Folly 无此保证——跨进程共享 `Core` 指针（或序列化 `Future`）是不可能的。

## 异常安全

### `Try<T>` 异常封装

`Try<T>` 是 Folly futures 异常安全的基石。它将 `T` 值和 `exception_wrapper` 封装在同一 union 中：

- `Promise::setException(ew)` → 构造 `Try<T>(std::move(ew))` → 写入 `Core::result_`。
- continuation 通过 `Try<T>&&` 接收结果，可安全检查 `hasValue()` / `hasException()` 后再访问值。
- `Try::value()` 在异常状态下直接 rethrow 存储的异常，不会返回垃圾值。

### continuation 抛异常的传播

```cpp
auto f = std::move(future)
    .thenValue([](int v) { throw std::runtime_error("oops"); return v; })
    .thenValue([](int v) { /* 永远不会执行 */ return v; })
    .thenError([](exception_wrapper ew) { /* 捕获 "oops" */ return 0; });
```

规则：
1. `.thenValue(f)` 中 `f` 抛出的异常被 Folly 内部 catch，包装为 `Try<R>(exception_wrapper)` 写入新 Core。
2. 异常沿 continuation 链自动传播，跳过后续 `.thenValue()`。
3. 只有 `.thenError()` / `.deferError()` 可以捕获并恢复。
4. 若链尾未捕获，异常在 `.get()` 时被 rethrow，或在 detached future 中被静默丢弃。

**与 `std::future` 的差异**：`std::future` 的异常只在 `.get()` 时 rethrow 一次；Folly 的异常在链中持续传播，每一步都可被拦截。

### Promise 破约（Broken Promise）

```cpp
{
  folly::Promise<int> p;
  auto f = p.getSemiFuture();
  // p 析构，未调用 setValue/setException
}
// f.get() 抛出 BrokenPromise 异常
```

`Promise::~Promise()` 的行为：
1. 如果 `valid() && !isFulfilled()`，自动调用 `setException(BrokenPromise(tag_t<T>{}))`。
2. `BrokenPromise` 继承自 `PromiseException`（`std::logic_error`），携带类型名信息。
3. 这保证了 future 链不会永远挂起——即使生产者忘记 fulfill，消费者也能得到明确的错误信号。

### Promise 违规的异常类型

| 操作 | 前置条件违反 | 抛出异常 |
|------|-------------|---------|
| `setValue()` / `setException()` / `setTry()` | `!valid()` | `PromiseInvalid` |
| `setValue()` / `setException()` / `setTry()` | `isFulfilled()` | `PromiseAlreadySatisfied` |
| `getSemiFuture()` / `getFuture()` | `!valid()` | `PromiseInvalid` |
| `getSemiFuture()` / `getFuture()` | 已调用过 | `FutureAlreadyRetrieved` |
| `Future::then*()` / `SemiFuture::defer*()` | `!valid()` | `FutureInvalid` |
| `Future::then*()` | 已挂 continuation | `FutureAlreadyContinued` |

所有这些异常继承自 `FutureException` 或 `PromiseException`（均为 `std::logic_error`），表明是编程错误而非运行时故障。

### executor 提交失败

executor 的 `add(Func&&)` 可能抛异常（例如队列满、executor 已关闭）。此时：

1. 异常会冒泡到 `doCallback()` 内部。
2. Folly 会将该异常捕获并作为 continuation 的结果写入下一个 Core。
3. 如果 executor 已关闭（例如 `CPUThreadPoolExecutor::join()` 之后调用 `add()`），行为取决于 executor 实现——通常抛 `std::runtime_error`。

**注意**：`InlineExecutor` 不会抛此类异常（始终在当前线程同步执行），但可能导致栈深度无限增长（每个 continuation inline 执行触发下一个）。

### interrupt 处理的异常安全

`future.raise(ew)` 和 `promise.setInterruptHandler(fn)` 共享一个原子状态机（`interrupt_` 字段的低 2 bit）：

- `raise()` 在 `setInterruptHandler()` 之前调用：中断对象被存储，等 handler 到达时调用。
- `setInterruptHandler()` 在 `raise()` 之前调用：handler 被存储，等 `raise()` 到达时同步调用。
- 两者同时到达：由 CAS 仲裁，保证恰好调用一次 handler。
- `setInterruptHandler()` 调用两次会触发 `terminate_with<logic_error>`（不可恢复的编程错误）。

## iterator / reference invalidation

### Future/SemiFuture 句柄有效性

Folly Future 遵循严格的**移动语义**：

- **不可复制**：`Future<T>` 和 `SemiFuture<T>` 均 delete 了拷贝构造/赋值。
- **移动后失效**：任何 `.then*()` / `.defer*()` / `.via()` / `.get()` 调用都以 `&&` 限定符消耗 `this`。调用后 `valid() == false`，对移动后对象的任何操作抛 `FutureInvalid`。
- **`SemiFuture` → `Future` 隐式移动**：`SemiFuture(Future<T>&&)` 允许隐式转换，`Future` 失效。

```cpp
auto f1 = std::move(p).getSemiFuture();
auto f2 = std::move(f1).via(&exec);  // f1 失效
auto f3 = std::move(f2).thenValue([](int v) { return v + 1; });  // f2 失效
// f3 有效，持有新 Core
```

### `Try<T>&` 引用的生命周期

`future.result()` / `future.value()` 返回对 `Core::result_` 的引用。引用的有效期受以下约束：

1. **`OnlyResult` 状态**：引用始终有效（producer 已写入，consumer 独占）。
2. **`Done` 状态**：引用可能失效——callback 可能已经 move-out 了 result。不能假设引用仍指向有效值。
3. **`poll()` 移动 result**：`poll()` 返回 `Optional<Try<T>>`，内部 move 出 result。之后 `result()` / `value()` 引用指向被移走的对象。

### continuation lambda 捕获对象的生命周期

```cpp
std::string s = "hello";
auto f = std::move(future).thenValue([s = std::move(s)](int v) {
  return v + s.size();
});
// s 已被移动，原变量失效
```

continuation lambda 被存储在 `Core::callback_`（`folly::Function`）中，其生命周期与 Core 绑定：
- 在 `Done` 状态转换时被调用。
- 调用后 `callback_` 被析构（`derefCallback()`）。
- 如果 future 链被 destroy 但 callback 尚未执行（例如 detached future 且 executor 未调度），callback 和其捕获的对象随 Core 一起析构。

### `Core*` 指针的有效性

`Core` 的生命周期由引用计数（`attached_` 字段）控制：

- `Promise` 析构 → `detachPromise()` → `detachOne()`
- `Future`/`SemiFuture` 析构 → `detachFuture()` → `detachOne()`
- 当 `attached_` 归零 → `delete this` → `~Core()` → `~ResultHolder()` → `result_.~Try<T>()`

`Core` 不可移动也不可复制（构造函数 private，`delete` 了移动/拷贝）。所有对 Core 的访问通过裸指针 `Core*`，指针有效性由析构顺序保证。

### proxy 链中的引用

当 Future 被 split 时，多个 `Core` 可以通过 `proxy_` 指针形成链。proxy 链上的 Core 可能比原始 Core 更早析构，但 `walkProxyChain()` 只在 `hasResult()` 之后调用，此时所有中间 proxy 已完成转换，不会出现悬垂指针。

## 性能模型

### 内存分配模型

每次 `Promise` 构造（或 `makePromiseContract()`）分配一个 `Core<T>`：

```
Core<T> 内存布局（典型 x86-64）：
┌─────────────────────────────────┐
│ vptr (8B)                       │ ← ResultHolder<T> + CoreBase
│ callback_ (Function, ~32-64B)   │ ← folly::Function 内联 buffer
│ state_ (1B atomic)              │
│ attached_ (1B atomic)           │
│ callbackReferences_ (1B atomic) │
│ [padding 5B]                    │
│ executor_ (KeepAliveOrDeferred) │ ← ~16B tagged union
│ context_ (shared_ptr, 16B)      │
│ interrupt_ (8B atomic)          │
│ proxy_ (8B)                     │
│ result_ (Try<T>, 变长)          │ ← 同一缓存行（若 T 较小）
└─────────────────────────────────┘
```

`Core<T>` 通过 `new` 分配，`Core<T>::make()` 是唯一入口。一次 Promise-Future 对的创建 = **一次堆分配**。

`Core<Unit>` 使用 `extern template`（`FOLLY_USE_EXTERN_FUTURE_UNIT`）减少模板膨胀。

### 状态机原子同步成本

| 操作 | 原子指令 | 内存序 |
|------|---------|--------|
| `setResult_()` | `CAS(state_, Start→OnlyResult)` | `acq_rel` |
| `setCallback_()` | `CAS(state_, Start→OnlyCallback)` | `acq_rel` |
| `doCallback()` | `CAS(state_, OnlyCallback→Done)` | `acq_rel` |
| `hasResult()` | `load(state_)` | `acquire` |
| `raise()` / `setInterruptHandler()` | `CAS(interrupt_, ...)` | `acq_rel` |

每个 Core 状态转换恰好一次 CAS（无 spin loop）。若 CAS 失败（另一方已先到），直接走对方先到的分支，无重试。

**最坏情况**：每次 continuation 注册 + fulfill 涉及 2 次 CAS（一方 setCallback，一方 setResult，各一次 CAS 一次失败后走 fallthrough）。

### Executor hop 成本

```
链长度 N 的 continuation 链：
  auto f = std::move(f0)
    .thenValue(f1)    // Core 0 → Core 1
    .thenValue(f2)    // Core 1 → Core 2
    ...
    .thenValue(fN);   // Core N-1 → Core N

executor hop 数 = N（每次 thenValue 可能触发一次 executor->add()）
```

**inline 优化**：当 `thenValueInline` / `thenTryInline` 使用时，若前一个 callback 的完成 executor 与下一个 Core 的 executor 相同，跳过 executor 调度直接 inline 执行。这将 executor hop 数从 N 降为 executor 切换次数。

`OnlyCallbackAllowInline` 状态专门支持此优化：`setCallback_` 传入 `InlineContinuation::allow` 时，若 result 已就绪且完成 executor 匹配，直接 inline 而非 `executor->add()`。

### 与 `std::future` 的性能对比

| 维度 | `std::future` (libstdc++) | Folly Future |
|------|--------------------------|-------------|
| 共享状态分配 | `shared_ptr<_Task_state>`，一次堆分配 | `Core<T>*`，一次堆分配 |
| 同步原语 | `mutex` + `condition_variable`（`.wait()` 路径） | 无 mutex，纯 `atomic<CAS>` |
| continuation | 无原生支持 | 每个 Core 一次 CAS |
| 异常存储 | `exception_ptr`（引用计数） | `exception_wrapper`（引用计数 + 类型擦除） |
| 阻塞等待 | `condition_variable::wait()` → futex | `Baton::wait()` → futex（更轻量） |

Folly 的优势在于**全无锁状态机**——`std::future` 的 `.wait()` 需要 mutex 保护条件变量，而 Folly 的 continuation 模型完全避免了 mutex。

### `folly::Function` 的 SBO

`Core::callback_` 类型是 `folly::Function<void(CoreBase&, KeepAlive<>&&, exception_wrapper*)>`。`folly::Function` 使用小对象优化（SBO），将小 lambda（通常 ≤ 32 字节，平台相关）存储在栈上内联 buffer 中，避免额外堆分配。大多数 continuation lambda 足够小，不会触发二次分配。

## libstdc++ vs libc++ vs MSVC

Folly futures 的行为与具体平台标准库实现无关（它是独立库），但与三家标准库的 `std::future`/`std::promise` 有以下对照：

### continuation 模型差异

| 维度 | libstdc++ (`std::future`) | libc++ (`std::future`) | MSVC (`std::future`) | Folly Future |
|------|--------------------------|----------------------|---------------------|-------------|
| continuation API | 无 | 无 | 无（C++11 起无 `.then()`） | `.thenValue()` / `.thenTry()` / `.deferValue()` |
| executor 支持 | 无 | 无 | 无 | 显式 `Executor` 抽象，`.via()` 绑定 |
| lazy deferral | 不支持 | 不支持 | 不支持 | `SemiFuture::defer*()` 延迟到 `.get()` 或 executor 设置 |
| exception 沿链传播 | N/A | N/A | N/A | 自动通过 `Try<T>` 沿链传播 |

### 异常传递实现差异

| 维度 | libstdc++ | libc++ | MSVC | Folly |
|------|-----------|--------|------|-------|
| 异常存储 | `exception_ptr`（`shared_ptr<__exception_ptr::exception_ptr>`） | `exception_ptr`（`shared_ptr` 类似实现） | `exception_ptr`（内部 SEH 集成） | `exception_wrapper`（引用计数 + `type_info` 擦除，可携带任意类型） |
| 传递方式 | `.get()` 时 `rethrow` | `.get()` 时 `rethrow` | `.get()` 时 `rethrow` | `Try<T>` 在 Core 间 move 传递，每步可拦截 |
| 丢失行为 | future 析构时异常被静默丢弃 | 同左 | 同左 | `BrokenPromise` 异常保证不会静默丢失 |

### 调度语义差异

| 维度 | libstdc++ | libc++ | MSVC | Folly |
|------|-----------|--------|------|-------|
| `.wait()` 实现 | `condition_variable` + mutex | `condition_variable` + mutex | `condition_variable` + mutex | `folly::Baton`（基于 futex，无 mutex） |
| `std::async` 线程策略 | `launch::async | launch::deferred`（实现各异） | 类似 libstdc++ | 类似 libstdc++ | 无隐式线程创建；executor 显式控制 |
| shared_state 生命周期 | `shared_ptr` 引用计数 | `shared_ptr` 引用计数 | `shared_ptr` 引用计数 | 裸指针 + 位计数（`attached_`），无 `shared_ptr` 开销 |

### 平台特定注意

- **libstdc++**：`std::promise` 的 shared_state 在 `libstdc++.so` 内实现，ABI 稳定。Folly 无此保证。
- **libc++**：`exception_ptr` 基于 `__cxa_current_exception`，与 Folly 的 `exception_wrapper`（基于 `folly::exception_tracer`）不兼容。
- **MSVC**：`exception_ptr` 集成 Windows SEH；Folly 在 MSVC 上的 `exception_wrapper` 使用不同的底层机制。`CoreBase::interrupt_` 的指针低位编码依赖 8 字节对齐，在 MSVC 64-bit 下成立。
- **Folly 跨平台**：`folly::Baton` 在 Linux 上用 futex，在 macOS 用 `ulock_wait`/`__ulock_wait`，在 Windows 用 `WaitOnAddress`。executor hop 成本在所有平台上一致（都是 `add(Func&&)` 虚调用）。

## 最小复现代码

```cpp
#include <folly/futures/Future.h>
#include <folly/futures/Promise.h>

int main() {
  folly::Promise<int> p;
  auto f = p.getSemiFuture().deferValue([](int x) { return x + 1; });
  p.setValue(41);
  return std::move(f).get();
}
```

## 编译 / 反汇编 / benchmark 证据

### Core 状态转移的 CAS 反汇编

以 `CoreBase::setResult_()` 为例，核心 CAS 在 x86-64 上编译为单条 `lock cmpxchg`：

```asm
; folly::futures::detail::CoreBase::setResult_(folly::Executor::KeepAlive<>&&)
; state_ 是 CoreBase+偏移处的 std::atomic<State>
mov     eax, 1          ; State::Start (0x1)
mov     ecx, 2          ; State::OnlyResult (0x2)
lock cmpxchg [rdi+OFF], cl  ; CAS(state_, Start→OnlyResult)
jne     .slow_path      ; 失败 → 检查是 OnlyCallback 还是 Proxy
; 快速路径：Start → OnlyResult，直接返回
ret
.slow_path:
; 失败分支：对方先到，走 doCallback 或 proxyCallback
```

关键特征：
- **无 mutex、无 futex**：纯 `lock cmpxchg`，单条指令完成状态转换。
- **内存序 `acq_rel`**：编译为带 `lock` 前缀的 CAS（x86 上 `lock` 隐含 full barrier）。
- **失败不重试**：CAS 失败后直接进入 fallthrough 分支（`OnlyCallback` → `Done`），无 spin loop。

### continuation 提交的路径

`doCallback()` 中 executor 提交的简化反汇编：

```asm
; 检查是否 inline 执行
cmp     byte [rdi+OFF_STATE], 3  ; OnlyCallbackAllowInline?
jne     .enqueue
; 检查 completing executor == stored executor
cmp     rsi, [rdi+OFF_EXECUTOR]
je      .inline_execute
.enqueue:
; executor_->add(std::move(callback))
mov     rax, [rdi+OFF_EXECUTOR]   ; 加载 executor vptr
call    [rax+VTABLE_ADD_OFFSET]   ; 虚调用 add(Func&&)
ret
.inline_execute:
; 直接调用 callback，不经过 executor
call    callback_func
ret
```

### `std::future` 阻塞路径对比

libstdc++ `std::future::wait()` 的典型实现：

```asm
; std::future::wait() — 需要 mutex + condition_variable
lea     rdi, [rsp+MUTEX_OFF]
call    pthread_mutex_lock       ; 1. 加锁
; 检查 ready flag
cmp     byte [rsp+READY_OFF], 0
jne     .ready
.wait:
lea     rdi, [rsp+CV_OFF]
lea     rsi, [rsp+MUTEX_OFF]
call    pthread_cond_wait        ; 2. 阻塞等待（futex 内部）
cmp     byte [rsp+READY_OFF], 0
je      .wait
.ready:
lea     rdi, [rsp+MUTEX_OFF]
call    pthread_mutex_unlock     ; 3. 解锁
```

Folly `SemiFuture::wait()` 使用 `folly::Baton`：

```asm
; folly::Baton::wait() — 无 mutex
mov     eax, [rdi]              ; 加载 atomic state
test    eax, eax
jnz     .ready
; Linux: 直接 futex 系统调用
mov     eax, 202                ; __NR_futex
xor     r10, r10               ; timeout = NULL
syscall
.ready:
ret
```

### Benchmark 量级参考

基于 folly 源码自带的 `futures/benchmarks/` 测试（典型 x86-64 / Linux 5.x / GCC 12）：

| 操作 | 典型耗时 | 说明 |
|------|---------|------|
| `Promise` + `SemiFuture` 创建 | ~50-80 ns | 一次 `Core<T>` 堆分配 + 原子初始化 |
| `setValue()`（无 callback） | ~10-15 ns | 单次 CAS + placement new |
| `thenValue()` 注册 + fulfill | ~80-150 ns | Core 分配 + CAS + executor->add() |
| inline continuation 链（同 executor） | ~30-50 ns/步 | 无 executor hop，纯 CAS + 函数调用 |
| `std::promise` + `std::future` 创建 | ~40-60 ns | `shared_ptr` 分配 |
| `std::future::get()` 阻塞 | ~200-500 ns | mutex lock + cond_wait + unlock |

**要点**：
- Folly continuation 链（同 executor inline）比 `std::future` 阻塞 `.get()` 快约 3-10x。
- 主要开销来自堆分配 `Core<T>`（~50ns）和 executor 调度（~30-80ns）。
- 对于 T 为小型 POD（如 `int`），`Try<T>` 的 value 存储零开销（union 内直接存放）。

### 编译期保证

```cpp
// Future 不可复制——编译期拦截
Future<int> f1 = makeFuture(42);
Future<int> f2 = f1;  // CE: copy constructor is deleted

// .then*() 消费 Future——编译期拦截
auto f3 = f1.thenValue([](int v) { return v + 1; });
// CE: f1 是左值，thenValue 要求 &&

// void 特化被 static_assert 拦截
// Core<void> 编译失败: "void futures are not supported. Use Unit instead."
```

这些约束在编译期通过 `= delete`、`&&` 限定符和 `static_assert` 强制执行，运行时无检查开销。

## cpplings 练习入口

- [`jthread1` — std::jthread 与 stop_token](../../../exercises/cpp20/jthread1.cpp)
- [`condvar1` — 条件变量与生产者-消费者模式](../../../exercises/cpp11-std/condvar1.cpp)
- [`expected23` — std::expected 错误处理](../../../exercises/cpp23/expected23.cpp)
