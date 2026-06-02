// cpplings: deducingthis1
// 主题: C++23 — 显式对象参数 (Deducing this)
//
// TODO: 使用 C++23 的显式对象参数 (deducing this)
//   - 合并 const/non-const 重载为单一模板成员函数
//   - 用递归 lambda (this auto&& self)
//   - 简化 CRTP
//
// 提示: template <typename Self> decltype(auto) get(this Self&& self)
//       Self 自动推导为 T / const T / T& / const T&

#include "cpplings.h"
#include <functional>
#include <string>
#include <type_traits>
#include <utility>

#if defined(__cpp_explicit_this_parameter) && __cpp_explicit_this_parameter >= 202110L

// === 使用显式对象参数合并重载 ===
class TextWrapper {
    std::string data_;

public:
    explicit TextWrapper(std::string s) : data_(std::move(s)) {}

    // TODO: 用一个模板成员函数替换 const/non-const 两个重载
    // Self 将被推导为 TextWrapper / const TextWrapper
    // 返回 "const: " + data_ 或 "mutable: " + data_
    template <typename Self>
    decltype(auto) get(this Self&& self) {
        int _todo_ = "FILL IN THE TODO";
        return std::string{};
    }

    // TODO: data() 访问器 — 完美转发对象自身
    template <typename Self>
    decltype(auto) data(this Self&& self) {
        int _todo_ = "FILL IN THE TODO";
        return std::string{};
    }
};

// === 递归 lambda (使用 this auto&& self) ===
// TODO: 用 this auto&& self 实现递归阶乘
auto factorial = [](this auto&& self, int n) -> int {
    int _todo_ = "FILL IN THE TODO";
    return 0;
};

// === 使用显式对象参数简化 CRTP ===
class Counter {
    int count_ = 0;

public:
    // TODO: increment — 不再需要模板基类
    // 使用 this auto& self 返回派生类引用以支持链式调用
    auto increment(this auto& self) -> decltype(self) {
        int _todo_ = "FILL IN THE TODO";
        return self;
    }

    int get_count() const { return count_; }
};

struct MyCounter : Counter {
    int increment_twice() {
        increment();
        return increment().get_count();
    }
};

#else // Fallback for compilers without deducing this support

// Fallback: traditional overloads / std::function / CRTP 版本

class TextWrapper {
    std::string data_;

public:
    explicit TextWrapper(std::string s) : data_(std::move(s)) {}

    std::string get() const { return "const: " + data_; }
    std::string get() { return "mutable: " + data_; }
    const std::string& data() const { return data_; }
    std::string& data() { return data_; }
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

    Derived& increment() {
        ++count_;
        return static_cast<Derived&>(*this);
    }

    int get_count() const { return count_; }
};

struct MyCounter : Counter<MyCounter> {
    int increment_twice() {
        increment();
        return increment().get_count();
    }
};

#endif

TEST("const 对象调用 const 行为") {
    const TextWrapper tw("hello");
    ASSERT_EQ(tw.get(), "const: hello");
}

TEST("非 const 对象调用 mutable 行为") {
    TextWrapper tw("hello");
    ASSERT_EQ(tw.get(), "mutable: hello");
}

TEST("递归 lambda/函数 阶乘 — 0") {
    ASSERT_EQ(factorial(0), 1);
}

TEST("递归 lambda/函数 阶乘 — 5") {
    ASSERT_EQ(factorial(5), 120);
}

TEST("递归 lambda/函数 阶乘 — 10") {
    ASSERT_EQ(factorial(10), 3628800);
}

TEST("Counter increment") {
    MyCounter mc;
    ASSERT_EQ(mc.increment().get_count(), 1);
    ASSERT_EQ(mc.increment().get_count(), 2);
    ASSERT_EQ(mc.increment().get_count(), 3);
}

TEST("Counter increment_twice") {
    MyCounter mc;
    mc.increment_twice();
    ASSERT_EQ(mc.get_count(), 2);
}

CPPLINGS_MAIN
