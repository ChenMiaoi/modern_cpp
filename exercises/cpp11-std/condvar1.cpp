// Exercise: 条件变量与生产者-消费者模式
// std::condition_variable 是 C++11 线程同步的核心工具，
// 配合 std::unique_lock 实现线程间的等待-通知机制。
//
// 任务:
//   1. 实现 SafeQueue::push — 加锁后推入元素并通知一个等待线程
//   2. 实现 SafeQueue::pop — 等待直到队列非空或生产结束，然后取出元素
//   3. 实现 SafeQueue::set_done — 标记生产结束并唤醒所有等待线程
//
// 提示: std::condition_variable 必须配合 std::unique_lock 使用（不能用 lock_guard）
//       cv.wait(lock, predicate) 在等待前检查 predicate，处理虚假唤醒
//       cv.notify_one() 唤醒一个等待线程
//       cv.notify_all() 唤醒所有等待线程（用于终止通知）

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
    // TODO 1: 实现 push
    //   void push(T value) {
    //       {
    //           std::lock_guard<std::mutex> lock(mtx_);
    //           queue_.push(std::move(value));
    //       }
    //       cv_.notify_one();  // 唤醒一个等待的消费者
    //   }

    int _todo_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    // TODO 2: 实现 pop
    //   bool pop(T& value) {
    //       std::unique_lock<std::mutex> lock(mtx_);
    //       cv_.wait(lock, [this] { return !queue_.empty() || done_.load(); });
    //       if (queue_.empty()) return false;  // done 且队列空
    //       value = std::move(queue_.front());
    //       queue_.pop();
    //       return true;
    //   }
    //
    //   注意: unique_lock 是必需的，因为 condition_variable::wait 需要
    //   能够原子地释放和重新获取锁。lock_guard 不支持这种操作。

    int _todo2_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    // TODO 3: 实现 set_done
    //   void set_done() {
    //       done_.store(true);
    //       cv_.notify_all();  // 唤醒所有等待的消费者以便它们退出
    //   }

    int _todo3_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配
};

// --- Tests ---

TEST("单生产者/单消费者 — 数据完整性") {
    SafeQueue<int> q;
    constexpr int count = 100;

    // 生产者线程
    std::thread producer([&q]() {
        for (int i = 0; i < count; ++i) {
            q.push(i);
        }
        q.set_done();
    });

    // 消费者线程
    std::vector<int> consumed;
    std::thread consumer([&q, &consumed]() {
        int val;
        while (q.pop(val)) {
            consumed.push_back(val);
        }
    });

    producer.join();
    consumer.join();

    // 验证所有元素都被消费，且顺序正确
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

    // 等待所有生产者完成后设置 done
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

    // 总共应有 num_producers * per_producer 个元素
    ASSERT_EQ(consumed.size(), static_cast<size_t>(num_producers * per_producer));
}

TEST("空队列 set_done 后 pop 返回 false") {
    SafeQueue<int> q;

    std::atomic<bool> pop_result{true};
    std::thread consumer([&q, &pop_result]() {
        int val;
        pop_result.store(q.pop(val));  // 应该阻塞直到 set_done
    });

    // 小延迟确保消费者正在等待
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
    q.push(3);  // done 后仍可 push

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
