---
title: 内存模型与并发
topic: topics
feature: memory-model
status_checked_at: 2026-06-01
---
# 内存模型与并发

## C++11 内存模型

C++11 引入正式的多线程内存模型，精确回答了"当一个线程写入内存而另一个线程读取同一位置时，会发生什么"这一核心问题。在此之前，可移植的无锁代码几乎不可能正确编写。

```cpp
// 典型的数据竞争场景
int data = 0;
bool ready = false;
// 线程 A                        // 线程 B
data = 42;     // ①              while (!ready) {} // ③
ready = true;  // ②              std::cout << data; // ④ 能保证输出 42 吗？

// 没有内存模型时，编译器可能将 ② 重排到 ① 之前（store-store 重排）
// CPU 也可能以不同于程序序的顺序提交存储
```

## 六种内存序

`std::atomic` 操作接受 `std::memory_order` 参数，控制同步强度：

```cpp
std::atomic<int> x{0};

// relaxed — 仅保证原子性（不可撕裂），不建立同步关系
x.store(1, std::memory_order_relaxed);

// acquire — 读端；本线程中此 load 之后的读写不能重排到之前
int v = x.load(std::memory_order_acquire);

// release — 写端；本线程中此 store 之前的读写不能重排到之后
x.store(42, std::memory_order_release);

// acq_rel — 同时具有 acquire 和 release 语义（用于 fetch_add, CAS 等）
x.fetch_add(1, std::memory_order_acq_rel);

// seq_cst — 最强，默认值，所有操作存在全局全序
x.store(1, std::memory_order_seq_cst);

// consume — 仅保证依赖于该值的读取能看到 release 写入
// 实践中编译器通常提升为 acquire，标准委员会正在讨论其未来
```

**选择指南**：除非有性能瓶颈的证据，始终使用 `seq_cst`（默认值）。

## Happens-Before 关系

Happens-before 定义了操作之间的可见性保证：

1. **sequenced-before**：同一线程中语句的顺序
2. **synchronizes-with**：`release` 写同步到对应的 `acquire` 读
3. **传递性**：若 A happens-before B 且 B happens-before C，则 A happens-before C

```cpp
std::atomic<bool> flag{false};
int payload = 0;

// 线程 A                              // 线程 B
payload = 99;                          while (!flag.load(std::memory_order_acquire)) {}
flag.store(true, std::memory_order_release);
                                       std::cout << payload; // 保证输出 99
// ② synchronizes-with ③ → payload 的写入对 ④ 可见
```

## 原子操作

```cpp
std::atomic<int> counter{0};

// fetch_add — 原子加法，返回旧值
counter.fetch_add(1, std::memory_order_relaxed);

// C++20: 原子等待/通知（替代忙轮询）
std::atomic<bool> ready{false};

// 旧式忙轮询（C++20 之前）— 浪费 CPU：
// while (!ready.load(std::memory_order_acquire)) {
//     std::this_thread::yield();
// }

// C++20 阻塞等待 — 内核级挂起，不浪费 CPU：
while (!ready.load(std::memory_order_acquire)) {
    ready.wait(false, std::memory_order_acquire); // 若 ready 仍为 false，线程阻塞
}

// 通知端
ready.store(true, std::memory_order_release);
ready.notify_one();

// atomic_flag — 唯一保证无锁的原子类型
class spinlock {
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
public:
    void lock() noexcept {
        while (flag_.test_and_set(std::memory_order_acquire)) {}
    }
    void unlock() noexcept { flag_.clear(std::memory_order_release); }
};
```

## mutex 与 condition_variable

```cpp
template <typename T>
class blocking_queue {
    std::mutex mtx_;
    std::condition_variable cv_not_empty_, cv_not_full_;
    std::queue<T> queue_;
    std::size_t capacity_;
public:
    explicit blocking_queue(std::size_t cap) : capacity_(cap) {}

    void push(T value) {
        std::unique_lock lock(mtx_);
        cv_not_full_.wait(lock, [this] { return queue_.size() < capacity_; });
        queue_.push(std::move(value));
        cv_not_empty_.notify_one();
    }

    T pop() {
        std::unique_lock lock(mtx_);
        cv_not_empty_.wait(lock, [this] { return !queue_.empty(); });
        T val = std::move(queue_.front());
        queue_.pop();
        cv_not_full_.notify_one();
        return val;
    }
};
```

`lock_guard` 是轻量 RAII 包装，无额外开销；`unique_lock` 支持延迟加锁、手动 lock/unlock、转移所有权，配合 `condition_variable` 使用。C++17 的 `scoped_lock` 可同时锁多个互斥量并避免死锁。

## C++20 jthread 与 stop_token

`std::jthread` 在析构时自动 join，`stop_token` 提供协作式取消：

```cpp
#include <stop_token>

void worker(std::stop_token stoken) {
    while (!stoken.stop_requested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

{
    std::jthread t(worker); // 作用域结束时：请求停止 → join → 析构
}
```

## C++26 Senders/Receivers

C++26 引入 `std::execution`（P2300），提供结构化并发框架：

```cpp
auto work = std::execution::just(42)
          | std::execution::then([](int v) { return v * 2; })
          | std::execution::then([](int v) { return std::to_string(v); });

auto result = std::this_thread::sync_wait(std::move(work));

// 优势：取消通过 stop_token 自动传播，错误通过类型系统传播，
// 执行策略可组合且不与特定线程池绑定
```

## 数据竞争与未定义行为

数据竞争——两个线程同时访问同一非原子内存位置且至少一个是写入——是**未定义行为**。编译器在优化时假设不存在数据竞争，可能删除看似必要的代码：

```cpp
// ❌ UB
int shared = 0;
std::thread t1([&] { shared = 1; });
std::thread t2([&] { shared = 2; });

// ✅ 修复
std::atomic<int> safe{0};
// 或使用 std::mutex 保护
```

使用 ThreadSanitizer（`-fsanitize=thread`）检测运行时数据竞争是生产级项目的必备实践。
