---
title: "C++14 shared_timed_mutex and shared_lock"
topic: unknown
feature: shared-timed-mutex
standard: N/A
status_checked_at: 2026-06-02
---
# C++14 shared_timed_mutex and shared_lock

## Overview

C++14 introduces `std::shared_timed_mutex` and `std::shared_lock`, providing shared (reader-writer) mutex support with timeout. Multiple threads can hold a shared lock (read lock) simultaneously, but an exclusive lock (write lock) requires exclusive access. This reader-writer lock pattern is suited for concurrent scenarios with frequent reads and infrequent writes.

C++17 further introduces `std::shared_mutex` (a non-timed version), but in C++14, `shared_timed_mutex` is the only shared mutex type.

## Headers

```cpp
#include <shared_mutex>  // shared_timed_mutex, shared_lock
#include <mutex>          // unique_lock
#include <chrono>         // chrono literals
```

## Reader-Writer Lock Core Concepts

| Operation | Lock Type | Concurrency | Standard Library Tool |
|-----------|-----------|-------------|----------------------|
| Read | Shared lock | Multiple readers concurrently | `shared_lock` |
| Write | Exclusive lock | Only one writer | `unique_lock` |
| Timed read | Shared + timeout | Multiple readers | `shared_lock` + `try_lock_for` |
| Timed write | Exclusive + timeout | Only one writer | `unique_lock` + `try_lock_for` |

## Code Examples

### Basic Reader-Writer Pattern

```cpp
#include <shared_mutex>
#include <string>
#include <vector>

class ThreadSafeCache {
    mutable std::shared_timed_mutex mutex_;
    std::vector<std::string> data_;

public:
    // Read operation — shared lock, multiple threads can read concurrently
    std::string get(size_t index) const {
        std::shared_lock<std::shared_timed_mutex> lock(mutex_);
        return data_[index];
    }

    // Write operation — exclusive lock, exclusive access
    void set(size_t index, std::string value) {
        std::unique_lock<std::shared_timed_mutex> lock(mutex_);
        data_[index] = std::move(value);
    }

    // Iteration — exclusive or shared depending on whether modification occurs
    template <typename Func>
    void for_each(Func f) const {
        std::shared_lock<std::shared_timed_mutex> lock(mutex_);
        for (const auto& item : data_) {
            f(item);
        }
    }
};
```

### Timed Locks

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
    // Timed read: wait up to 100ms
    std::optional<std::string> try_get() const {
        std::shared_lock<std::shared_timed_mutex> lock(
            mutex_, std::chrono::milliseconds(100));
        if (!lock.owns_lock()) {
            return std::nullopt;  // Timed out, lock not acquired
        }
        return value_;
    }

    // Timed write: wait up to 500ms
    bool try_set(std::string val) {
        std::unique_lock<std::shared_timed_mutex> lock(
            mutex_, std::chrono::milliseconds(500));
        if (!lock.owns_lock()) {
            return false;  // Timed out
        }
        value_ = std::move(val);
        return true;
    }
};
```

### `try_lock_for` vs `try_lock_until`

```cpp
#include <shared_mutex>
#include <chrono>

std::shared_timed_mutex mtx;

void example_try_lock_for() {
    // Relative timeout: wait up to 50ms
    std::shared_lock<std::shared_timed_mutex> lock(
        mtx, std::chrono::milliseconds(50));
    if (lock.owns_lock()) {
        // Lock acquired, perform read operation
    }
}

void example_try_lock_until() {
    // Absolute timeout: wait until a specific time point
    auto deadline = std::chrono::steady_clock::now()
                  + std::chrono::seconds(2);

    std::unique_lock<std::shared_timed_mutex> lock(mtx, deadline);
    if (lock.owns_lock()) {
        // Lock acquired
    }
}

// Using C++14 chrono literals
using namespace std::chrono_literals;

void example_literals() {
    std::shared_lock<std::shared_timed_mutex> lock(mtx, 100ms);
}
```

### Reader-Writer Priority Strategy

```cpp
#include <shared_mutex>
#include <atomic>

class ReadWritePriority {
    std::shared_timed_mutex mutex_;
    std::atomic<int> pending_writers_{0};

public:
    // Read operation — yield briefly when writers are waiting
    void read() {
        // If writers are waiting, yield first
        while (pending_writers_.load() > 0) {
            std::this_thread::yield();
        }
        std::shared_lock<std::shared_timed_mutex> lock(mutex_);
        // Perform read operation...
    }

    // Write operation — mark self as waiting
    void write() {
        pending_writers_.fetch_add(1);
        {
            std::unique_lock<std::shared_timed_mutex> lock(mutex_);
            // Perform write operation...
        }
        pending_writers_.fetch_sub(1);
    }
};
```

### shared_timed_mutex vs shared_mutex (C++17)

```cpp
// C++14: only shared_timed_mutex
#include <shared_mutex>
std::shared_timed_mutex mtx14;  // Supports try_lock_for / try_lock_until

// C++17: adds shared_mutex (no timeout, potentially more efficient)
// std::shared_mutex mtx17;     // Does not support timed operations

// Selection guidance:
// - Need timeout → shared_timed_mutex
// - No timeout needed, and C++17 available → shared_mutex (may use lighter futex implementation)
// - In C++14 → only shared_timed_mutex is available
```

### Typical Application: Hot Configuration Reload

```cpp
#include <shared_mutex>
#include <string>
#include <unordered_map>

class ConfigManager {
    mutable std::shared_timed_mutex mutex_;
    std::unordered_map<std::string, std::string> config_;

public:
    // Read config — high-frequency operation
    std::string get(const std::string& key) const {
        std::shared_lock<std::shared_timed_mutex> lock(mutex_);
        auto it = config_.find(key);
        return it != config_.end() ? it->second : "";
    }

    // Batch update — low-frequency operation
    void update(std::unordered_map<std::string, std::string> new_config) {
        std::unique_lock<std::shared_timed_mutex> lock(mutex_);
        for (auto& [k, v] : new_config) {
            config_[k] = std::move(v);
        }
    }
};
```

## Best Practices

1. **Use reader-writer locks only for read-heavy, write-light scenarios**: If read and write frequencies are similar, the extra overhead of `shared_timed_mutex` (reader counting) may be slower than a plain `std::mutex`. Measure first, then choose.
2. **Use `shared_lock` for shared locks, `unique_lock` for exclusive locks**: The type system prevents misuse — `shared_lock` can only acquire shared locks.
3. **Keep lock granularity small**: Do not perform expensive operations (I/O, network requests) while holding a read lock; otherwise writers may be starved.
4. **Watch for writer starvation**: The default implementation typically favors readers (allowing new readers to continue acquiring locks while a writer waits). If writers are frequent, consider adjusting the strategy or switching to a plain mutex.
5. **The `mutable` keyword**: `shared_timed_mutex` members need to be locked even in `const` methods, so they should be declared `mutable`.
6. **C++17 migration recommendation**: If timeout functionality is not needed, migrating to `std::shared_mutex` may yield better platform-level performance.
