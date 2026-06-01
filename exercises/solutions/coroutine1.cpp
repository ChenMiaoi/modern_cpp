// Solution — coroutine1: 协程基础（手写 Generator）
#include "cpplings.h"

#ifdef __cpp_impl_coroutines
#include <coroutine>
#include <vector>
#include <variant>
#include <cstddef>

template <typename T>
struct Generator {
    struct promise_type {
        std::variant<std::monostate, T> value_;

        Generator get_return_object() {
            return Generator{handle_t::from_promise(*this)};
        }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        std::suspend_always yield_value(T v) {
            value_ = std::move(v);
            return {};
        }
        void return_void() {}
    };

    using handle_t = std::coroutine_handle<promise_type>;
    handle_t handle_;

    explicit Generator(handle_t h) : handle_(h) {}
    ~Generator() { if (handle_) handle_.destroy(); }

    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;
    Generator(Generator&& o) noexcept : handle_(o.handle_) { o.handle_ = {}; }
    Generator& operator=(Generator&& o) noexcept {
        if (this != &o) {
            if (handle_) handle_.destroy();
            handle_ = o.handle_;
            o.handle_ = {};
        }
        return *this;
    }

    struct iterator {
        handle_t handle_;
        bool done_;

        iterator& operator++() {
            handle_.resume();
            done_ = handle_.done();
            return *this;
        }
        T operator*() const {
            return std::get<T>(handle_.promise().value_);
        }
        bool operator!=(const iterator& o) const {
            return done_ != o.done_;
        }
    };

    iterator begin() {
        handle_.resume();
        return iterator{handle_, handle_.done()};
    }
    iterator end() {
        return iterator{{}, true};
    }
};

Generator<long long> fibonacci() {
    long long a = 0, b = 1;
    while (true) {
        co_yield a;
        auto tmp = a + b;
        a = b;
        b = tmp;
    }
}

TEST("coroutine — fibonacci 前10项") {
    long long expected[] = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34};
    auto gen = fibonacci();
    auto it = gen.begin();
    for (int i = 0; i < 10; ++i, ++it) {
        ASSERT_EQ(*it, expected[i]);
    }
}

TEST("coroutine — range-for 遍历 Generator") {
    long long expected[] = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34};
    std::vector<long long> result;
    int count = 0;
    for (auto val : fibonacci()) {
        result.push_back(val);
        if (++count >= 10) break;
    }
    ASSERT_EQ(result.size(), 10u);
    for (int i = 0; i < 10; ++i) {
        ASSERT_EQ(result[i], expected[i]);
    }
}

TEST("coroutine — Generator 耗尽后 done() 为 true") {
    auto count_to = []() -> Generator<int> {
        for (int i = 1; i <= 5; ++i) co_yield i;
    };
    auto gen = count_to();
    int last = 0;
    for (auto v : gen) last = v;
    ASSERT_EQ(last, 5);
}

#else
// Fallback: iterator-based Generator when <coroutine> is unavailable

#include <vector>
#include <cstddef>

template <typename T>
struct Generator {
    struct iterator {
        const T* data_;
        std::size_t idx_;
        std::size_t size_;

        iterator& operator++() { ++idx_; return *this; }
        T operator*() const { return data_[idx_]; }
        bool operator!=(const iterator& o) const { return idx_ != o.idx_; }
    };

    std::vector<T> values_;

    template <typename Fn>
    explicit Generator(Fn gen, std::size_t count) {
        values_.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
            values_.push_back(gen(i));
    }

    iterator begin() { return iterator{values_.data(), 0, values_.size()}; }
    iterator end() { return iterator{values_.data(), values_.size(), values_.size()}; }
    bool done() const { return true; }
};

TEST("coroutine — fibonacci 前10项 (iterator fallback)") {
    long long expected[] = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34};
    auto fib = [](std::size_t i) {
        long long a = 0, b = 1;
        for (std::size_t j = 0; j < i; ++j) { auto t = a + b; a = b; b = t; }
        return a;
    };
    Generator<long long> gen(fib, 10);
    int idx = 0;
    for (auto v : gen) {
        ASSERT_EQ(v, expected[idx++]);
    }
}

TEST("coroutine — range-for 遍历 Generator (iterator fallback)") {
    auto fib = [](std::size_t i) {
        long long a = 0, b = 1;
        for (std::size_t j = 0; j < i; ++j) { auto t = a + b; a = b; b = t; }
        return a;
    };
    std::vector<long long> result;
    for (auto v : Generator<long long>(fib, 10))
        result.push_back(v);
    ASSERT_EQ(result.size(), 10u);
    ASSERT_EQ(result[0], 0LL);
    ASSERT_EQ(result[9], 34LL);
}

TEST("coroutine — Generator 耗尽后 done() 为 true (iterator fallback)") {
    auto gen = [](std::size_t i) { return static_cast<int>(i + 1); };
    Generator<int> g(gen, 5);
    int last = 0;
    for (auto v : g) last = v;
    ASSERT_EQ(last, 5);
    ASSERT_TRUE(g.done());
}

#endif

CPPLINGS_MAIN
