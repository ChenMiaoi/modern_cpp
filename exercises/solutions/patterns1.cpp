// cpplings: patterns1 — 解答
// 主题: 设计模式 — RAII Guard, 类型擦除, CRTP, Observer

#include "cpplings.h"
#include <functional>
#include <vector>
#include <string>
#include <utility>

// === 1. ScopeGuard ===
class ScopeGuard {
    std::function<void()> action_;
    bool active_;

public:
    explicit ScopeGuard(std::function<void()> action)
        : action_(std::move(action)), active_(true) {}

    ~ScopeGuard() {
        if (active_) action_();
    }

    void dismiss() { active_ = false; }

    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
};

// === 2. TypeErasedFunc ===
class TypeErasedFunc {
    struct Concept {
        virtual ~Concept() = default;
        virtual void call() = 0;
    };

    template <typename F>
    struct Model : Concept {
        F func_;
        explicit Model(F f) : func_(std::move(f)) {}
        void call() override { func_(); }
    };

    Concept* impl_;

public:
    template <typename F>
    TypeErasedFunc(F f) : impl_(new Model<F>(std::move(f))) {}

    void operator()() const { impl_->call(); }

    ~TypeErasedFunc() { delete impl_; }

    TypeErasedFunc(const TypeErasedFunc&) = delete;
    TypeErasedFunc& operator=(const TypeErasedFunc&) = delete;
};

// === 3. CRTP ===
template <typename Derived>
struct Shape {
    double area() const {
        return static_cast<const Derived*>(this)->area_impl();
    }
};

struct Circle : Shape<Circle> {
    double radius;
    explicit Circle(double r) : radius(r) {}
    double area_impl() const { return 3.14159265358979 * radius * radius; }
};

struct Square : Shape<Square> {
    double side;
    explicit Square(double s) : side(s) {}
    double area_impl() const { return side * side; }
};

// === 4. Observer ===
template <typename... Args>
class Event {
    std::vector<std::function<void(Args...)>> handlers_;

public:
    int subscribe(std::function<void(Args...)> handler) {
        int id = static_cast<int>(handlers_.size());
        handlers_.push_back(std::move(handler));
        return id;
    }

    void emit(Args... args) const {
        for (auto& h : handlers_) {
            h(args...);
        }
    }

    std::size_t handler_count() const {
        return handlers_.size();
    }
};

TEST("ScopeGuard 析构时执行动作") {
    int val = 0;
    {
        ScopeGuard guard([&]() { val = 42; });
        ASSERT_EQ(val, 0);
    }
    ASSERT_EQ(val, 42);
}

TEST("ScopeGuard dismiss 取消执行") {
    int val = 0;
    {
        ScopeGuard guard([&]() { val = 42; });
        guard.dismiss();
    }
    ASSERT_EQ(val, 0);
}

TEST("TypeErasedFunc 调用存储的函数") {
    int val = 0;
    TypeErasedFunc func([&]() { val = 99; });
    func();
    ASSERT_EQ(val, 99);
}

TEST("CRTP 静态多态 — Circle") {
    Circle c(1.0);
    double a = c.area();
    ASSERT_TRUE(a > 3.14 && a < 3.15);
}

TEST("CRTP 静态多态 — Square") {
    Square s(5.0);
    ASSERT_EQ(s.area(), 25.0);
}

TEST("Event 订阅和触发") {
    Event<int> evt;
    int result = 0;
    evt.subscribe([&](int v) { result += v; });
    evt.subscribe([&](int v) { result += v * 2; });
    evt.emit(10);
    ASSERT_EQ(result, 30);
}

TEST("Event handler_count") {
    Event<> evt;
    ASSERT_EQ(evt.handler_count(), 0u);
    evt.subscribe([]() {});
    evt.subscribe([]() {});
    ASSERT_EQ(evt.handler_count(), 2u);
}

CPPLINGS_MAIN
