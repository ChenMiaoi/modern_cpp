// cpplings: atomics2
// 主题: 原子操作 — release sequence 与 acquire-release 语义
//
// TODO: 实现一个简单的 spinlock，使用 acquire-release 语义
//
// 提示: lock() 用 acquire load + exchange，unlock() 用 release store
//       acquire 保证看到 release 之前的所有写入

#include "cpplings.h"
#include <atomic>
#include <thread>
#include <vector>

class SpinLock {
    std::atomic<bool> locked_{false};
public:
    // TODO: 使用 acquire 语义获取锁
    void lock() {
        // 提示: while (locked_.exchange(true, std::memory_order_acquire)) { ... }
        while (locked_.exchange(true, std::memory_order_acquire)) {}
    }

    // TODO: 使用 release 语义释放锁
    void unlock() {
        // 提示: locked_.store(false, std::memory_order_release);
        locked_.store(false, std::memory_order_release);
    }
};

TEST("release sequence — 多线程计数器") {
    SpinLock mutex;
    int counter = 0;
    constexpr int N = 1000;

    auto worker = [&]() {
        for (int i = 0; i < N; ++i) {
            mutex.lock();
            ++counter;
            mutex.unlock();
        }
    };

    std::thread t1(worker);
    std::thread t2(worker);
    std::thread t3(worker);
    t1.join();
    t2.join();
    t3.join();

    ASSERT_EQ(counter, 3 * N);
}

TEST("acquire-release 语义 — happens-before 传递") {
    std::atomic<int> data{0};
    std::atomic<bool> ready{false};

    std::thread producer([&]() {
        data.store(42, std::memory_order_relaxed);
        ready.store(true, std::memory_order_release);  // release
    });

    std::thread consumer([&]() {
        while (!ready.load(std::memory_order_acquire)) {}  // acquire
        int val = data.load(std::memory_order_relaxed);
        ASSERT_EQ(val, 42);  // release 之前的写入对 acquire 之后的读取可见
    });

    producer.join();
    consumer.join();
}

CPPLINGS_MAIN
