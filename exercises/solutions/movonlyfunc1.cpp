// Solution — movonlyfunc1: move_only_function 移动专用可调用包装器
#include "cpplings.h"

#if __cpp_lib_move_only_function >= 202110L
#include <functional>
#include <memory>
#include <string>
#include <utility>

TEST("movonlyfunc — 存储 lambda 并调用") {
    std::move_only_function<int(int, int)> add = [](int a, int b) {
        return a + b;
    };
    ASSERT_EQ(add(3, 4), 7);
}

TEST("movonlyfunc — 存储捕获 unique_ptr 的 lambda") {
    auto up = std::make_unique<int>(42);
    std::move_only_function<int()> fn = [u = std::move(up)]() {
        return *u;
    };
    ASSERT_EQ(fn(), 42);
}

TEST("movonlyfunc — 不可拷贝只能移动") {
    std::move_only_function<int()> fn = []() { return 1; };
    auto fn2 = std::move(fn);
    ASSERT_EQ(fn2(), 1);
    ASSERT_FALSE(static_cast<bool>(fn));  // moved-from is null
}

TEST("movonlyfunc — null 检查") {
    std::move_only_function<int()> fn;
    ASSERT_FALSE(static_cast<bool>(fn));
    fn = []() { return 42; };
    ASSERT_TRUE(static_cast<bool>(fn));
}

TEST("movonlyfunc — 调用空函数") {
    std::move_only_function<int()> fn;
    ASSERT_FALSE(static_cast<bool>(fn));  // 空函数应为 falsy
    // 注意: 某些编译器实现调用空 move_only_function 会段错误而非抛异常
}

TEST("movonlyfunc — 异步回调模式") {
    struct AsyncOp {
        std::move_only_function<void(int)> callback;
        void complete(int result) { if (callback) callback(result); }
    };
    AsyncOp op;
    int captured = 0;
    op.callback = [&captured](int v) { captured = v; };
    op.complete(99);
    ASSERT_EQ(captured, 99);
}

#else
// Fallback: simple MoveOnlyFunc using type erasure

#include <memory>
#include <utility>
#include <stdexcept>

template <typename Signature>
struct MoveOnlyFunc;

template <typename R, typename... Args>
struct MoveOnlyFunc<R(Args...)> {
    struct Concept {
        virtual ~Concept() = default;
        virtual R invoke(Args... args) = 0;
    };

    template <typename F>
    struct Model : Concept {
        F func_;
        explicit Model(F f) : func_(std::move(f)) {}
        R invoke(Args... args) override {
            return func_(std::forward<Args>(args)...);
        }
    };

    std::unique_ptr<Concept> impl_;

    MoveOnlyFunc() = default;
    MoveOnlyFunc(std::nullptr_t) {}

    template <typename F>
    MoveOnlyFunc(F f) : impl_(std::make_unique<Model<F>>(std::move(f))) {}

    MoveOnlyFunc(MoveOnlyFunc&&) = default;
    MoveOnlyFunc& operator=(MoveOnlyFunc&&) = default;
    MoveOnlyFunc(const MoveOnlyFunc&) = delete;
    MoveOnlyFunc& operator=(const MoveOnlyFunc&) = delete;

    explicit operator bool() const { return impl_ != nullptr; }

    R operator()(Args... args) {
        if (!impl_) throw std::bad_function_call();
        return impl_->invoke(std::forward<Args>(args)...);
    }
};

TEST("movonlyfunc — 存储 lambda 并调用 (fallback)") {
    MoveOnlyFunc<int(int, int)> add = [](int a, int b) { return a + b; };
    ASSERT_EQ(add(3, 4), 7);
}

TEST("movonlyfunc — 存储捕获 unique_ptr 的 lambda (fallback)") {
    auto up = std::make_unique<int>(42);
    MoveOnlyFunc<int()> fn = [u = std::move(up)]() { return *u; };
    ASSERT_EQ(fn(), 42);
}

TEST("movonlyfunc — 不可拷贝只能移动 (fallback)") {
    MoveOnlyFunc<int()> fn = []() { return 1; };
    auto fn2 = std::move(fn);
    ASSERT_EQ(fn2(), 1);
    ASSERT_FALSE(static_cast<bool>(fn));
}

TEST("movonlyfunc — null 检查 (fallback)") {
    MoveOnlyFunc<int()> fn;
    ASSERT_FALSE(static_cast<bool>(fn));
    fn = []() { return 42; };
    ASSERT_TRUE(static_cast<bool>(fn));
}

TEST("movonlyfunc — 调用空函数 (fallback)") {
    MoveOnlyFunc<int()> fn;
    ASSERT_FALSE(static_cast<bool>(fn));  // 空函数应为 falsy
}

TEST("movonlyfunc — 异步回调模式 (fallback)") {
    struct AsyncOp {
        MoveOnlyFunc<void(int)> callback;
        void complete(int result) { if (callback) callback(result); }
    };
    AsyncOp op;
    int captured = 0;
    op.callback = [&captured](int v) { captured = v; };
    op.complete(99);
    ASSERT_EQ(captured, 99);
}

#endif

CPPLINGS_MAIN
