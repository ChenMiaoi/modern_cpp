// cpplings: atomics4
// 主题: 性能 — false sharing 与缓存行对齐
//
// TODO: 实现缓存行对齐的计数器，避免 false sharing
//
// 提示: alignas(64) 或 std::hardware_destructive_interference_size
//       false sharing 发生在不同核心访问同一缓存行的不同变量

#include "cpplings.h"
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>

// 有 false sharing 的版本
struct BadCounters {
    std::atomic<int> counter0{0};
    std::atomic<int> counter1{0};  // 与 counter0 在同一缓存行
};

// TODO: 缓存行对齐的版本
struct AlignedCounters {
    // TODO: 用 alignas(64) 保证两个计数器不在同一缓存行
    alignas(64) std::atomic<int> counter0{0};
    alignas(64) std::atomic<int> counter1{0};
};

TEST("false sharing 概念验证") {
    // 简化测试: 验证 AlignedCounters 的计数器独立工作
    AlignedCounters ac;
    ac.counter0.store(10);
    ac.counter1.store(20);
    ASSERT_EQ(ac.counter0.load(), 10);
    ASSERT_EQ(ac.counter1.load(), 20);
}

TEST("多线程独立计数器") {
    AlignedCounters ac;
    constexpr int N = 100000;

    auto t0 = std::thread([&]() {
        for (int i = 0; i < N; ++i) ac.counter0.fetch_add(1, std::memory_order_relaxed);
    });
    auto t1 = std::thread([&]() {
        for (int i = 0; i < N; ++i) ac.counter1.fetch_add(1, std::memory_order_relaxed);
    });

    t0.join();
    t1.join();
    ASSERT_EQ(ac.counter0.load(), N);
    ASSERT_EQ(ac.counter1.load(), N);
}

CPPLINGS_MAIN
