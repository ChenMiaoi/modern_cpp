// cpplings: deducingthis0 — 解答
// 主题: C++23 — 传统重载与 CRTP (Deducing this 的前置知识)

#include "cpplings.h"
#include <functional>
#include <string>
#include <type_traits>
#include <utility>

class TextWrapper {
    std::string data_;

public:
    explicit TextWrapper(std::string s) : data_(std::move(s)) {}

    std::string get() const {
        return "const: " + data_;
    }

    std::string get() {
        return "mutable: " + data_;
    }
};

int factorial(int n) {
    std::function<int(int)> fac = [&](int x) -> int {
        if (x <= 1) return 1;
        return x * fac(x - 1);
    };
    return fac(n);
}

template <typename Derived>
struct Counter {
    int count_ = 0;

    int increment() {
        return ++count_;
    }

    int get_count() const { return count_; }
};

struct MyCounter : Counter<MyCounter> {
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
