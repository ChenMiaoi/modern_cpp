---
title: "C++20 Synchronization Primitives (latch/barrier/semaphore)"
topic: unknown
feature: synchronization-primitives
standard: N/A
status_checked_at: 2026-06-20
---
# C++20 Synchronization Primitives (latch/barrier/semaphore)

## Overview

C++20 introduces three high-level synchronization primitives in `<latch>`, `<barrier>`, and `<semaphore>`, filling the gap between `condition_variable` and atomic operations:

| Primitive | Semantics | Reusable |
|-----------|-----------|----------|
| `std::latch` | One-shot countdown, unblocks when reaching zero | Cannot reset |
| `std::barrier` | Phase-based sync, auto-resets each phase | Reusable |
| `std::counting_semaphore` | Resource pool counter, acquire/release controls concurrency | Unlimited |

## `std::latch` — One-Shot Countdown

A `std::latch` is a countdown gate: constructed with a count, `count_down()` decrements it, `wait()` blocks until it reaches zero.

```cpp
#include <latch>
#include <thread>
#include <iostream>
#include <vector>

int main() {
    constexpr int N = 4;
    std::latch ready(N);  // initial count N
    std::vector<std::jthread> threads;

    for (int i = 0; i < N; ++i) {
        threads.emplace_back([&ready, i] {
            std::cout << "Thread " << i << " preparing...\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100 * i));
            std::cout << "Thread " << i << " ready\n";
            ready.count_down();  // decrement
        });
    }

    ready.wait();  // blocks until all threads finish initialization
    std::cout << "All threads ready, proceeding\n";
}
```

### Core API

| Method | Description |
|--------|-------------|
| `latch(ptrdiff_t n)` | Construct with initial count `n` |
| `count_down()` | Atomic decrement, non-blocking |
| `wait()` | Block until count reaches zero |
| `try_wait()` | Non-blocking check if count has reached zero |
| `arriveAndWait()` | Equivalent to `count_down()` + `wait()` |

```cpp
std::latch latch(1);
// In some thread
latch.count_down();        // decrement
latch.wait();              // block
// Or combine both
latch.arriveAndWait();     // decrement and wait
```

## `std::barrier` — Reusable Phase Synchronization

`barrier` synchronizes threads across phases: all participants arrive at a sync point, a completion callback fires, then the next phase begins.

```cpp
#include <barrier>
#include <thread>
#include <iostream>
#include <vector>

int main() {
    constexpr int N = 4;
    int phase = 0;

    std::barrier sync_point(N, [&phase] {
        ++phase;
        std::cout << "--- Phase " << phase << " complete ---\n";
    });

    std::vector<std::jthread> threads;
    for (int i = 0; i < N; ++i) {
        threads.emplace_back([&sync_point, i] {
            for (int p = 0; p < 3; ++p) {
                std::cout << "Thread " << i << " phase " << p << " work\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                sync_point.arriveAndWait();  // arrive at sync point
            }
        });
    }
}
```

### Core API

| Method | Description |
|--------|-------------|
| `barrier(ptrdiff_t expected, completion_fn)` | Construct with participant count and completion callback |
| `arrive()` | Arrive without waiting |
| `arriveAndWait()` | Arrive and block until current phase ends |
| `wait()` | Wait for current phase to end (no arrival) |
| `advance_phase()` | Advance to next phase (called automatically by completion callback) |

### `arrive_and_drop` — Dynamic Participants

```cpp
std::barrier b(4);
// When a thread exits
b.arrive_and_drop();  // remove from participant set, no longer required to arrive
```

## `std::counting_semaphore` — Resource Pool Semaphore

A semaphore controls concurrent access to a shared resource pool, useful for connection pools, token buckets, etc.

```cpp
#include <semaphore>
#include <thread>
#include <iostream>
#include <vector>

int main() {
    constexpr int MAX_CONNECTIONS = 3;
    std::counting_semaphore sem(MAX_CONNECTIONS);  // max 3 concurrent

    auto use_connection = [&](int id) {
        sem.acquire();  // P operation
        std::cout << "Connection " << id << " acquired\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "Connection " << id << " released\n";
        sem.release();  // V operation
    };

    std::vector<std::jthread> threads;
    for (int i = 0; i < 6; ++i) {
        threads.emplace_back(use_connection, i);
    }
}
```

### Core API

| Method | Description |
|--------|-------------|
| `counting_semaphore(ptrdiff_t max)` | Construct with maximum count `max` |
| `acquire()` | Block until count > 0, then decrement |
| `try_acquire()` | Non-blocking attempt to acquire |
| `try_acquire_for(duration)` | Acquire with timeout |
| `try_acquire_until(time_point)` | Acquire with absolute time timeout |
| `release()` | Increment count |
| `release(1)` | Increment by specified amount |

### `std::binary_semaphore` — Binary Semaphore

```cpp
// binary_semaphore is equivalent to counting_semaphore<1>
std::binary_semaphore sem;  // usable as a mutex alternative
```

## Comparison with `condition_variable`

| Dimension | `condition_variable` | `latch` | `barrier` | `counting_semaphore` |
|-----------|---------------------|---------|-----------|---------------------|
| Purpose | General wait/notify | One-shot sync | Phase sync | Resource pool throttling |
| Reusable | Yes | No | Per-phase | Unlimited |
| State | Manual management | Built-in count | Built-in count | Built-in count |
| Fairness | No guarantee | FIFO | FIFO | FIFO |
| Complexity | Requires mutex | Lock-free (underneath) | Lock-free (underneath) | Lock-free (underneath) |

```cpp
// condition_variable approach for latch semantics (verbose)
std::mutex mtx;
std::condition_variable cv;
int count = 4;
// Each thread: { std::lock_guard lk(mtx); if (--count == 0) cv.notify_all(); }
// Waiter: { std::unique_lock lk(mtx); cv.wait(lk, [&]{ return count == 0; }); }

// latch approach (concise)
std::latch latch(4);
// Each thread: latch.count_down();
// Waiter: latch.wait();
```

## Use Cases

### Parallel Initialization

```cpp
void init_parallel(std::vector<Module>& modules) {
    std::latch init_done(modules.size());
    std::vector<std::jthread> threads;
    for (auto& mod : modules) {
        threads.emplace_back([&mod, &init_done] {
            mod.initialize();
            init_done.count_down();
        });
    }
    init_done.wait();  // wait for all modules to finish initialization
    for (auto& mod : modules) mod.start();
}
```

### Task Graph Execution

```cpp
void execute_task_graph(TaskGraph& graph) {
    auto barrier = std::barrier(graph.worker_count());
    for (int phase = 0; phase < graph.phase_count(); ++phase) {
        auto tasks = graph.tasks_for_phase(phase);
        std::vector<std::jthread> workers;
        for (auto& task : tasks) {
            workers.emplace_back([&task] { task.execute(); });
        }
        barrier.arriveAndWait();  // wait for all tasks in this phase to complete
    }
}
```

### Connection Pool Throttling

```cpp
class ConnectionPool {
    std::counting_semaphore sem_;
    std::vector<Connection> pool_;
public:
    ConnectionPool(size_t max_conn) : sem_(max_conn), pool_(max_conn) {}
    Connection acquire() {
        sem_.acquire();
        return pool_.back();
    }
    void release(Connection conn) {
        pool_.push_back(std::move(conn));
        sem_.release();
    }
};
```

## Compiler Support

| Compiler | Version | Support Status |
|----------|---------|----------------|
| GCC | 10+ | Full support (`-std=c++20`) |
| Clang | 16+ | Full support (`-std=c++20`) |
| MSVC | 19.29+ (VS 2019 16.10+) | Full support (`/std:c++20`) |

```cpp
// Compile verification
// g++ -std=c++20 -pthread sync.cpp -o sync
// clang++ -std=c++20 -pthread sync.cpp -o sync
// cl.exe /std:c++20 sync.cpp
```

## Summary

- `std::latch`: One-shot countdown for "wait for N events to complete."
- `std::barrier`: Reusable phase synchronization for iterative parallel computation.
- `std::counting_semaphore`: Resource pool counting for connection pools, token buckets, and throttling.
- All three are simpler, more efficient, and less error-prone than `condition_variable`.
- Prefer these high-level primitives over manually combining mutex + condition_variable.
