// cpplings: noexcept1
// 主题: noexcept 的影响 — specifier, operator, 移动语义与 vector
//
// TODO: 理解和使用 noexcept 对移动语义和容器操作的影响
//
// 提示: noexcept 说明符 — 承诺函数不抛出异常
//       noexcept 运算符 — 编译期检查表达式是否 noexcept
//       vector 在扩容时：如果移动构造是 noexcept 则移动，否则拷贝（保持强保证）
//       条件 noexcept — noexcept(expr) 根据表达式决定是否 noexcept

#include "cpplings.h"
#include <type_traits>
#include <string>
#include <vector>
#include <utility>
#include <memory>

int _todo_ = "请删除此行，实现所有 TODO";  // 编译错误：类型不匹配

// === Noexcept 移动类 ===

// TODO: SafeMover — 移动操作标记 noexcept
struct SafeMover {
    std::string data;
    int value;

    SafeMover() : data(""), value(0) {}
    SafeMover(std::string d, int v) : data(std::move(d)), value(v) {}

    // TODO: 移动构造 — 标记为 noexcept
    SafeMover(SafeMover&& other) noexcept
        : data(std::move(other.data)), value(other.value) {
        // TODO: 将 other 置为空状态
    }

    // TODO: 移动赋值 — 标记为 noexcept
    SafeMover& operator=(SafeMover&& other) noexcept {
        // TODO: 转移数据
        return *this;
    }

    // 拷贝操作（不标记 noexcept）
    SafeMover(const SafeMover& other) : data(other.data), value(other.value) {}
    SafeMover& operator=(const SafeMover& other) {
        data = other.data;
        value = other.value;
        return *this;
    }
};

// TODO: UnsafeMover — 移动操作可能抛异常
struct UnsafeMover {
    std::string data;
    int value;

    UnsafeMover() : data(""), value(0) {}
    UnsafeMover(std::string d, int v) : data(std::move(d)), value(v) {}

    // TODO: 移动构造 — 不标记 noexcept（故意的）
    // 移动 string 本身是 noexcept，但我们要模拟不安全的移动
    UnsafeMover(UnsafeMover&& other)
        : data(std::move(other.data)), value(other.value) {
        other.value = 0;
    }

    // TODO: 移动赋值 — 不标记 noexcept
    UnsafeMover& operator=(UnsafeMover&& other) {
        data = std::move(other.data);
        value = other.value;
        other.value = 0;
        return *this;
    }

    // 拷贝操作
    UnsafeMover(const UnsafeMover& other) : data(other.data), value(other.value) {}
    UnsafeMover& operator=(const UnsafeMover& other) {
        data = other.data;
        value = other.value;
        return *this;
    }
};

// === Conditional noexcept ===

// TODO: 条件 noexcept 示例
// Wrapper 的移动构造是否 noexcept 取决于 T 的移动构造
template <typename T>
struct Wrapper {
    T value;

    // TODO: 条件 noexcept — 仅当 T 的移动构造是 noexcept 时才是 noexcept
    // 提示: Wrapper(Wrapper&&) noexcept(std::is_nothrow_move_constructible_v<T>)
    Wrapper(Wrapper&& other) noexcept(/* TODO 条件表达式 */)
        : value(std::move(other.value)) {}
};

// TODO: 条件 noexcept 的拷贝构造
template <typename T>
struct ConditionalCopy {
    T value;

    // TODO: 拷贝构造的条件 noexcept
    // 提示: noexcept(std::is_nothrow_copy_constructible_v<T>)
    ConditionalCopy(const ConditionalCopy& other) noexcept(/* TODO 条件表达式 */)
        : value(other.value) {}
};

// === noexcept 运算符检查 ===

// TODO: 一些测试用函数
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
    // noexcept 运算符返回 bool 常量
    ASSERT_FALSE(noexcept(FuncTester::may_throw(0)));
    ASSERT_TRUE(noexcept(FuncTester::wont_throw(0)));
}

TEST("标准库函数的 noexcept 检查") {
    // std::move 是 noexcept
    int x = 1;
    ASSERT_TRUE(noexcept(std::move(x)));
    // std::swap<int> 是 noexcept
    int a = 1, b = 2;
    ASSERT_TRUE(noexcept(std::swap(a, b)));
}

TEST("条件 noexcept — Wrapper<int>") {
    // int 移动构造是 noexcept，所以 Wrapper<int> 移动也是
    static_assert(std::is_nothrow_move_constructible_v<int>,
                  "int 应可 noexcept 移动构造");
    static_assert(std::is_nothrow_move_constructible_v<Wrapper<int>>,
                  "Wrapper<int> 应可 noexcept 移动构造");
    ASSERT_TRUE(true);
}

TEST("条件 noexcept — Wrapper<SafeMover>") {
    // SafeMover 移动构造是 noexcept
    static_assert(std::is_nothrow_move_constructible_v<Wrapper<SafeMover>>,
                  "Wrapper<SafeMover> 应可 noexcept 移动构造");
    ASSERT_TRUE(true);
}

TEST("条件 noexcept — Wrapper<UnsafeMover>") {
    // UnsafeMover 移动构造不是 noexcept，所以 Wrapper 也不是
    static_assert(!std::is_nothrow_move_constructible_v<Wrapper<UnsafeMover>>,
                  "Wrapper<UnsafeMover> 不应是 noexcept 移动构造");
    ASSERT_TRUE(true);
}

TEST("vector 使用 SafeMover 时移动元素（不拷贝）") {
    // 因为 SafeMover 的移动是 noexcept，vector 扩容时会移动而非拷贝
    std::vector<SafeMover> v;
    v.reserve(1);
    v.emplace_back("test", 42);
    // 强制扩容：如果移动是 noexcept，vector 会使用移动
    v.emplace_back("test2", 43);
    ASSERT_EQ(v.size(), 2u);
    ASSERT_EQ(v[0].data, "test");
    ASSERT_EQ(v[0].value, 42);
    ASSERT_EQ(v[1].data, "test2");
    ASSERT_EQ(v[1].value, 43);
}

TEST("vector 使用 UnsafeMover 时拷贝元素（不移动）") {
    // 因为 UnsafeMover 的移动不是 noexcept，vector 扩容时可能拷贝
    std::vector<UnsafeMover> v;
    v.reserve(1);
    v.emplace_back("test", 42);
    v.emplace_back("test2", 43);
    // 即使拷贝，结果仍然正确
    ASSERT_EQ(v.size(), 2u);
    ASSERT_EQ(v[0].data, "test");
    ASSERT_EQ(v[0].value, 42);
    ASSERT_EQ(v[1].data, "test2");
    ASSERT_EQ(v[1].value, 43);
}

TEST("ConditionalCopy 拷贝构造的条件 noexcept") {
    // int 是 trivially copyable，拷贝不抛异常
    static_assert(std::is_nothrow_copy_constructible_v<ConditionalCopy<int>>,
                  "ConditionalCopy<int> 拷贝应 noexcept");
    ASSERT_TRUE(true);
}

CPPLINGS_MAIN
