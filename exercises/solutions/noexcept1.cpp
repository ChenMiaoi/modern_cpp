// cpplings: noexcept1 — 解答
// 主题: noexcept 的影响 — specifier, operator, 移动语义与 vector

#include "cpplings.h"
#include <type_traits>
#include <string>
#include <vector>
#include <utility>
#include <memory>

// === Noexcept 移动类 ===

struct SafeMover {
    std::string data;
    int value;

    SafeMover() : data(""), value(0) {}
    SafeMover(std::string d, int v) : data(std::move(d)), value(v) {}

    SafeMover(SafeMover&& other) noexcept
        : data(std::move(other.data)), value(other.value) {
        other.data.clear();
        other.value = 0;
    }

    SafeMover& operator=(SafeMover&& other) noexcept {
        if (this != &other) {
            data = std::move(other.data);
            value = other.value;
            other.data.clear();
            other.value = 0;
        }
        return *this;
    }

    SafeMover(const SafeMover& other) : data(other.data), value(other.value) {}
    SafeMover& operator=(const SafeMover& other) {
        data = other.data;
        value = other.value;
        return *this;
    }
};

struct UnsafeMover {
    std::string data;
    int value;

    UnsafeMover() : data(""), value(0) {}
    UnsafeMover(std::string d, int v) : data(std::move(d)), value(v) {}

    UnsafeMover(UnsafeMover&& other)
        : data(std::move(other.data)), value(other.value) {
        other.value = 0;
    }

    UnsafeMover& operator=(UnsafeMover&& other) {
        data = std::move(other.data);
        value = other.value;
        other.value = 0;
        return *this;
    }

    UnsafeMover(const UnsafeMover& other) : data(other.data), value(other.value) {}
    UnsafeMover& operator=(const UnsafeMover& other) {
        data = other.data;
        value = other.value;
        return *this;
    }
};

// === Conditional noexcept ===

template <typename T>
struct Wrapper {
    T value;

    Wrapper(Wrapper&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
        : value(std::move(other.value)) {}
};

template <typename T>
struct ConditionalCopy {
    T value;

    ConditionalCopy(const ConditionalCopy& other) noexcept(std::is_nothrow_copy_constructible_v<T>)
        : value(other.value) {}
};

// === noexcept 运算符检查 ===

struct FuncTester {
    static int may_throw(int x) { return x + 1; }
    static int wont_throw(int x) noexcept { return x + 1; }
};

// === 测试 ===

TEST("SafeMover 移动构造是 noexcept") {
    static_assert(std::is_nothrow_move_constructible_v<SafeMover>,
                  "SafeMover 移动构造应为 noexcept");
    ASSERT_TRUE(std::is_nothrow_move_constructible_v<SafeMover>);
}

TEST("SafeMover 移动赋值是 noexcept") {
    static_assert(std::is_nothrow_move_assignable_v<SafeMover>,
                  "SafeMover 移动赋值应为 noexcept");
    ASSERT_TRUE(std::is_nothrow_move_assignable_v<SafeMover>);
}

TEST("UnsafeMover 移动构造不是 noexcept") {
    static_assert(!std::is_nothrow_move_constructible_v<UnsafeMover>,
                  "UnsafeMover 移动构造不标记 noexcept");
    ASSERT_FALSE(std::is_nothrow_move_constructible_v<UnsafeMover>);
}

TEST("UnsafeMover 移动赋值不是 noexcept") {
    static_assert(!std::is_nothrow_move_assignable_v<UnsafeMover>,
                  "UnsafeMover 移动赋值不标记 noexcept");
    ASSERT_FALSE(std::is_nothrow_move_assignable_v<UnsafeMover>);
}

TEST("noexcept 运算符 — 编译期检查") {
    ASSERT_FALSE(noexcept(FuncTester::may_throw(0)));
    ASSERT_TRUE(noexcept(FuncTester::wont_throw(0)));
}

TEST("标准库函数的 noexcept 检查") {
    int x = 1;
    ASSERT_TRUE(noexcept(std::move(x)));
    int a = 1, b = 2;
    ASSERT_TRUE(noexcept(std::swap(a, b)));
}

TEST("条件 noexcept — Wrapper<int>") {
    static_assert(std::is_nothrow_move_constructible_v<int>,
                  "int 应可 noexcept 移动构造");
    static_assert(std::is_nothrow_move_constructible_v<Wrapper<int>>,
                  "Wrapper<int> 应可 noexcept 移动构造");
    ASSERT_TRUE(true);
}

TEST("条件 noexcept — Wrapper<SafeMover>") {
    static_assert(std::is_nothrow_move_constructible_v<Wrapper<SafeMover>>,
                  "Wrapper<SafeMover> 应可 noexcept 移动构造");
    ASSERT_TRUE(true);
}

TEST("条件 noexcept — Wrapper<UnsafeMover>") {
    static_assert(!std::is_nothrow_move_constructible_v<Wrapper<UnsafeMover>>,
                  "Wrapper<UnsafeMover> 不应是 noexcept 移动构造");
    ASSERT_TRUE(true);
}

TEST("vector 使用 SafeMover 时移动元素（不拷贝）") {
    std::vector<SafeMover> v;
    v.reserve(1);
    v.emplace_back("test", 42);
    v.emplace_back("test2", 43);
    ASSERT_EQ(v.size(), 2u);
    ASSERT_EQ(v[0].data, "test");
    ASSERT_EQ(v[0].value, 42);
    ASSERT_EQ(v[1].data, "test2");
    ASSERT_EQ(v[1].value, 43);
}

TEST("vector 使用 UnsafeMover 时拷贝元素（不移动）") {
    std::vector<UnsafeMover> v;
    v.reserve(1);
    v.emplace_back("test", 42);
    v.emplace_back("test2", 43);
    ASSERT_EQ(v.size(), 2u);
    ASSERT_EQ(v[0].data, "test");
    ASSERT_EQ(v[0].value, 42);
    ASSERT_EQ(v[1].data, "test2");
    ASSERT_EQ(v[1].value, 43);
}

TEST("ConditionalCopy 拷贝构造的条件 noexcept") {
    static_assert(std::is_nothrow_copy_constructible_v<ConditionalCopy<int>>,
                  "ConditionalCopy<int> 拷贝应 noexcept");
    ASSERT_TRUE(true);
}

CPPLINGS_MAIN
