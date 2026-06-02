---
title: Memory Model and Concurrency
topic: topics
feature: memory-model
status_checked_at: 2026-06-01
standard: N/A
---

# Memory Model and Concurrency

## C++11 Memory Model

C++11 introduces a formal multi-threaded memory model that precisely answers the core question: "when one thread writes to memory and another thread reads the same location, what happens?" Before this, writing correct portable lock-free code was nearly impossible.

```cpp
// A typical data race scenario
int data = 0;
bool ready = false;
// Thread A                         // Thread B
data = 42;     // ①                 while (!ready) {} // ③
ready = true;  // ②                 std::cout << data; // ④ — is the output guaranteed to be 42?

// Without a memory model, the compiler might reorder ② before ① (store-store reordering)
// The CPU may also commit stores in a different order than program order
```

## Six Memory Orders

`std::atomic` operations accept a `std::memory_order` parameter that controls synchronization strength:

```cpp
std::atomic<int> x{0};

// relaxed — only guarantees atomicity (no tearing), does not establish synchronization
x.store(1, std::memory_order_relaxed);

// acquire — load side; reads and writes after this load in the same thread cannot be reordered before it
int v = x.load(std::memory_order_acquire);

// release — store side; reads and writes before this store in the same thread cannot be reordered after it
x.store(42, std::memory_order_release);

// acq_rel — has both acquire and release semantics (used with fetch_add, CAS, etc.)
x.fetch_add(1, std::memory_order_acq_rel);

// seq_cst — strongest, the default; all operations share a single global total order
x.store(1, std::memory_order_seq_cst);

// consume — only guarantees that reads that depend on the loaded value see the release store
// In practice compilers typically promote this to acquire; the standards committee is discussing its future
```

**Selection guide**: unless you have evidence of a performance bottleneck, always use `seq_cst` (the default).

## Happens-Before Relationship

Happens-before defines visibility guarantees between operations:

1. **sequenced-before**: the order of statements within the same thread
2. **synchronizes-with**: a `release` store synchronizes with the corresponding `acquire` load
3. **transitivity**: if A happens-before B and B happens-before C, then A happens-before C

```cpp
std::atomic<bool> flag{false};
int payload = 0;

// Thread A                                   // Thread B
payload = 99;                                 while (!flag.load(std::memory_order_acquire)) {}
flag.store(true, std::memory_order_release);
                                              std::cout << payload; // guaranteed to output 99
// ② synchronizes-with ③ → the write to payload is visible at ④
```

## Atomic Operations

```cpp
std::atomic<int> counter{0};

// fetch_add — atomic addition, returns the previous value
counter.fetch_add(1, std::memory_order_relaxed);

// C++20: atomic wait/notify (replaces busy-wait polling)
std::atomic<bool> ready{false};

// Legacy busy-wait (before C++20) — wastes CPU:
// while (!ready.load(std::memory_order_acquire)) {
//     std::this_thread::yield();
// }

// C++20 blocking wait — kernel-level suspension, does not waste CPU:
while (!ready.load(std::memory_order_acquire)) {
    ready.wait(false, std::memory_order_acquire); // if ready is still false, the thread blocks
}

// Notify side
ready.store(true, std::memory_order_release);
ready.notify_one();

// atomic_flag — the only atomic type guaranteed to be lock-free
class spinlock {
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
public:
    void lock() noexcept {
        while (flag_.test_and_set(std::memory_order_acquire)) {}
    }
    void unlock() noexcept { flag_.clear(std::memory_order_release); }
};
```

## mutex and condition_variable

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

`lock_guard` is a lightweight RAII wrapper with no overhead; `unique_lock` supports deferred locking, manual lock/unlock, and ownership transfer, and is used with `condition_variable`. C++17's `scoped_lock` can lock multiple mutexes simultaneously while avoiding deadlocks.

## C++20 jthread and stop_token

`std::jthread` automatically joins on destruction; `stop_token` provides cooperative cancellation:

```cpp
#include <stop_token>

void worker(std::stop_token stoken) {
    while (!stoken.stop_requested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

{
    std::jthread t(worker); // on scope exit: request stop → join → destroy
}
```

## C++26 Senders/Receivers

C++26 introduces `std::execution` (P2300), providing a structured concurrency framework:

```cpp
auto work = std::execution::just(42)
          | std::execution::then([](int v) { return v * 2; })
          | std::execution::then([](int v) { return std::to_string(v); });

auto result = std::this_thread::sync_wait(std::move(work));

// Benefits: cancellation propagates automatically via stop_token, errors propagate
// through the type system, and execution policies are composable and not bound to a specific thread pool
```

## Data Races and Undefined Behavior

A data race — two threads simultaneously accessing the same non-atomic memory location with at least one being a write — is **undefined behavior**. The compiler assumes no data races exist when optimizing and may remove seemingly necessary code:

```cpp
// ❌ UB
int shared = 0;
std::thread t1([&] { shared = 1; });
std::thread t2([&] { shared = 2; });

// ✅ Fix
std::atomic<int> safe{0};
// or protect with std::mutex
```

Using ThreadSanitizer (`-fsanitize=thread`) to detect runtime data races is an essential practice for production-grade projects.
