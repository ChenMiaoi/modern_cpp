---
title: "std::thread 与多线程编程"
topic: unknown
feature: thread
standard: N/A
status_checked_at: 2026-06-02
---
# std::thread 与多线程编程

## 概述

C++11 引入了标准化的线程支持库，包括 `std::thread`、互斥量、条件变量以及异步设施。在此之前，C++ 标准不涉及并发，各平台各自为政。C++11 的线程库让开发者能够编写可移植的多线程代码，无需依赖 POSIX threads 或 Win32 API。

本文涵盖线程生命周期管理、同步原语、异步编程模型，以及线程局部存储。

## API 概览

| 组件 | 说明 |
|------|------|
| `std::thread` | 可执行线程的句柄，接受可调用对象 |
| `std::mutex` / `std::lock_guard` / `std::unique_lock` | 互斥量与 RAII 锁 |
| `std::condition_variable` | 条件变量，线程间等待/通知 |
| `std::async` / `std::future` / `std::promise` | 异步任务与返回值传递 |
| `thread_local` | 线程局部存储说明符 |

## 线程的创建、join 与 detach

`std::thread` 接受任何可调用对象（函数、lambda、仿函数）及其参数：

```cpp
#include <thread>
#include <iostream>

void worker(int id) { std::cout << "Thread " << id << " running\n"; }

int main() {
    std::thread t1(worker, 1);
    std::thread t2([](int id) { std::cout << "Lambda " << id << '\n'; }, 2);
    t1.join();   // 等待线程完成
    t2.join();
    // detach：让线程在后台运行，失去对它的控制
    std::thread t3(worker, 3);
    t3.detach();          // t3 不再 joinable
}
```

**关键规则：** `std::thread` 析构前必须 `join()` 或 `detach()`，否则程序调用 `std::terminate()`。可用 `joinable()` 检查状态。

## RAII 线程包装

手动管理 `join()` 容易在异常路径下遗漏。RAII 包装器解决此问题：

```cpp
class scoped_thread {
    std::thread t_;
public:
    explicit scoped_thread(std::thread t) : t_(std::move(t)) {
        if (!t_.joinable()) throw std::logic_error("No thread");
    }
    ~scoped_thread() { if (t_.joinable()) t_.join(); }
    scoped_thread(const scoped_thread&) = delete;
    scoped_thread& operator=(const scoped_thread&) = delete;
};

void use_scoped_thread() {
    scoped_thread st{std::thread(worker, 42)};
    // 函数返回时自动 join，即使抛出异常也不会悬挂
}
```

> **C++20 `std::jthread`**：标准库内置 RAII 线程类，析构时自动 `join()`，支持 `std::stop_token` 协作式取消。C++11 中需自行实现上述包装。

## std::mutex 与锁

互斥量保护共享数据，防止数据竞争：

```cpp
#include <mutex>

std::mutex mtx;
int shared_counter = 0;

void increment(int n) {
    for (int i = 0; i < n; ++i) {
        std::lock_guard<std::mutex> lock(mtx);  // 构造加锁，析构解锁
        ++shared_counter;
    }
}
```

`std::unique_lock` 提供更灵活的锁管理（手动 lock/unlock、配合条件变量）。相比 `lock_guard`，它支持延迟加锁、手动解锁、重新加锁，是 `condition_variable` 的必要搭配。

## std::condition_variable

条件变量用于线程间的等待/通知模式：

```cpp
#include <condition_variable>
#include <queue>

std::mutex queue_mtx;
std::condition_variable cv;
std::queue<int> task_queue;
bool done = false;

void producer() {
    for (int i = 0; i < 10; ++i) {
        { std::lock_guard<std::mutex> lk(queue_mtx); task_queue.push(i); }
        cv.notify_one();
    }
    { std::lock_guard<std::mutex> lk(queue_mtx); done = true; }
    cv.notify_all();
}

void consumer() {
    std::unique_lock<std::mutex> lk(queue_mtx);
    while (true) {
        cv.wait(lk, [] { return !task_queue.empty() || done; });
        // wait：条件为 false 时释放锁并阻塞，唤醒后重新加锁
        while (!task_queue.empty()) {
            int task = task_queue.front(); task_queue.pop();
            lk.unlock();
            // 处理 task ...
            lk.lock();
        }
        if (done) break;
    }
}
```

> **注意：** `wait` 必须搭配 `std::unique_lock`，因为它需要在等待期间释放锁并在唤醒后重新获取。

## std::async、std::future 与 std::promise

```cpp
#include <future>

int compute(int x) { return x * x; }

int main() {
    // std::launch::async：保证新线程；std::launch::deferred：延迟到 get() 时执行
    auto fut = std::async(std::launch::async, compute, 42);
    int result = fut.get();  // 1764，阻塞等待
}
```

`std::promise` 用于手动传递异步结果（promise 在另一线程 `set_value`，future 在当前线程 `get` 阻塞等待）：

```cpp
std::promise<int> prom;
auto fut = prom.get_future();
std::thread t([](std::promise<int> p) { p.set_value(42); }, std::move(prom));
int val = fut.get();  // 42
t.join();
```

## thread_local 与硬件并发度

```cpp
thread_local int tls_counter = 0;  // 每个线程独立副本

void thread_func() { ++tls_counter; /* 始终为 1 */ }

int main() {
    std::thread t1(thread_func), t2(thread_func);
    t1.join(); t2.join();
    // 主线程的 tls_counter 仍为 0
    // hardware_concurrency：返回逻辑核心数，0 表示无法检测
    unsigned cores = std::thread::hardware_concurrency();
}
```

## 最佳实践

1. **优先使用 RAII 锁**：`std::lock_guard` 和 `std::unique_lock` 防止忘记解锁。
2. **避免裸 `std::thread`**：封装为 RAII 类或迁移到 C++20 `std::jthread`。
3. **锁的粒度要小**：只在访问共享数据时持锁。
4. **用 `std::lock()` 同时锁定多个互斥量**：避免死锁。
5. **优先 `std::async` 处理一次性异步任务**：比手动管理线程更安全。

## 常见陷阱

- **忘记 join/detach**：`std::thread` 析构时若仍 joinable，程序直接终止。
- **数据竞争**：未加保护地从多线程读写同一变量是未定义行为。
- **虚假唤醒**：`condition_variable::wait` **必须**搭配谓词使用。
- **`std::async` 返回值被忽略**：返回的 `std::future` 析构时会阻塞。
- **`std::mutex` 不可拷贝不可移动**：持有 mutex 的对象不应被拷贝或移动。
