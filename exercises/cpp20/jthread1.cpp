// cpplings: jthread1
// Title: std::jthread 与 stop_token
// Description: Use std::jthread for automatically joining threads and
//   stop_token for cooperative cancellation. jthread joins on
//   destruction, eliminating the forgotten-join bug.
//
// Instructions:
//   1. Create a jthread that increments an atomic counter.
//   2. Use stop_token to implement a cancellable worker.
//   3. Verify that jthread auto-joins on scope exit.
//   4. Use request_stop() to signal a worker to stop.
//   5. Delete each _todo_ guard after filling in the TODO block.
//
// Hint: std::jthread jt([](std::stop_token st) { while (!st.stop_requested()) ... });

#include "cpplings.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>

// TODO: Implement a jthread that increments an atomic counter N times.
//   std::atomic<int> counter{0};
//   {
//       std::jthread jt([&counter, n](std::stop_token) {
//           for (int i = 0; i < n; ++i) counter.fetch_add(1);
//       });
//   }  // auto-joins here

TEST("jthread — basic increment") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // std::atomic<int> counter{0};
    // {
    //     std::jthread jt([&counter](std::stop_token) {
    //         for (int i = 0; i < 1000; ++i)
    //             counter.fetch_add(1, std::memory_order_relaxed);
    //     });
    // }
    // ASSERT_EQ(counter.load(), 1000);
}

// TODO: Implement a cancellable worker using stop_token.
//   The worker should loop until stop is requested, counting iterations.
//   std::jthread jt([&count](std::stop_token st) {
//       while (!st.stop_requested()) {
//           count.fetch_add(1, std::memory_order_relaxed);
//           std::this_thread::yield();
//       }
//   });
//   // give it time to run, then jt.request_stop();

TEST("jthread — stop_token cancellation") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // std::atomic<int> count{0};
    // {
    //     std::jthread jt([&count](std::stop_token st) {
    //         while (!st.stop_requested()) {
    //             count.fetch_add(1, std::memory_order_relaxed);
    //             std::this_thread::yield();
    //         }
    //     });
    //     std::this_thread::sleep_for(std::chrono::milliseconds(10));
    //     jt.request_stop();
    // }
    // ASSERT_TRUE(count.load() > 0);
}

// TODO: Multiple jthreads incrementing a shared counter.
//   Each thread adds 1 to counter in a loop of fixed count.

TEST("jthread — multiple threads") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // std::atomic<int> counter{0};
    // {
    //     std::vector<std::jthread> threads;
    //     for (int t = 0; t < 4; ++t) {
    //         threads.emplace_back([&counter](std::stop_token) {
    //             for (int i = 0; i < 250; ++i)
    //                 counter.fetch_add(1, std::memory_order_relaxed);
    //         });
    //     }
    // }  // all auto-join
    // ASSERT_EQ(counter.load(), 1000);
}

// TODO: Auto-join verification — jthread joins before scope exit.
//   A detached/leaked thread would cause undefined behavior or test failure.

TEST("jthread — auto-join on destruction") {
    int _todo_ = "FILL IN"; (void)_todo_;
    // bool completed = false;
    // {
    //     std::jthread jt([&completed](std::stop_token) {
    //         std::this_thread::sleep_for(std::chrono::milliseconds(5));
    //         completed = true;
    //     });
    // }
    // ASSERT_TRUE(completed);  // must be true because jt joined here
}

CPPLINGS_MAIN
