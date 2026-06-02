// cpplings: atomics3
// 主题: 原子操作 — CAS (compare_exchange_weak/strong) 失败序
//
// TODO: 使用 compare_exchange_weak 实现无锁栈
//
// 提示: compare_exchange_weak(expected, desired, success_order, failure_order)
//       weak 版本在 spurious failure 时返回 false 但不修改 expected
//       strong 版本永不 spurious failure

#include "cpplings.h"
#include <atomic>
#include <memory>
#include <thread>

template <typename T>
class LockFreeStack {
    struct Node {
        T data;
        Node* next;
        Node(T d, Node* n) : data(std::move(d)), next(n) {}
    };
    std::atomic<Node*> head_{nullptr};
public:
    // TODO: 使用 CAS 实现 push
    void push(T value) {
        // 提示:
        // Node* node = new Node(std::move(value), nullptr);
        // Node* old_head = head_.load(std::memory_order_relaxed);
        // do {
        //     node->next = old_head;
        // } while (!head_.compare_exchange_weak(old_head, node,
        //     std::memory_order_release, std::memory_order_relaxed));
        int _todo_ = "FILL IN THE TODO";
    }

    // TODO: 使用 CAS 实现 pop
    bool pop(T& result) {
        // 提示: 类似 push，但需要处理 ABA 问题的简单版本
        int _todo_ = "FILL IN THE TODO";
        return false;
    }
};

TEST("CAS 无锁栈 — push/pop 基本操作") {
    LockFreeStack<int> stack;
    stack.push(1);
    stack.push(2);
    stack.push(3);
    int val;
    ASSERT_TRUE(stack.pop(val));
    ASSERT_EQ(val, 3);
    ASSERT_TRUE(stack.pop(val));
    ASSERT_EQ(val, 2);
    ASSERT_TRUE(stack.pop(val));
    ASSERT_EQ(val, 1);
    ASSERT_FALSE(stack.pop(val));
}

TEST("CAS 无锁栈 — 多线程") {
    LockFreeStack<int> stack;
    constexpr int N = 10000;

    std::thread producer([&]() {
        for (int i = 0; i < N; ++i) {
            stack.push(i);
        }
    });

    int count = 0;
    std::thread consumer([&]() {
        int val;
        while (stack.pop(val)) {
            ++count;
        }
    });

    producer.join();
    consumer.join();
    // count 可能小于 N，因为 consumer 可能先跑完
    ASSERT_TRUE(count > 0);
}

CPPLINGS_MAIN
