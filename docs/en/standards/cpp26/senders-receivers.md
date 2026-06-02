---
title: C++26 Senders/Receivers
status_checked_at: 2026-06-01
topic: unknown
feature: senders-receivers
standard: N/A
---


# C++26 Senders/Receivers

## Overview

Senders/Receivers (P2300) is the C++26 asynchronous execution framework, defining three core concepts — `sender`, `receiver`, and `scheduler` — to build a type-safe, composable structured concurrency model.

**Proposal status:** P2300R10 has been accepted for C++26.

## Core Concepts

**Sender** — A lazy description of an asynchronous operation, not yet executed:

```cpp
#include <execution>
namespace ex = std::execution;
auto s = ex::just(42);  // Description only
```

**Receiver** — Handles three signals: `set_value` (normal), `set_error` (error), `set_stopped` (cancellation).

**Scheduler** — Determines execution context: `auto sched = ex::system_context().get_scheduler();`

## Chained Composition

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

## Parallel Composition

### `when_all`

```cpp
auto s1 = ex::just(1) | ex::then([](int x) { return x * 10; });
auto s2 = ex::just(2) | ex::then([](int x) { return x * 10; });
auto [a, b] = *ex::sync_wait(ex::when_all(s1, s2));  // a=10, b=20
```

### `transfer`

```cpp
auto work = ex::just(42)
          | ex::transfer(sched)                       // Switch to thread pool
          | ex::then([](int x) { return x * 2; })
          | ex::transfer(ex::inline_scheduler());     // Switch back to calling thread
```

## Complete Pipeline

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

## Comparison with std::async/future

| Dimension | `std::async`/`future` | Senders/Receivers |
|-----------|----------------------|-------------------|
| Execution model | Eager | Lazy |
| Cancellation | None | `set_stopped` |
| Error propagation | `get()` throws | `set_error` channel |
| Composition | Limited | Pipeline `\|` operator |
| Execution context | async/deferred | Any scheduler |

## Structured Concurrency

All asynchronous operations have their lifetimes managed by scopes, eliminating the "dangling future" problem:

```cpp
auto work = ex::when_all(
    ex::just(1) | ex::then([](int x) { return x * 2; }),
    ex::just(2) | ex::then([](int x) { return x * 3; })
);
auto [a, b] = *ex::sync_wait(std::move(work));
```

## Implementation Status

| Library | Status |
|---------|--------|
| libstdc++ / libc++ / MSVC STL | In progress |
| NVIDIA [stdexec](https://github.com/NVIDIA/stdexec) | Most complete reference implementation |

## Summary

Senders/Receivers, through sender/receiver/scheduler concepts and `then`/`when_all`/`transfer` composition operations, provide fundamental improvements over `std::async`/`future` in cancellation, error handling, and composability, with native support for structured concurrency.
