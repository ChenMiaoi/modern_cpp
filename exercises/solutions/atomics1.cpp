// cpplings: atomics1 — 解答
// 主题: 原子操作与内存序 — std::atomic, memory_order

#include "cpplings.h"
#include <atomic>
#include <thread>
#include <vector>

class SpinLock {
    std::atomic<bool> locked_{false};

public:
    void lock() {
        bool expected = false;
        while (!locked_.compare_exchange_weak(expected, true, std::memory_order_acquire)) {
            expected = false;
        }
    }

    void unlock() {
        locked_.store(false, std::memory_order_release);
    }
};

class AtomicCounter {
    std::atomic<int> count_{0};

public:
    void increment() {
        count_.fetch_add(1, std::memory_order_relaxed);
    }

    int add(int val) {
        return count_.fetch_add(val, std::memory_order_relaxed);
    }

    int load() const {
        return count_.load(std::memory_order_relaxed);
    }
};

TEST("atomic load 和 store") {
    std::atomic<int> val{0};
    ASSERT_EQ(val.load(), 0);
    val.store(42);
    ASSERT_EQ(val.load(), 42);
}

TEST("atomic fetch_add") {
    std::atomic<int> val{10};
    int old = val.fetch_add(5);
    ASSERT_EQ(old, 10);
    ASSERT_EQ(val.load(), 15);
}

TEST("compare_exchange_weak CAS 操作") {
    std::atomic<int> val{100};
    int expected = 100;
    bool success = val.compare_exchange_weak(expected, 200);
    ASSERT_TRUE(success);
    ASSERT_EQ(val.load(), 200);

    expected = 100;
    success = val.compare_exchange_weak(expected, 300);
    ASSERT_FALSE(success);
    ASSERT_EQ(expected, 200);
}

TEST("SpinLock 保护共享数据") {
    SpinLock lock;
    int counter = 0;
    constexpr int iterations = 1000;
    constexpr int num_threads = 4;

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < iterations; ++j) {
                lock.lock();
                ++counter;
                lock.unlock();
            }
        });
    }
    for (auto& t : threads) t.join();
    ASSERT_EQ(counter, num_threads * iterations);
}

TEST("AtomicCounter 多线程递增") {
    AtomicCounter ac;
    constexpr int iterations = 1000;
    constexpr int num_threads = 4;

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < iterations; ++j) {
                ac.increment();
            }
        });
    }
    for (auto& t : threads) t.join();
    ASSERT_EQ(ac.load(), num_threads * iterations);
}

TEST("AtomicCounter fetch_add 返回旧值") {
    AtomicCounter ac;
    int old = ac.add(10);
    ASSERT_EQ(old, 0);
    ASSERT_EQ(ac.load(), 10);
    old = ac.add(5);
    ASSERT_EQ(old, 10);
    ASSERT_EQ(ac.load(), 15);
}

CPPLINGS_MAIN
