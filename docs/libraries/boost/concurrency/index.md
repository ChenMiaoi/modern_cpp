# Boost 并发

## Thread

Boost.Thread 是 `std::thread` 的前身。在 C++11 标准化之前，它是跨平台线程的唯一标准选择。现代项目应直接使用 `std::thread` + `std::jthread`（C++20）。

## Fiber：用户态协程

Boost.Fiber 提供用户态的协作式多任务——协程在用户空间切换，不涉及内核上下文切换：

```cpp
boost::fibers::buffered_channel<int> chan(10);

// 生产者
boost::fibers::fiber producer([&chan] {
    for (int i = 0; i < 100; ++i)
        chan.push(i);
    chan.close();
});

// 消费者
boost::fibers::fiber consumer([&chan] {
    int value;
    while (chan.pop(value) != boost::fibers::channel_op_status::closed)
        process(value);
});
```

Fiber 的核心优势：切换开销约 10ns（vs 线程切换约 1μs）。适合 I/O 密集型的高并发服务。

## Coroutine2：对称协程

Boost.Coroutine2 提供对称和非对称协程的底层控制：

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

**注意**：C++20 的原生协程 (`co_await`/`co_yield`) 已成为首选方案。Coroutine2 适合需要 C++14/17 兼容的项目。

## Lockfree：无锁数据结构

```cpp
boost::lockfree::queue<int> queue(1024);  // 无锁队列
queue.push(42);
int value;
queue.pop(value);  // 无锁弹出
```

Lockfree 提供三种无锁容器：`queue`（Michael-Scott 队列）、`stack`（Treiber 栈）、`spsc_queue`（单生产者单消费者队列，最快）。`spsc_queue` 在单生产者单消费者场景下无任何原子操作——仅需内存屏障。
