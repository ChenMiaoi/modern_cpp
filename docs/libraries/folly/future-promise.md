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

待补：补上 Folly Future 与标准 `std::future`/`std::promise` 的语义差异，尤其是 continuation 与 executor 绑定。

## 对象布局

上文已经给出 `Core<T>` 的关键字段；后续补 `Promise`/`Future` 句柄与共享 `Core` 之间的所有权关系图。

## 核心源码路径

本文开头已给出 `Future.h`、`Promise.h`、`Core.h`；后续补 continuation 注册到 executor 调度的入口链。

## 核心类 / 函数

待补：统一整理 `Core<T>`、`Try<T>`、`Promise<T>`、`SemiFuture<T>`、`Future<T>`、`.via()` 与 `.thenValue()`。

## 关键算法

上文已经覆盖四状态机；后续补 `setValue` / `setException` / continuation 安装 / executor 提交这几条核心路径。

## ABI 约束

待补：说明 Folly futures 以模板与头文件内联为主，兼容性更多受 API/布局演进影响，而不是标准库式 ABI 保证。

## 异常安全

待补：补充 `Try<T>` 封装异常、continuation 抛异常传播、Promise 破约（broken promise）与 executor 提交失败的边界。

## iterator / reference invalidation

待补：本文主题不是容器 iterator；后续这里补共享状态、continuation 捕获对象与 `Future` 链移动后的句柄有效性规则。

## 性能模型

待补：补上一次共享状态分配、状态机原子同步、executor hop 数量与 continuation 链长度的成本模型。

## libstdc++ vs libc++ vs MSVC

待补：这里主要与三家标准库的 `std::future` 对照，说明 Folly continuation 模型、异常传递与调度语义的差别。

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

待补：补上 `Core` 状态转移、continuation 提交与 `std::future` 阻塞路径的 benchmark/反汇编证据。

## cpplings 练习入口

- [`jthread1` — std::jthread 与 stop_token](../../../exercises/cpp20/jthread1.cpp)
- [`condvar1` — 条件变量与生产者-消费者模式](../../../exercises/cpp11-std/condvar1.cpp)
- [`expected23` — std::expected 错误处理](../../../exercises/cpp23/expected23.cpp)
