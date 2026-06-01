// cpplings: expected23 — 解答
// 主题: C++23 — std::expected 错误处理

#include "cpplings.h"
#include <string>
#include <type_traits>
#include <utility>
#include <stdexcept>

template <typename T, typename E>
class Expected {
    union {
        T value_;
        E error_;
    };
    bool has_value_;

public:
    Expected(T value) : value_(std::move(value)), has_value_(true) {}

    struct Unexpected {};
    Expected(E error, Unexpected) : error_(std::move(error)), has_value_(false) {}

    bool has_value() const { return has_value_; }

    const T& value() const {
        if (!has_value_) throw std::logic_error("no value");
        return value_;
    }

    const E& error() const {
        if (has_value_) throw std::logic_error("no error");
        return error_;
    }

    T value_or(T default_val) const {
        return has_value_ ? value_ : default_val;
    }

    template <typename F>
    auto transform(F func) const -> Expected<decltype(func(std::declval<T>())), E> {
        using U = decltype(func(std::declval<T>()));
        if (has_value_) return Expected<U, E>(func(value_));
        return Expected<U, E>(error_, typename Expected<U, E>::Unexpected{});
    }

    ~Expected() {
        if (has_value_) value_.~T();
        else error_.~E();
    }
};

Expected<int, std::string> parse_int(const std::string& s) {
    if (s.empty()) return Expected<int, std::string>(std::string("empty string"), Expected<int, std::string>::Unexpected{});
    try {
        std::size_t pos = 0;
        int val = std::stoi(s, &pos);
        if (pos != s.size()) return Expected<int, std::string>(std::string("invalid: trailing chars"), Expected<int, std::string>::Unexpected{});
        return Expected<int, std::string>(val);
    } catch (...) {
        return Expected<int, std::string>(std::string("invalid number"), Expected<int, std::string>::Unexpected{});
    }
}

TEST("expected 持有值时 has_value 为 true") {
    Expected<int, std::string> e(42);
    ASSERT_TRUE(e.has_value());
    ASSERT_EQ(e.value(), 42);
}

TEST("expected 持有错误时 has_value 为 false") {
    Expected<int, std::string> e(std::string("bad"), Expected<int, std::string>::Unexpected{});
    ASSERT_FALSE(e.has_value());
    ASSERT_EQ(e.error(), "bad");
}

TEST("value_or 在有值时返回值") {
    Expected<int, std::string> e(42);
    ASSERT_EQ(e.value_or(0), 42);
}

TEST("value_or 在无值时返回默认值") {
    Expected<int, std::string> e(std::string("err"), Expected<int, std::string>::Unexpected{});
    ASSERT_EQ(e.value_or(0), 0);
}

TEST("transform 对值做变换") {
    Expected<int, std::string> e(5);
    auto doubled = e.transform([](int v) { return v * 2; });
    ASSERT_TRUE(doubled.has_value());
    ASSERT_EQ(doubled.value(), 10);
}

TEST("parse_int 成功解析") {
    auto r = parse_int("123");
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(r.value(), 123);
}

TEST("parse_int 解析失败返回错误") {
    auto r = parse_int("abc");
    ASSERT_FALSE(r.has_value());
    ASSERT_TRUE(r.error().size() > 0);
}

CPPLINGS_MAIN
