// cpplings: deducingthis0
// 主题: C++23 — 传统重载与 CRTP (Deducing this 的前置知识)
//
// TODO: 用传统方式实现 const/non-const 重载、递归 lambda、CRTP
// 这些是 C++23 Deducing this 所简化的传统写法
//
// 提示: 后续 deducingthis1 练习将用 C++23 显式对象参数替换这些模式

#include "cpplings.h"
#include <functional>
#include <string>
#include <type_traits>
#include <utility>

// === 传统 const/non-const 重载 ===
// TODO: 实现 TextWrapper 类，对 const/non-const 对象有不同的行为
//
// const 对象: get() 返回 "const: " + data
// 非 const 对象: get() 返回 "mutable: " + data
//
// 提示: 用两个重载成员函数，一个 const 限定，一个非 const
class TextWrapper {
    std::string data_;

public:
    explicit TextWrapper(std::string s) : data_(std::move(s)) {}

    // TODO: const 版本的 get()
    std::string get() const {
        int _todo_ = "FILL IN THE TODO";
        return "";
    }

    // TODO: 非 const 版本的 get()
    std::string get() {
        int _todo_ = "FILL IN THE TODO";
        return "";
    }
};

// === 递归 lambda ===
// TODO: 用 std::function 实现递归阶乘
// 提示: 先声明 std::function<int(int)>，再在 lambda 内递归调用它
int factorial(int n) {
    int _todo_ = "FILL IN THE TODO";
    return 0;
}

// === CRTP ===
// TODO: 实现一个计数器基类 Counter<Derived>
// 每次调用 increment() 时计数加一
// 派生类可以有自己的额外行为

template <typename Derived>
struct Counter {
    int count_ = 0;

    // TODO: increment — 计数加一，返回计数值
    int increment() {
        int _todo_ = "FILL IN THE TODO";
        return 0;
    }

    int get_count() const { return count_; }
};

struct MyCounter : Counter<MyCounter> {
    // 派生类可以添加自己的方法
    int increment_twice() {
        increment();
        return increment();
    }
};

TEST("const 对象调用 const 版本") {
    const TextWrapper tw("hello");
    ASSERT_EQ(tw.get(), "const: hello");
}

TEST("非 const 对象调用非 const 版本") {
    TextWrapper tw("hello");
    ASSERT_EQ(tw.get(), "mutable: hello");
}

TEST("递归 lambda 阶乘 — 0") {
    ASSERT_EQ(factorial(0), 1);
}

TEST("递归 lambda 阶乘 — 5") {
    ASSERT_EQ(factorial(5), 120);
}

TEST("递归 lambda 阶乘 — 10") {
    ASSERT_EQ(factorial(10), 3628800);
}

TEST("CRTP 计数器 increment") {
    MyCounter mc;
    ASSERT_EQ(mc.increment(), 1);
    ASSERT_EQ(mc.increment(), 2);
    ASSERT_EQ(mc.increment(), 3);
}

TEST("CRTP 计数器 increment_twice") {
    MyCounter mc;
    ASSERT_EQ(mc.increment_twice(), 2);
    ASSERT_EQ(mc.get_count(), 2);
}

CPPLINGS_MAIN
