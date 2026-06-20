---
title: C++20 同步原语（latch/barrier/semaphore）
topic: unknown
feature: synchronization-primitives
standard: N/A
status_checked_at: 2026-06-20
---
# C++20 同步原语（latch/barrier/semaphore）

## 概述

C++20 在 `<latch>`、`<barrier>`、`<semaphore>` 中引入三种高级同步原语，填补了 `condition_variable` 与原子操作之间的空白：

| 原语 | 语义 | 重用性 |
|------|------|--------|
| `std::latch` | 一次性倒数，归零时唤醒 | 不能重置 |
| `std::barrier` | 阶段性同步，每轮归零自动重置 | 可重用 |
| `std::counting_semaphore` | 资源池计数，acquire/release 控制并发度 | 无限制 |

## `std::latch`——一次性倒数

`std::latch` 类似一个倒数门闩：构造时指定计数，每次 `count_down()` 递减，`wait()` 阻塞直到计数归零。

```cpp
#include <latch>
#include <thread>
#include <iostream>
#include <vector>

int main() {
    constexpr int N = 4;
    std::latch ready(N);  // 初始计数 N
    std::vector<std::jthread> threads;

    for (int i = 0; i < N; ++i) {
        threads.emplace_back([&ready, i] {
            std::cout << "Thread " << i << " preparing...\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100 * i));
            std::cout << "Thread " << i << " ready\n";
            ready.count_down();  // 递减
        });
    }

    ready.wait();  // 阻塞直到所有线程完成初始化
    std::cout << "All threads ready, proceeding\n";
}
```

### 核心 API

| 方法 | 说明 |
|------|------|
| `latch(ptrdiff_t n)` | 构造，初始计数 `n` |
| `count_down()` | 原子递减，不阻塞 |
| `wait()` | 阻塞直到计数归零 |
| `try_wait()` | 非阻塞检查是否已归零 |
| `arriveAndWait()` | 等价于 `count_down()` + `wait()` |

```cpp
std::latch latch(1);
// 在某线程中
latch.count_down();        // 递减
latch.wait();              // 阻塞
// 或一次性操作
latch.arriveAndWait();     // 递减并等待
```

## `std::barrier`——可重用阶段同步

`barrier` 用于多线程的阶段式同步：每个阶段所有参与者到达后同步点，然后进入下一阶段。每个阶段可执行回调。

```cpp
#include <barrier>
#include <thread>
#include <iostream>
#include <vector>
#include <string>

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
                sync_point.arriveAndWait();  // 到达同步点
            }
        });
    }
}
```

### 核心 API

| 方法 | 说明 |
|------|------|
| `barrier(ptrdiff_t expected, completion_fn)` | 构造，指定参与者数量和完成回调 |
| `arrive()` | 到达但不等待 |
| `arriveAndWait()` | 到达并阻塞直到本阶段结束 |
| `wait()` | 等待当前阶段结束（不带到达语义） |
| `advance_phase()` | 推进到下一阶段（由完成回调自动调用） |

### `arrive_and_drop`——动态参与者

```cpp
std::barrier b(4);
// 线程退出时
b.arrive_and_drop();  // 从参与者集合中移除，不再要求到达
```

## `std::counting_semaphore`——资源池信号量

信号量控制对共享资源的并发访问数量，适用于连接池、令牌桶等场景。

```cpp
#include <semaphore>
#include <thread>
#include <iostream>
#include <vector>
#include <string>

int main() {
    constexpr int MAX_CONNECTIONS = 3;
    std::counting_semaphore sem(MAX_CONNECTIONS);  // 最多 3 个并发

    auto use_connection = [&](int id) {
        sem.acquire();  // P 操作（acquire）
        std::cout << "Connection " << id << " acquired\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "Connection " << id << " released\n";
        sem.release();  // V 操作（release）
    };

    std::vector<std::jthread> threads;
    for (int i = 0; i < 6; ++i) {
        threads.emplace_back(use_connection, i);
    }
}
```

### 核心 API

| 方法 | 说明 |
|------|------|
| `counting_semaphore(ptrdiff_t max)` | 构造，最大计数 `max` |
| `acquire()` | 阻塞直到计数 > 0，然后递减 |
| `try_acquire()` | 非阻塞尝试获取 |
| `try_acquire_for(duration)` | 带超时获取 |
| `try_acquire_until(time_point)` | 带绝对时间超时获取 |
| `release()` | 递增计数 |
| `release(1)` | 递增指定数量 |

### `std::binary_semaphore`——二值信号量

```cpp
// binary_semaphore 等价于 counting_semaphore<1>
std::binary_signal sem;  // 用作互斥锁替代品
```

## 与 `condition_variable` 对比

| 维度 | `condition_variable` | `latch` | `barrier` | `counting_semaphore` |
|------|---------------------|---------|-----------|---------------------|
| 用途 | 通用等待/通知 | 一次性同步 | 阶段性同步 | 资源池限流 |
| 重用 | 可重用 | 不可重用 | 按阶段重用 | 无限制 |
| 状态 | 需手动管理 | 内置计数 | 内置计数 | 内置计数 |
| 公平性 | 无保证 | FIFO | FIFO | FIFO |
| 复杂度 | 需 mutex | 无锁（底层） | 无锁（底层） | 无锁（底层） |

```cpp
// condition_variable 方式实现 latch 语义（繁琐）
std::mutex mtx;
std::condition_variable cv;
int count = 4;
// 每个线程：{ std::lock_guard lk(mtx); if (--count == 0) cv.notify_all(); }
// 等待方：{ std::unique_lock lk(mtx); cv.wait(lk, [&]{ return count == 0; }); }

// latch 方式（简洁）
std::latch latch(4);
// 每个线程：latch.count_down();
// 等待方：latch.wait();
```

## 使用场景

### 并行初始化

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
    init_done.wait();  // 等待所有模块初始化完成
    for (auto& mod : modules) mod.start();
}
```

### 任务图执行

```cpp
void execute_task_graph(TaskGraph& graph) {
    auto barrier = std::barrier(graph.worker_count());
    for (int phase = 0; phase < graph.phase_count(); ++phase) {
        auto tasks = graph.tasks_for_phase(phase);
        std::vector<std::jthread> workers;
        for (auto& task : tasks) {
            workers.emplace_back([&task] { task.execute(); });
        }
        barrier.arriveAndWait();  // 等待本阶段所有任务完成
    }
}
```

### 连接池限流

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

## 编译器支持

| 编译器 | 版本 | 支持状态 |
|--------|------|----------|
| GCC | 10+ | 完整支持（`-std=c++20`） |
| Clang | 16+ | 完整支持（`-std=c++20`） |
| MSVC | 19.29+ (VS 2019 16.10+) | 完整支持（`/std:c++20`） |

```cpp
// 编译验证
// g++ -std=c++20 -pthread sync.cpp -o sync
// clang++ -std=c++20 -pthread sync.cpp -o sync
// cl.exe /std:c++20 sync.cpp
```

## 总结

- `std::latch`：一次性倒数，适用于"等待 N 个事件全部完成"。
- `std::barrier`：可重用阶段同步，适用于迭代式并行计算。
- `std::counting_semaphore`：资源池计数，适用于连接池、令牌桶等限流场景。
- 三者均比 `condition_variable` 更简洁、更高效、更不易出错。
- 优先选择这些高层原语而非手动组合 mutex + condition_variable。
