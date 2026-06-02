---
title: "`std::jthread` and Cooperative Cancellation"
topic: unknown
feature: jthread
standard: N/A
status_checked_at: 2026-06-02
---
# `std::jthread` and Cooperative Cancellation

## Overview

C++20 introduces `std::jthread` (joining thread):
1. **Automatically joins** on destruction, eliminating `terminate` from forgotten join/detach calls.
2. Built-in **cooperative cancellation**: `stop_token` + `stop_source`.

## Basic Usage

```cpp
#include <thread>
#include <iostream>

int main() {
    std::jthread t([] {
        std::cout << "Hello from jthread\n";
    });
    // Automatically joins on destruction
}
```

## Comparison with `std::thread`

| Feature | `std::thread` | `std::jthread` |
|---------|---------------|-----------------|
| Destruction behavior | `terminate()` (if joinable) | Automatic `join()` |
| Cancellation mechanism | Must implement yourself | Built-in `stop_token` |
| Reassignable | Yes | Yes (joins current thread first) |
| C++ standard | C++11 | C++20 |

```cpp
// std::thread: exception-unsafe
{ std::thread t([]{ /* work */ }); t.join(); }

// std::jthread: exception-safe
{ std::jthread t([]{ /* work */ }); }
```

## `stop_token` and `stop_source`

```cpp
#include <thread>
#include <iostream>

void worker(std::stop_token token) {
    int count = 0;
    while (!token.stop_requested()) {
        ++count;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "Stopped after " << count << " iterations\n";
}

int main() {
    std::jthread t(worker);  // stop_token is automatically passed in
    std::this_thread::sleep_for(std::chrono::seconds(1));
    t.request_stop();
}
```

`jthread` internally holds a `stop_source` and automatically injects a `stop_token` into functions accepting that parameter:

```cpp
void worker(std::stop_token token, int extra);
std::jthread t(worker, 42);  // token automatically injected
```

## Manual Use of `stop_source`

```cpp
std::stop_source source;
std::stop_token token = source.get_token();
std::jthread t([token] {
    while (!token.stop_requested()) { /* work */ }
});
source.request_stop();
```

## Cancellation Patterns

### Periodic Checking

```cpp
void process(std::stop_token token, std::span<int> data) {
    for (size_t i = 0; i < data.size(); ++i) {
        if (token.stop_requested()) return;
        data[i] = heavy_computation(data[i]);
    }
}
```

### `stop_callback` Cleanup

```cpp
void worker(std::stop_token token) {
    std::stop_callback on_stop(token, [] {
        std::cout << "Cleaning up...\n";
    });
    while (!token.stop_requested()) { /* work */ }
}
```

## Integration with `condition_variable_any`

C++20 adds a `stop_token` overload for `condition_variable_any`:

```cpp
template <typename T>
class CancelableQueue {
    std::mutex mtx_;
    std::condition_variable_any cv_;
    std::queue<T> queue_;
public:
    void push(T item) {
        { std::lock_guard lk(mtx_); queue_.push(std::move(item)); }
        cv_.notify_one();
    }

    std::optional<T> pop(std::stop_token token) {
        std::unique_lock lk(mtx_);
        cv_.wait(lk, token, [this] { return !queue_.empty(); });
        if (token.stop_requested()) return std::nullopt;
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }
};
```

## Cancelling Multiple Threads

```cpp
std::stop_source source;
std::vector<std::jthread> workers;
for (int i = 0; i < 4; ++i) {
    workers.emplace_back([token = source.get_token()] {
        while (!token.stop_requested()) { /* work */ }
    });
}
source.request_stop();  // request all to stop at once
```

## Common Pitfalls

```cpp
// 1. Cooperative cancellation — threads can ignore stop_requested
// 2. Destruction join may block
void bad() { std::jthread t([] { while (true) {} }); }
// 3. Destruction order
struct Worker {
    std::jthread thread_;
    ~Worker() { thread_.request_stop(); }
};
// 4. Original object is no longer joinable after move
std::jthread t1([]{});
std::jthread t2 = std::move(t1);
```

## Summary

- Automatic join on destruction eliminates common thread management bugs.
- `stop_token` + `stop_source` provide standardized cooperative cancellation.
- Functions with a `stop_token` parameter are automatically injected by `jthread`.
- `condition_variable_any::wait` supports `stop_token` for cancellable blocking.
