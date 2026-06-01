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
