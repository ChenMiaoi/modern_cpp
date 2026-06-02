---
title: "C++14 shared_timed_mutex 与 shared_lock"
topic: unknown
feature: shared-timed-mutex
standard: N/A
status_checked_at: 2026-06-02
---
# C++14 shared_timed_mutex 与 shared_lock

## 概述

C++14 引入了 `std::shared_timed_mutex` 和 `std::shared_lock`，提供了带超时的共享（读-写）互斥量支持。多个线程可以同时持有共享锁（读锁），但独占锁（写锁）需要排他访问。这种读-写锁模式适用于读多写少的并发场景。

C++17 进一步引入了 `std::shared_mutex`（无超时版本），但在 C++14 中 `shared_timed_mutex` 是唯一的共享互斥量类型。

## 头文件

```cpp
#include <shared_mutex>  // shared_timed_mutex, shared_lock
#include <mutex>          // unique_lock
#include <chrono>         // 时间字面量
```

## 读-写锁核心概念

| 操作 | 锁类型 | 并发数 | 标准库工具 |
|------|--------|--------|-----------|
| 读 | 共享锁 (shared) | 多个读者可并发 | `shared_lock` |
| 写 | 独占锁 (unique) | 仅一个写者 | `unique_lock` |
| 带超时的读 | 共享 + 超时 | 多个读者 | `shared_lock` + `try_lock_for` |
| 带超时的写 | 独占 + 超时 | 仅一个写者 | `unique_lock` + `try_lock_for` |

## 代码示例

### 基本读-写模式

```cpp
#include <shared_mutex>
#include <string>
#include <vector>

class ThreadSafeCache {
    mutable std::shared_timed_mutex mutex_;
    std::vector<std::string> data_;

public:
    // 读操作 — 共享锁，多线程可同时读
    std::string get(size_t index) const {
        std::shared_lock<std::shared_timed_mutex> lock(mutex_);
        return data_[index];
    }

    // 写操作 — 独占锁，排他访问
    void set(size_t index, std::string value) {
        std::unique_lock<std::shared_timed_mutex> lock(mutex_);
        data_[index] = std::move(value);
    }

    // 迭代 — 独占或共享取决于是否修改
    template <typename Func>
    void for_each(Func f) const {
        std::shared_lock<std::shared_timed_mutex> lock(mutex_);
        for (const auto& item : data_) {
            f(item);
        }
    }
};
```

### 带超时的锁

```cpp
#include <shared_mutex>
#include <chrono>
#include <optional>
#include <string>
#include <stdexcept>

class TimedCache {
    mutable std::shared_timed_mutex mutex_;
    std::string value_;

public:
    // 超时读取：最多等待 100ms
    std::optional<std::string> try_get() const {
        std::shared_lock<std::shared_timed_mutex> lock(
            mutex_, std::chrono::milliseconds(100));
        if (!lock.owns_lock()) {
            return std::nullopt;  // 超时，未获得锁
        }
        return value_;
    }

    // 超时写入：最多等待 500ms
    bool try_set(std::string val) {
        std::unique_lock<std::shared_timed_mutex> lock(
            mutex_, std::chrono::milliseconds(500));
        if (!lock.owns_lock()) {
            return false;  // 超时
        }
        value_ = std::move(val);
        return true;
    }
};
```

### `try_lock_for` 与 `try_lock_until`

```cpp
#include <shared_mutex>
#include <chrono>

std::shared_timed_mutex mtx;

void example_try_lock_for() {
    // 相对超时：等待最多 50ms
    std::shared_lock<std::shared_timed_mutex> lock(
        mtx, std::chrono::milliseconds(50));
    if (lock.owns_lock()) {
        // 获得锁，执行读操作
    }
}

void example_try_lock_until() {
    // 绝对超时：等待到某个时间点
    auto deadline = std::chrono::steady_clock::now()
                  + std::chrono::seconds(2);

    std::unique_lock<std::shared_timed_mutex> lock(mtx, deadline);
    if (lock.owns_lock()) {
        // 获得锁
    }
}

// 使用 C++14 时间字面量
using namespace std::chrono_literals;

void example_literals() {
    std::shared_lock<std::shared_timed_mutex> lock(mtx, 100ms);
}
```

### 读-写优先级策略

```cpp
#include <shared_mutex>
#include <atomic>

class ReadWritePriority {
    std::shared_timed_mutex mutex_;
    std::atomic<int> pending_writers_{0};

public:
    // 读操作 — 有写者等待时，短暂退让
    void read() {
        // 如果有写者等待，先退让
        while (pending_writers_.load() > 0) {
            std::this_thread::yield();
        }
        std::shared_lock<std::shared_timed_mutex> lock(mutex_);
        // 执行读操作...
    }

    // 写操作 — 标记自己正在等待
    void write() {
        pending_writers_.fetch_add(1);
        {
            std::unique_lock<std::shared_timed_mutex> lock(mutex_);
            // 执行写操作...
        }
        pending_writers_.fetch_sub(1);
    }
};
```

### shared_timed_mutex vs shared_mutex (C++17)

```cpp
// C++14: 只有 shared_timed_mutex
#include <shared_mutex>
std::shared_timed_mutex mtx14;  // 支持 try_lock_for / try_lock_until

// C++17: 新增 shared_mutex（无超时，可能更高效）
// std::shared_mutex mtx17;     // 不支持超时操作

// 选择建议：
// - 需要超时 → shared_timed_mutex
// - 不需要超时，且平台 C++17 可用 → shared_mutex（可能用更轻量的 futex 实现）
// - 在 C++14 中 → 只能用 shared_timed_mutex
```

### 典型应用：配置热更新

```cpp
#include <shared_mutex>
#include <string>
#include <unordered_map>

class ConfigManager {
    mutable std::shared_timed_mutex mutex_;
    std::unordered_map<std::string, std::string> config_;

public:
    // 读取配置 — 高频操作
    std::string get(const std::string& key) const {
        std::shared_lock<std::shared_timed_mutex> lock(mutex_);
        auto it = config_.find(key);
        return it != config_.end() ? it->second : "";
    }

    // 批量更新 — 低频操作
    void update(std::unordered_map<std::string, std::string> new_config) {
        std::unique_lock<std::shared_timed_mutex> lock(mutex_);
        for (auto& [k, v] : new_config) {
            config_[k] = std::move(v);
        }
    }
};
```

## 最佳实践

1. **读多写少场景才用读-写锁**：如果读写频率接近，`shared_timed_mutex` 的额外开销（读者计数）可能比普通 `std::mutex` 更慢。先测量再选择。
2. **共享锁用 `shared_lock`，独占锁用 `unique_lock`**：类型系统防止误用——`shared_lock` 只能获得共享锁。
3. **锁的粒度要小**：持有读锁期间不要执行耗时操作（I/O、网络请求），否则写者会被饿死。
4. **注意写者饥饿问题**：默认实现通常偏向读者（允许新读者在写者等待时继续获取锁）。写者频繁时考虑策略调整或改用普通互斥量。
5. **`mutable` 关键字**：`shared_timed_mutex` 成员在 `const` 方法中也需要加锁，因此应声明为 `mutable`。
6. **C++17 迁移建议**：如果不需要超时功能，迁移到 `std::shared_mutex` 可能获得更好的平台性能。
