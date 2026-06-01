// Exercise: std::thread 基础
// C++11 引入了标准线程库，包括 std::thread、std::mutex、std::atomic。
//
// 任务:
//   1. 实现 increment_n，接受 std::atomic<int>& 和 int n，
//      将 atomic 值自增 n 次
//   2. 实现 add_range，接受 std::mutex&、int& total、int start、int end，
//      在 mutex 保护下将 [start, end) 的和加到 total
//   3. 使用 std::thread 创建线程并 join
//
// 提示: std::thread t(func, args...) 创建并启动线程
//       t.join() 等待线程结束
//       std::atomic<int> 保证原子操作
//       std::lock_guard<std::mutex> 自动加锁/解锁

#include "cpplings.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

// TODO: 实现 increment_n
//   void increment_n(std::atomic<int>& counter, int n) {
//       for (int i = 0; i < n; ++i) {
//           ++counter;
//       }
//   }

int _todo_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

// TODO: 实现 add_range
//   void add_range(std::mutex& mtx, int& total, int start, int end) {
//       int local_sum = 0;
//       for (int i = start; i < end; ++i) {
//           local_sum += i;
//       }
//       std::lock_guard<std::mutex> lock(mtx);
//       total += local_sum;
//   }

int _todo2_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

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
