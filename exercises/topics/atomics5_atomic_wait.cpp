// cpplings: atomics5
// 主题: 原子操作 — atomic wait/notify (C++20)
//
// TODO: 使用 atomic::wait 和 atomic::notify_one/notify_all
//
// 提示: wait(old) 会阻塞直到值不等于 old，或被 notify
//       notify_one() 唤醒一个等待者
//       notify_all() 唤醒所有等待者

#include "cpplings.h"
#include <atomic>
#include <thread>
#include <chrono>

std::atomic<int> shared_value{0};

TEST("atomic wait/notify — 生产者消费者") {
    // TODO: 使用 atomic::wait 和 notify_one 实现同步
    // 生产者: 设置值并 notify
    // 消费者: wait 直到值变化

    std::thread producer([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        shared_value.store(42, std::memory_order_release);
        shared_value.notify_one();  // 唤醒等待者
    });

    std::thread consumer([&]() {
        int expected = 0;
        shared_value.wait(expected, std::memory_order_acquire);  // 阻塞直到值 != 0
        ASSERT_EQ(shared_value.load(), 42);
    });

    producer.join();
    consumer.join();
}

TEST("atomic wait — 已满足条件不阻塞") {
    std::atomic<int> val{42};
    // 值已经是 42，wait(0) 应立即返回
    val.wait(0, std::memory_order_relaxed);  // val != 0，不阻塞
    ASSERT_EQ(val.load(), 42);
}

CPPLINGS_MAIN
