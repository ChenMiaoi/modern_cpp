# `std::jthread` 与协作式取消

## 概述

C++20 引入 `std::jthread`（joining thread）：
1. 析构时**自动 join**，消除遗忘 join/detach 导致的 `terminate`。
2. 内置**协作式取消**：`stop_token` + `stop_source`。

## 基本用法

```cpp
#include <thread>
#include <iostream>

int main() {
    std::jthread t([] {
        std::cout << "Hello from jthread\n";
    });
    // 析构时自动 join
}
```

## 与 `std::thread` 对比

| 特性 | `std::thread` | `std::jthread` |
|------|---------------|-----------------|
| 析构行为 | `terminate()`（若可 join） | 自动 `join()` |
| 取消机制 | 需自行实现 | 内置 `stop_token` |
| 可重新赋值 | 是 | 是（先 join 当前线程） |
| C++ 标准 | C++11 | C++20 |

```cpp
// std::thread：异常不安全
{ std::thread t([]{ /* work */ }); t.join(); }

// std::jthread：异常安全
{ std::jthread t([]{ /* work */ }); }
```

## `stop_token` 与 `stop_source`

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
    std::jthread t(worker);  // stop_token 自动传入
    std::this_thread::sleep_for(std::chrono::seconds(1));
    t.request_stop();
}
```

`jthread` 内部持有 `stop_source`，自动将 `stop_token` 注入含该参数的函数：

```cpp
void worker(std::stop_token token, int extra);
std::jthread t(worker, 42);  // token 自动注入
```

## 手动使用 `stop_source`

```cpp
std::stop_source source;
std::stop_token token = source.get_token();
std::jthread t([token] {
    while (!token.stop_requested()) { /* work */ }
});
source.request_stop();
```

## 取消模式

### 周期性检查

```cpp
void process(std::stop_token token, std::span<int> data) {
    for (size_t i = 0; i < data.size(); ++i) {
        if (token.stop_requested()) return;
        data[i] = heavy_computation(data[i]);
    }
}
```

### `stop_callback` 清理

```cpp
void worker(std::stop_token token) {
    std::stop_callback on_stop(token, [] {
        std::cout << "Cleaning up...\n";
    });
    while (!token.stop_requested()) { /* work */ }
}
```

## 与 `condition_variable_any` 配合

C++20 为 `condition_variable_any` 添加 `stop_token` 重载：

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

## 取消多个线程

```cpp
std::stop_source source;
std::vector<std::jthread> workers;
for (int i = 0; i < 4; ++i) {
    workers.emplace_back([token = source.get_token()] {
        while (!token.stop_requested()) { /* work */ }
    });
}
source.request_stop();  // 一次性请求全部停止
```

## 常见陷阱

```cpp
// 1. 协作式取消——线程可以忽略 stop_requested
// 2. 析构时 join 可能阻塞
void bad() { std::jthread t([] { while (true) {} }); }
// 3. 析构顺序
struct Worker {
    std::jthread thread_;
    ~Worker() { thread_.request_stop(); }
};
// 4. move 后原对象不再 joinable
std::jthread t1([]{});
std::jthread t2 = std::move(t1);
```

## 总结

- 析构自动 join，消除线程管理常见 bug。
- `stop_token` + `stop_source` 提供标准化协作式取消。
- 含 `stop_token` 签名的函数被 `jthread` 自动注入。
- `condition_variable_any::wait` 支持 `stop_token` 实现可取消阻塞。
