// cpplings: movonlyfunc1
// 主题: C++23 — move_only_function 移动专用可调用包装器
//
// TODO: 使用 std::move_only_function<R(Args...)> 作为移动专用可调用包装器
//   - 存储捕获 unique_ptr 的 lambda
//   - 与 std::function（需要可拷贝）对比
//   - 异步回调模式
//
// 提示: std::move_only_function<int(int)> fn = [u = std::move(up)](int x) { ... };
//       std::move_only_function<void()> fn = nullptr; — 空可调用对象
//       if (fn) fn(); — 检查是否可调用

#include "cpplings.h"

#if __cpp_lib_move_only_function >= 202110L
#include <functional>
#include <memory>
#include <string>
#include <stdexcept>
#include <utility>

// move_only_function 直接使用，无需额外实现

TEST("movonlyfunc — 存储 lambda 并调用") {
    // TODO: 创建 move_only_function 存储简单 lambda
    // std::move_only_function<int(int, int)> add = [](int a, int b) {
    //     return a + b;
    // };
    // ASSERT_EQ(add(3, 4), 7);
    int _todo_ = "请删除此行，实现上面的 TODO";
}

TEST("movonlyfunc — 存储捕获 unique_ptr 的 lambda") {
    // TODO: lambda 捕获 unique_ptr，只能通过 move_only_function 存储
    // auto up = std::make_unique<int>(42);
    // std::move_only_function<int()> fn = [u = std::move(up)]() {
    //     return *u;
    // };
    // ASSERT_EQ(fn(), 42);
    int _todo_ = "请删除此行，实现上面的 TODO";
}

TEST("movonlyfunc — 不可拷贝只能移动") {
    // TODO: 验证 move_only_function 只支持移动
    // std::move_only_function<int()> fn = []() { return 1; };
    // auto fn2 = std::move(fn);  // OK: move
    // ASSERT_EQ(fn2(), 1);
    // ASSERT_FALSE(static_cast<bool>(fn));  // moved-from is null
    int _todo_ = "请删除此行，实现上面的 TODO";
}

TEST("movonlyfunc — null 检查") {
    // TODO: 空可调用对象检查
    // std::move_only_function<int()> fn;
    // ASSERT_FALSE(static_cast<bool>(fn));
    // fn = []() { return 42; };
    // ASSERT_TRUE(static_cast<bool>(fn));
    int _todo_ = "请删除此行，实现上面的 TODO";
}

TEST("movonlyfunc — 调用空函数抛异常") {
    // TODO: 调用空 move_only_function 应抛 std::bad_function_call
    // std::move_only_function<int()> fn;
    // ASSERT_THROWS(fn(), std::bad_function_call);
    int _todo_ = "请删除此行，实现上面的 TODO";
}

TEST("movonlyfunc — 异步回调模式") {
    // TODO: 模拟异步回调 — 设置回调并触发
    // struct AsyncOp {
    //     std::move_only_function<void(int)> callback;
    //     void complete(int result) { if (callback) callback(result); }
    // };
    // AsyncOp op;
    // int captured = 0;
    // op.callback = [&captured](int v) { captured = v; };
    // op.complete(99);
    // ASSERT_EQ(captured, 99);
    int _todo_ = "请删除此行，实现上面的 TODO";
}

#else
// Fallback: simple MoveOnlyFunc using type erasure

#include <memory>
#include <utility>
#include <stdexcept>

// TODO: 实现 MoveOnlyFunc<R(Args...)>
//   - type-erase 任意可移动的 callable
//   - operator()(Args...) 调用存储的 callable
//   - operator bool() 检查是否持有 callable
//   - 空调用时抛 std::bad_function_call

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

    // TODO: 移动构造和移动赋值
    MoveOnlyFunc(MoveOnlyFunc&&) = default;
    MoveOnlyFunc& operator=(MoveOnlyFunc&&) = default;

    // 禁止拷贝
    MoveOnlyFunc(const MoveOnlyFunc&) = delete;
    MoveOnlyFunc& operator=(const MoveOnlyFunc&) = delete;

    // TODO: operator bool() — 是否持有 callable
    explicit operator bool() const {
        int _todo_ = "请删除此行，实现上面的 TODO";
        return false;
    }

    // TODO: operator()() — 调用存储的 callable，空时抛 bad_function_call
    R operator()(Args... args) {
        int _todo_ = "请删除此行，实现上面的 TODO";
        throw std::runtime_error("not implemented");
    }
};

template <typename R, typename... Args>
MoveOnlyFunc(R(*)(Args...)) -> MoveOnlyFunc<R(Args...)>;

using MoveOnlyFunc_Int = MoveOnlyFunc<int()>;

TEST("movonlyfunc — 存储 lambda 并调用 (fallback)") {
    int _todo_ = "请删除此行，实现上面的 TODO";
    // MoveOnlyFunc<int(int, int)> add = [](int a, int b) { return a + b; };
    // ASSERT_EQ(add(3, 4), 7);
}

TEST("movonlyfunc — 存储捕获 unique_ptr 的 lambda (fallback)") {
    int _todo_ = "请删除此行，实现上面的 TODO";
}

TEST("movonlyfunc — 不可拷贝只能移动 (fallback)") {
    int _todo_ = "请删除此行，实现上面的 TODO";
}

TEST("movonlyfunc — null 检查 (fallback)") {
    int _todo_ = "请删除此行，实现上面的 TODO";
}

TEST("movonlyfunc — 调用空函数抛异常 (fallback)") {
    int _todo_ = "请删除此行，实现上面的 TODO";
}

TEST("movonlyfunc — 异步回调模式 (fallback)") {
    int _todo_ = "请删除此行，实现上面的 TODO";
}

#endif

CPPLINGS_MAIN
