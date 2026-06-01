// Solution: std::thread 基础
// C++11 引入了标准线程库，包括 std::thread、std::mutex、std::atomic。

#include "cpplings.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

void increment_n(std::atomic<int>& counter, int n) {
    for (int i = 0; i < n; ++i) {
        ++counter;
    }
}

void add_range(std::mutex& mtx, int& total, int start, int end) {
    int local_sum = 0;
    for (int i = start; i < end; ++i) {
        local_sum += i;
    }
    std::lock_guard<std::mutex> lock(mtx);
    total += local_sum;
}

TEST("atomic 增量 — 多线程") {
    std::atomic<int> counter(0);

    std::thread t1(increment_n, std::ref(counter), 1000);
    std::thread t2(increment_n, std::ref(counter), 1000);
    std::thread t3(increment_n, std::ref(counter), 1000);

    t1.join();
    t2.join();
    t3.join();

    ASSERT_EQ(counter.load(), 3000);
}

TEST("mutex 保护共享变量") {
    std::mutex mtx;
    int total = 0;

    std::thread t1(add_range, std::ref(mtx), std::ref(total), 0, 500);
    std::thread t2(add_range, std::ref(mtx), std::ref(total), 500, 1000);

    t1.join();
    t2.join();

    // 0 + 1 + ... + 999 = 499500
    ASSERT_EQ(total, 499500);
}

TEST("thread 的 joinable 状态") {
    std::atomic<int> val(0);
    std::thread t(increment_n, std::ref(val), 1);

    ASSERT_TRUE(t.joinable());
    t.join();
    ASSERT_FALSE(t.joinable());
}

CPPLINGS_MAIN
