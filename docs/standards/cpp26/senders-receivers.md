---
title: C++26 Senders/Receivers
status_checked_at: 2026-06-01
topic: unknown
feature: senders-receivers
standard: N/A
---


# C++26 Senders/Receivers

## 概述

Senders/Receivers（P2300）是 C++26 异步执行框架，定义 `sender`、`receiver`、`scheduler` 三个核心概念，构建类型安全、可组合的结构化并发模型。

**提案状态：** P2300R10 已被 C++26 接受。

## 核心概念

**Sender** — 异步操作的惰性描述，尚未执行：

```cpp
#include <execution>
namespace ex = std::execution;
auto s = ex::just(42);  // 仅描述
```

**Receiver** — 处理三种信号：`set_value`（正常）、`set_error`（错误）、`set_stopped`（取消）。

**Scheduler** — 决定执行上下文：`auto sched = ex::system_context().get_scheduler();`

## 链式组合

### `then`

```cpp
#include <execution>
#include <print>
namespace ex = std::execution;

auto work = ex::just(21)
          | ex::then([](int x) { return x * 2; })
          | ex::then([](int x) { return std::format("result: {}", x); });

auto result = ex::sync_wait(std::move(work));
std::println("{}", std::get<0>(*result));  // result: 42
```

### `upon_error`

```cpp
auto work = ex::just(42)
          | ex::then([](int x) -> int {
                if (x > 100) throw std::runtime_error("too large");
                return x;
            })
          | ex::upon_error([](std::exception_ptr) -> int { return -1; });
```

### `upon_stopped`

```cpp
auto work = ex::just()
          | ex::upon_stopped([]() -> int { return 0; });
```

## 并行组合

### `when_all`

```cpp
auto s1 = ex::just(1) | ex::then([](int x) { return x * 10; });
auto s2 = ex::just(2) | ex::then([](int x) { return x * 10; });
auto [a, b] = *ex::sync_wait(ex::when_all(s1, s2));  // a=10, b=20
```

### `transfer`

```cpp
auto work = ex::just(42)
          | ex::transfer(sched)                       // 切到线程池
          | ex::then([](int x) { return x * 2; })
          | ex::transfer(ex::inline_scheduler());     // 切回调用线程
```

## 完整管线

```cpp
#include <execution>
#include <vector>
#include <numeric>
#include <algorithm>
namespace ex = std::execution;

std::string process_data(std::vector<int> const& data) {
    auto sched = ex::system_context().get_scheduler();
    auto pipeline = ex::just(data)
        | ex::transfer(sched)
        | ex::then([](std::vector<int> d) {
              std::ranges::sort(d); return d;
          })
        | ex::then([](std::vector<int> d) {
              return std::format("sorted, sum={}",
                  std::accumulate(d.begin(), d.end(), 0));
          });
    return std::get<0>(*ex::sync_wait(std::move(pipeline)));
}
```

## 与 std::async/future 对比

| 维度 | `std::async`/`future` | Senders/Receivers |
|------|----------------------|-------------------|
| 执行模型 | 立即（eager） | 惰性（lazy） |
| 取消 | 无 | `set_stopped` |
| 错误传播 | `get()` 抛异常 | `set_error` 通道 |
| 组合 | 有限 | 管线 `\|` 运算子 |
| 执行上下文 | async/deferred | 任意 scheduler |

## 结构化并发

所有异步操作的生命周期由作用域管理，消除"悬挂 future"问题：

```cpp
auto work = ex::when_all(
    ex::just(1) | ex::then([](int x) { return x * 2; }),
    ex::just(2) | ex::then([](int x) { return x * 3; })
);
auto [a, b] = *ex::sync_wait(std::move(work));
```

## 实现状态

| 库 | 状态 |
|----|------|
| libstdc++ / libc++ / MSVC STL | 跟进中 |
| NVIDIA [stdexec](https://github.com/NVIDIA/stdexec) | 最完整参考实现 |

## 总结

Senders/Receivers 通过 sender/receiver/scheduler 概念和 `then`/`when_all`/`transfer` 组合操作，在取消、错误处理、组合性方面相比 `std::async`/`future` 有根本性改进，原生支持结构化并发。
