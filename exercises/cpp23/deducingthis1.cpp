// cpplings: deducingthis1
// 主题: C++23 — Deducing this (显式对象参数)
//
// TODO: 使用 C++23 的 deducing this 特性
// 实现对 const/non-const 对象的重载，以及递归 lambda
//
// 提示: C++23 允许成员函数的第一个参数为 `this Self self`
//       使得模板成员函数可以捕获对象的值类别
//       如果编译器不支持，用传统重载实现等价功能

#include "cpplings.h"
#include <string>
#include <type_traits>

// === 传统实现 (编译器不支持 deducing this 时) ===
// TODO: 实现 TextWrapper 类，对 const/non-const 对象有不同的行为
//
// const 对象: get() 返回 "const: " + data
// 非const 对象: get() 返回 "mutable: " + data
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

// === 递归 lambda 实现 ===
// TODO: 用递归 lambda 实现阶乘
// 提示: 使用 std::function 或 Y-combinator 模式
//
// 传统方式: 用一个 static 辅助函数
int factorial(int n) {
    int _todo_ = "FILL IN THE TODO";
    return 0;
}

// === CRTP 使用 deducing this 的等价实现 ===
// TODO: 实现一个计数器基类 Counter<Base>
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
