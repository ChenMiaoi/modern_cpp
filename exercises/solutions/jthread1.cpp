// Solution — jthread1: std::jthread 与 stop_token
#include "cpplings.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>

TEST("jthread — basic increment") {
    std::atomic<int> counter{0};
    {
        std::jthread jt([&counter](std::stop_token) {
            for (int i = 0; i < 1000; ++i)
                counter.fetch_add(1, std::memory_order_relaxed);
        });
    }
    ASSERT_EQ(counter.load(), 1000);
}

TEST("jthread — stop_token cancellation") {
    std::atomic<int> count{0};
    {
        std::jthread jt([&count](std::stop_token st) {
            while (!st.stop_requested()) {
                count.fetch_add(1, std::memory_order_relaxed);
                std::this_thread::yield();
            }
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        jt.request_stop();
    }
    ASSERT_TRUE(count.load() > 0);
}

TEST("jthread — multiple threads") {
    std::atomic<int> counter{0};
    {
        std::vector<std::jthread> threads;
        for (int t = 0; t < 4; ++t) {
            threads.emplace_back([&counter](std::stop_token) {
                for (int i = 0; i < 250; ++i)
                    counter.fetch_add(1, std::memory_order_relaxed);
            });
        }
    }
    ASSERT_EQ(counter.load(), 1000);
}

TEST("jthread — auto-join on destruction") {
    bool completed = false;
    {
        std::jthread jt([&completed](std::stop_token) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            completed = true;
        });
    }
    ASSERT_TRUE(completed);
}

CPPLINGS_MAIN
