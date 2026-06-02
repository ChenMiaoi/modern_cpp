---
title: "Boost 并发"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# Boost Concurrency

## Thread

Boost.Thread is the predecessor of `std::thread`. Before C++11 standardization, it was the only standard choice for cross-platform threading. Modern projects should use `std::thread` + `std::jthread` (C++20) directly.

## Fiber: User-Space Coroutines

Boost.Fiber provides user-space cooperative multitasking — coroutines switch in user space without involving kernel context switches:

```cpp
boost::fibers::buffered_channel<int> chan(10);

// Producer
boost::fibers::fiber producer([&chan] {
    for (int i = 0; i < 100; ++i)
        chan.push(i);
    chan.close();
});

// Consumer
boost::fibers::fiber consumer([&chan] {
    int value;
    while (chan.pop(value) != boost::fibers::channel_op_status::closed)
        process(value);
});
```

Fiber's core advantage: switching overhead is approximately 10ns (vs thread switching ~1μs). Suitable for high-concurrency I/O-intensive services.

## Coroutine2: Symmetric Coroutines

Boost.Coroutine2 provides low-level control over symmetric and asymmetric coroutines:

```cpp
namespace coro = boost::coroutines2;

coro::asymmetric_coroutine<int>::pull_type source(
    [](coro::asymmetric_coroutine<int>::push_type& yield) {
        int i = 0;
        while (true) {
            yield(i++);
        }
    });

for (int i = 0; i < 10; ++i)
    std::cout << source.get() << " ", source();
```

**Note**: C++20 native coroutines (`co_await`/`co_yield`) are now the preferred solution. Coroutine2 is suitable for projects requiring C++14/17 compatibility.

## Lockfree: Lock-Free Data Structures

```cpp
boost::lockfree::queue<int> queue(1024);  // Lock-free queue
queue.push(42);
int value;
queue.pop(value);  // Lock-free pop
```

Lockfree provides three lock-free containers: `queue` (Michael-Scott queue), `stack` (Treiber stack), `spsc_queue` (single-producer single-consumer queue, fastest). `spsc_queue` in single-producer single-consumer scenarios uses no atomic operations — only memory barriers.
