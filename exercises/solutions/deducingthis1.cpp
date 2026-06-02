// cpplings: deducingthis1 — 解答
// 主题: C++23 — 显式对象参数 (Deducing this)

#include "cpplings.h"
#include <functional>
#include <string>
#include <type_traits>
#include <utility>

#if defined(__cpp_explicit_this_parameter) && __cpp_explicit_this_parameter >= 202110L

class TextWrapper {
    std::string data_;

public:
    explicit TextWrapper(std::string s) : data_(std::move(s)) {}

    template <typename Self>
    decltype(auto) get(this Self&& self) {
        if constexpr (std::is_const_v<std::remove_reference_t<Self>>) {
            return std::string{"const: "} + self.data_;
        } else {
            return std::string{"mutable: "} + self.data_;
        }
    }

    template <typename Self>
    decltype(auto) data(this Self&& self) {
        return std::forward<Self>(self).data_;
    }
};

auto factorial = [](this auto&& self, int n) -> int {
    if (n <= 1) {
        return 1;
    }
    return n * self(n - 1);
};

class Counter {
    int count_ = 0;

public:
    auto& increment(this auto& self) {
        ++static_cast<Counter&>(self).count_;
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

#else

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
