// cpplings: expected23
// 主题: C++23 — std::expected 错误处理
//
// TODO: 使用 Expected<T,E> 进行错误处理
// 实现 parse_int 将字符串转为整数，失败时返回错误信息
//
// 提示: Expected 持有值或错误，has_value() 检查状态，
//       value_or() 提供默认值，transform() 对值做变换

#include "cpplings.h"
#include <string>
#include <type_traits>
#include <utility>

// 简易 Expected<T,E>（编译器不支持 <expected> 时的替代实现）
template <typename T, typename E>
class Expected {
    union {
        T value_;
        E error_;
    };
    bool has_value_;

public:
    // TODO: 值构造函数
    Expected(T value) {
        int _todo_ = "FILL IN THE TODO";
    }

    // TODO: 错误构造函数（用标签区分）
    struct Unexpected {};
    Expected(E error, Unexpected) {
        int _todo_ = "FILL IN THE TODO";
    }

    // TODO: has_value() — 返回是否持有值
    bool has_value() const {
        int _todo_ = "FILL IN THE TODO";
        return false;
    }

    // TODO: value() — 返回值的引用
    const T& value() const {
        int _todo_ = "FILL IN THE TODO";
        return value_;
    }

    // TODO: error() — 返回错误的引用
    const E& error() const {
        int _todo_ = "FILL IN THE TODO";
        return error_;
    }

    // TODO: value_or(default_val) — 有值返回值，否则返回默认值
    T value_or(T default_val) const {
        int _todo_ = "FILL IN THE TODO";
        return default_val;
    }

    // TODO: transform(func) — 有值时对值应用函数返回新 Expected，否则传播错误
    template <typename F>
    auto transform(F func) const -> Expected<decltype(func(std::declval<T>())), E> {
        int _todo_ = "FILL IN THE TODO";
        return Expected<decltype(func(std::declval<T>())), E>(func(value_), typename Expected<decltype(func(std::declval<T>())), E>::Unexpected{});
    }

    ~Expected() {
        if (has_value_) value_.~T();
        else error_.~E();
    }
};

// TODO: 实现 parse_int — 将字符串转为整数
// 成功返回 Expected<int, std::string> 持有值
// 失败返回 Expected<int, std::string> 持有错误信息
Expected<int, std::string> parse_int(const std::string& s) {
    int _todo_ = "FILL IN THE TODO";
    return Expected<int, std::string>("not implemented", Expected<int, std::string>::Unexpected{});
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
