// cpplings: atomics1
// 主题: 原子操作与内存序 — std::atomic, memory_order
//
// TODO: 使用 std::atomic 实现原子操作和自旋锁
//
// 提示: atomic 提供无锁的线程安全操作
//       fetch_add 原子加法
//       compare_exchange_weak 实现 CAS 循环
//       memory_order 控制同步语义

#include "cpplings.h"
#include <atomic>
#include <thread>
#include <vector>

// TODO: 实现 SpinLock 自旋锁
class SpinLock {
    std::atomic<bool> locked_{false};

public:
    // TODO: lock — 使用 compare_exchange_weak 自旋等待
    void lock() {
        int _todo_ = "FILL IN THE TODO";
    }

    // TODO: unlock — 释放锁
    void unlock() {
        int _todo_ = "FILL IN THE TODO";
    }
};

// TODO: 实现 AtomicCounter 线程安全计数器
class AtomicCounter {
    std::atomic<int> count_{0};

public:
    // TODO: increment — 原子加一
    void increment() {
        int _todo_ = "FILL IN THE TODO";
    }

    // TODO: add — 原子加指定值，返回加之前的值
    int add(int val) {
        int _todo_ = "FILL IN THE TODO";
        return 0;
    }

    // TODO: load — 原子读取
    int load() const {
        int _todo_ = "FILL IN THE TODO";
        return 0;
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
    ASSERT_EQ(old, 10);       // 返回加之前的值
    ASSERT_EQ(val.load(), 15);
}

TEST("compare_exchange_weak CAS 操作") {
    std::atomic<int> val{100};
    int expected = 100;
    // CAS: 如果 val == expected，则设为 200
    bool success = val.compare_exchange_weak(expected, 200);
    ASSERT_TRUE(success);
    ASSERT_EQ(val.load(), 200);

    // CAS 失败: val 是 200，expected 是 100
    expected = 100;
    success = val.compare_exchange_weak(expected, 300);
    ASSERT_FALSE(success);
    ASSERT_EQ(expected, 200);  // expected 被更新为实际值
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
