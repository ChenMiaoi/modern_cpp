// Solution: 条件变量与生产者-消费者模式
// std::condition_variable 是 C++11 线程同步的核心工具，
// 配合 std::unique_lock 实现线程间的等待-通知机制。

#include "cpplings.h"
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

template <typename T>
class SafeQueue {
    std::queue<T> queue_;
    mutable std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic<bool> done_{false};

public:
    void push(T value) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            queue_.push(std::move(value));
        }
        cv_.notify_one();
    }

    bool pop(T& value) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this] { return !queue_.empty() || done_.load(); });
        if (queue_.empty()) return false;
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    void set_done() {
        done_.store(true);
        cv_.notify_all();
    }
};

// --- Tests ---

TEST("单生产者/单消费者 — 数据完整性") {
    SafeQueue<int> q;
    constexpr int count = 100;

    std::thread producer([&q]() {
        for (int i = 0; i < count; ++i) {
            q.push(i);
        }
        q.set_done();
    });

    std::vector<int> consumed;
    std::thread consumer([&q, &consumed]() {
        int val;
        while (q.pop(val)) {
            consumed.push_back(val);
        }
    });

    producer.join();
    consumer.join();

    ASSERT_EQ(consumed.size(), static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        ASSERT_EQ(consumed[i], i);
    }
}

TEST("多生产者/单消费者 — 合并结果") {
    SafeQueue<int> q;
    constexpr int per_producer = 50;
    constexpr int num_producers = 3;

    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&q, p]() {
            for (int i = 0; i < per_producer; ++i) {
                q.push(p * per_producer + i);
            }
        });
    }

    std::thread done_setter([&producers, &q]() {
        for (auto& t : producers) t.join();
        q.set_done();
    });

    std::vector<int> consumed;
    std::thread consumer([&q, &consumed]() {
        int val;
        while (q.pop(val)) {
            consumed.push_back(val);
        }
    });

    done_setter.join();
    consumer.join();

    ASSERT_EQ(consumed.size(), static_cast<size_t>(num_producers * per_producer));
}

TEST("空队列 set_done 后 pop 返回 false") {
    SafeQueue<int> q;

    std::atomic<bool> pop_result{true};
    std::thread consumer([&q, &pop_result]() {
        int val;
        pop_result.store(q.pop(val));
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    q.set_done();

    consumer.join();
    ASSERT_FALSE(pop_result.load());
}

TEST("set_done 后 push 的元素仍可消费") {
    SafeQueue<int> q;
    q.push(1);
    q.push(2);
    q.set_done();
    q.push(3);

    std::vector<int> consumed;
    int val;
    while (q.pop(val)) {
        consumed.push_back(val);
    }

    ASSERT_EQ(consumed.size(), 3u);
    ASSERT_EQ(consumed[0], 1);
    ASSERT_EQ(consumed[1], 2);
    ASSERT_EQ(consumed[2], 3);
}

CPPLINGS_MAIN
