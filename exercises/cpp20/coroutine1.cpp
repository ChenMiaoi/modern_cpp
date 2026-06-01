// cpplings: coroutine1
// 主题: C++20 — 协程基础（手写 Generator）
//
// TODO: 实现手写 Generator<T> 协程类，支持 co_yield / co_return
// 实现 fibonacci 协程生成器，支持 range-based for 遍历
//
// 提示: Generator 需要 promise_type，持有 coroutine_handle
//       co_yield value → promise 存储 value 并 suspend
//       co_return → promise 标记完成

#include "cpplings.h"

#ifdef __cpp_impl_coroutines
#include <coroutine>
#include <vector>
#include <variant>
#include <cstddef>

// TODO: 实现 Generator<T> 协程类
//   需要:
//   - struct promise_type，含 get_return_object / initial_suspend / final_suspend
//   - promise 用 variant 存储当前值或完成标记
//   - yield_value 和 return_void
//   - Iterator 支持 range-based for（operator++、operator*、operator!=）
//   - done() 检查协程是否完成
//   - 析构时 destroy coroutine_handle

template <typename T>
struct Generator {
    struct promise_type {
        // TODO: 存储当前值或完成标记
        //   std::variant<std::monostate, T> value_;
        //   auto yield_value(T v) → 存值并返回 suspend_always
        //   auto get_return_object() → 返回 Generator
        //   auto initial_suspend() → 返回 suspend_never
        //   auto final_suspend() noexcept → 返回 suspend_always
        //   void return_void() {}

        int _todo_ = "请删除此行，实现上面的 TODO";
    };

    using handle_t = std::coroutine_handle<promise_type>;
    handle_t handle_;

    // TODO: 构造函数和析构函数
    //   Generator(handle_t h) : handle_(h) {}
    //   ~Generator() { if (handle_) handle_.destroy(); }
    //   禁止拷贝，允许移动

    // TODO: begin() 和 end()，支持 range-based for
    //   Iterator 需要: operator++(resume), operator*(get value), operator!=(done check)

    int _todo_ = "请删除此行，实现上面的 TODO";
};

// TODO: 实现 fibonacci 协程生成器
//   Generator<long long> fibonacci() {
//       long long a = 0, b = 1;
//       while (true) {
//           co_yield a;
//           auto tmp = a + b;
//           a = b;
//           b = tmp;
//       }
//   }

TEST("coroutine — fibonacci 前10项") {
    int _todo_ = "请删除此行，实现上面的 TODO";
    // long long expected[] = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34};
    // auto gen = fibonacci();
    // auto it = gen.begin();
    // for (int i = 0; i < 10; ++i, ++it) {
    //     ASSERT_EQ(*it, expected[i]);
    // }
}

TEST("coroutine — range-for 遍历 Generator") {
    int _todo_ = "请删除此行，实现上面的 TODO";
    // long long expected[] = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34};
    // std::vector<long long> result;
    // int count = 0;
    // for (auto val : fibonacci()) {
    //     result.push_back(val);
    //     if (++count >= 10) break;
    // }
    // ASSERT_EQ(result.size(), 10u);
    // for (int i = 0; i < 10; ++i) {
    //     ASSERT_EQ(result[i], expected[i]);
    // }
}

TEST("coroutine — Generator 耗尽后 done() 为 true") {
    int _todo_ = "请删除此行，实现上面的 TODO";
    // auto count_to = []() -> Generator<int> {
    //     for (int i = 1; i <= 5; ++i) co_yield i;
    // };
    // auto gen = count_to();
    // int last = 0;
    // for (auto v : gen) last = v;
    // ASSERT_EQ(last, 5);
}

#else
// <coroutine> 不可用时的回退：基于迭代器的 Generator

template <typename T>
struct Generator {
    struct iterator {
        // TODO: 实现基于状态机的迭代器
        //   用一个 vector<T> 存储预生成的值
        //   operator* 返回当前值
        //   operator++ 推进到下一个值
        //   operator!= 比较位置

        int _todo_ = "请删除此行，实现上面的 TODO";
    };

    // TODO: 存储生成的值序列
    //   std::vector<T> values_;

    // TODO: begin() / end()

    int _todo_ = "请删除此行，实现上面的 TODO";
};

TEST("coroutine — fibonacci 前10项 (iterator fallback)") {
    int _todo_ = "请删除此行，实现上面的 TODO";
}

TEST("coroutine — range-for 遍历 Generator (iterator fallback)") {
    int _todo_ = "请删除此行，实现上面的 TODO";
}

TEST("coroutine — Generator 耗尽后 done() 为 true (iterator fallback)") {
    int _todo_ = "请删除此行，实现上面的 TODO";
}

#endif

CPPLINGS_MAIN
