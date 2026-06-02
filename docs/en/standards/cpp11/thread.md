---
title: "std::thread and Multithreaded Programming"
topic: unknown
feature: thread
standard: N/A
status_checked_at: 2026-06-02
---
# std::thread and Multithreaded Programming

## Overview

C++11 introduced a standardized thread support library, including `std::thread`, mutexes, condition variables, and async facilities. Before this, the C++ standard did not address concurrency, and each platform went its own way. C++11's thread library enables developers to write portable multithreaded code without relying on POSIX threads or the Win32 API.

This article covers thread lifecycle management, synchronization primitives, the asynchronous programming model, and thread-local storage.

## API Overview

| Component | Description |
|-----------|-------------|
| `std::thread` | Handle to an executable thread, accepts callable objects |
| `std::mutex` / `std::lock_guard` / `std::unique_lock` | Mutexes and RAII locks |
| `std::condition_variable` | Condition variable, inter-thread wait/notify |
| `std::async` / `std::future` / `std::promise` | Async tasks and return value passing |
| `thread_local` | Thread-local storage specifier |

## Creating, Joining, and Detaching Threads

`std::thread` accepts any callable object (function, lambda, functor) and its arguments:

```cpp
#include <thread>
#include <iostream>

void worker(int id) { std::cout << "Thread " << id << " running\n"; }

int main() {
    std::thread t1(worker, 1);
    std::thread t2([](int id) { std::cout << "Lambda " << id << '\n'; }, 2);
    t1.join();   // wait for thread to complete
    t2.join();
    // detach: let the thread run in the background, losing control over it
    std::thread t3(worker, 3);
    t3.detach();          // t3 is no longer joinable
}
```

**Key rule:** `std::thread` must be `join()`ed or `detach()`ed before destruction, otherwise the program calls `std::terminate()`. Use `joinable()` to check state.

## RAII Thread Wrapper

Manually managing `join()` is easily missed on exception paths. RAII wrappers solve this:

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
    // Automatically joins when the function returns, even if an exception is thrown
}
```

> **C++20 `std::jthread`**: The standard library's built-in RAII thread class, automatically `join()`s on destruction, supports `std::stop_token` for cooperative cancellation. In C++11, you need to implement the above wrapper yourself.

## std::mutex and Locks

Mutexes protect shared data, preventing data races:

```cpp
#include <mutex>

std::mutex mtx;
int shared_counter = 0;

void increment(int n) {
    for (int i = 0; i < n; ++i) {
        std::lock_guard<std::mutex> lock(mtx);  // locks on construction, unlocks on destruction
        ++shared_counter;
    }
}
```

`std::unique_lock` provides more flexible lock management (manual lock/unlock, pairing with condition variables). Compared to `lock_guard`, it supports deferred locking, manual unlocking, and re-locking, and is a necessary companion for `condition_variable`.

## std::condition_variable

Condition variables are used for inter-thread wait/notify patterns:

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
        // wait: releases lock and blocks when condition is false; re-acquires lock on wake
        while (!task_queue.empty()) {
            int task = task_queue.front(); task_queue.pop();
            lk.unlock();
            // process task ...
            lk.lock();
        }
        if (done) break;
    }
}
```

> **Note:** `wait` must be used with `std::unique_lock`, as it needs to release the lock during waiting and re-acquire it upon waking.

## std::async, std::future, and std::promise

```cpp
#include <future>

int compute(int x) { return x * x; }

int main() {
    // std::launch::async: guarantees new thread; std::launch::deferred: deferred until get()
    auto fut = std::async(std::launch::async, compute, 42);
    int result = fut.get();  // 1764, blocks until ready
}
```

`std::promise` is used for manually passing async results (promise `set_value`s in another thread, future `get`s in the current thread, blocking):

```cpp
std::promise<int> prom;
auto fut = prom.get_future();
std::thread t([](std::promise<int> p) { p.set_value(42); }, std::move(prom));
int val = fut.get();  // 42
t.join();
```

## thread_local and Hardware Concurrency

```cpp
thread_local int tls_counter = 0;  // independent copy per thread

void thread_func() { ++tls_counter; /* always 1 */ }

int main() {
    std::thread t1(thread_func), t2(thread_func);
    t1.join(); t2.join();
    // Main thread's tls_counter remains 0
    // hardware_concurrency: returns logical core count, 0 means undetectable
    unsigned cores = std::thread::hardware_concurrency();
}
```

## Best Practices

1. **Prefer RAII locks**: `std::lock_guard` and `std::unique_lock` prevent forgetting to unlock.
2. **Avoid raw `std::thread`**: Wrap in an RAII class or migrate to C++20 `std::jthread`.
3. **Keep lock granularity small**: Only hold locks while accessing shared data.
4. **Use `std::lock()` to lock multiple mutexes simultaneously**: Avoids deadlocks.
5. **Prefer `std::async` for one-off async tasks**: Safer than manual thread management.

## Common Pitfalls

- **Forgetting join/detach**: If `std::thread` is still joinable on destruction, the program terminates directly.
- **Data races**: Reading and writing the same variable from multiple threads without protection is undefined behavior.
- **Spurious wakeups**: `condition_variable::wait` **must** be used with a predicate.
- **`std::async` return value ignored**: The returned `std::future` blocks on destruction.
- **`std::mutex` is not copyable or movable**: Objects holding a mutex should not be copied or moved.
