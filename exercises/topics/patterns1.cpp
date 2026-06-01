// cpplings: patterns1
// 主题: 设计模式 — RAII Guard, 类型擦除, CRTP, Observer
//
// TODO: 实现四种经典设计模式
//
// 提示: ScopeGuard 利用析构函数做作用域清理
//       TypeErasedFunc 用虚函数做类型擦除
//       CRTP 用派生类注入基类模板参数
//       Observer 用回调列表实现事件通知

#include "cpplings.h"
#include <functional>
#include <vector>
#include <string>
#include <utility>

// === 1. ScopeGuard — RAII 作用域清理 ===
// TODO: 实现 ScopeGuard，在析构时调用注册的函数
class ScopeGuard {
    std::function<void()> action_;
    bool active_;

public:
    // TODO: 构造函数，接受一个可调用对象
    explicit ScopeGuard(std::function<void()> action) {
        int _todo_ = "FILL IN THE TODO";
    }

    // TODO: 析构函数，如果 active_ 则调用 action_
    ~ScopeGuard() {
        int _todo_ = "FILL IN THE TODO";
    }

    // TODO: dismiss — 取消执行
    void dismiss() {
        int _todo_ = "FILL IN THE TODO";
    }

    // 禁止拷贝
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
};

// === 2. TypeErasedFunc — 类型擦除的函数包装 ===
// TODO: 实现一个类似 std::function 的包装器（简化版）
// 只需支持 void() 类型
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
    // TODO: 构造函数，接受任意可调用对象
    template <typename F>
    TypeErasedFunc(F f) {
        int _todo_ = "FILL IN THE TODO";
    }

    // TODO: operator() — 调用存储的函数
    void operator()() const {
        int _todo_ = "FILL IN THE TODO";
    }

    // TODO: 析构函数
    ~TypeErasedFunc() {
        int _todo_ = "FILL IN THE TODO";
    }

    // 禁止拷贝（简化实现）
    TypeErasedFunc(const TypeErasedFunc&) = delete;
    TypeErasedFunc& operator=(const TypeErasedFunc&) = delete;
};

// === 3. CRTP — 静态多态 ===
// TODO: 实现 CRTP 基类 Shape，派生类实现 area()
template <typename Derived>
struct Shape {
    // TODO: area — 调用派生类的 area_impl
    double area() const {
        int _todo_ = "FILL IN THE TODO";
        return 0.0;
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

// === 4. Observer — 事件通知模式 ===
// TODO: 实现简单的 Observer 模式
// Event<T> 持有多个回调，触发时依次调用
template <typename... Args>
class Event {
    std::vector<std::function<void(Args...)>> handlers_;

public:
    // TODO: subscribe — 注册回调，返回 ID
    int subscribe(std::function<void(Args...)> handler) {
        int _todo_ = "FILL IN THE TODO";
        return -1;
    }

    // TODO: emit — 触发事件，调用所有回调
    void emit(Args... args) const {
        int _todo_ = "FILL IN THE TODO";
    }

    // TODO: handler_count — 返回注册的回调数量
    std::size_t handler_count() const {
        int _todo_ = "FILL IN THE TODO";
        return 0;
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
    ASSERT_EQ(result, 30);  // 10 + 20
}

TEST("Event handler_count") {
    Event<> evt;
    ASSERT_EQ(evt.handler_count(), 0u);
    evt.subscribe([]() {});
    evt.subscribe([]() {});
    ASSERT_EQ(evt.handler_count(), 2u);
}

CPPLINGS_MAIN
