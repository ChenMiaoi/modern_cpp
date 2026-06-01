// Solution: 完美转发 (std::forward)
// std::forward 是 C++11 实现完美转发的关键工具，
// 它能保持参数的值类别（左值/右值）不变地传递给下一个函数。

#include "cpplings.h"
#include <string>
#include <utility>
#include <type_traits>
#include <sstream>

// 用于检测值类别的辅助结构
struct ValueTracker {
    std::string category;
    int id;

    ValueTracker() : category("default"), id(0) {}
    ValueTracker(int i) : category("int"), id(i) {}

    // 移动构造函数
    ValueTracker(ValueTracker&& other) noexcept
        : category("moved-from"), id(other.id) {
        other.category = "moved-from";
        other.id = 0;
    }

    // 拷贝构造函数
    ValueTracker(const ValueTracker& other)
        : category("copied-from"), id(other.id) {}
};

// 完美转发：保持值类别传递给 ValueTracker 构造函数
template <typename T>
ValueTracker forward_value(T&& arg) {
    return ValueTracker(std::forward<T>(arg));
}

// 工厂函数：完美转发所有参数给 T 的构造函数
template <typename T, typename... Args>
T make_widget(Args&&... args) {
    return T(std::forward<Args>(args)...);
}

// 辅助模板：检测 std::forward<T>(arg) 的结果是否为左值引用
template <typename T, typename Arg>
constexpr bool deduces_to_lref() {
    return std::is_lvalue_reference<
        decltype(std::forward<T>(std::declval<Arg>()))
    >::value;
}

struct CollapseResults {
    bool case1_is_lref;
    bool case2_is_lref;
    bool case3_is_lref;
    bool case4_is_rref;
};

// 引用折叠规则验证
CollapseResults ref_collapse_demo() {
    return CollapseResults{
        // Case 1: T=int&,  Arg=int&  → int& + int& = int& (左值引用)
        deduces_to_lref<int&, int&>(),
        // Case 2: T=int&&, Arg=int&  → int&& + int& = int& (左值引用)
        deduces_to_lref<int&&, int&>(),
        // Case 3: T=int&,  Arg=int&& → int& + int&& = int& (左值引用)
        deduces_to_lref<int&, int&&>(),
        // Case 4: T=int&&, Arg=int&& → int&& + int&& = int&& (右值引用)
        // 注意: 这里返回 false 表示不是左值引用，即为右值引用
        !deduces_to_lref<int&&, int&&>()
    };
}

// --- Tests ---

TEST("forward 保持左值类别") {
    ValueTracker original(42);
    original.category = "lvalue";

    // 传入左值 → 应该调用拷贝构造
    ValueTracker result = forward_value(original);

    ASSERT_EQ(result.category, std::string("copied-from"));
    ASSERT_EQ(result.id, 42);
    ASSERT_EQ(original.category, std::string("lvalue"));
}

TEST("forward 保持右值类别") {
    ValueTracker original(42);

    // 传入右值 → 应该调用移动构造
    ValueTracker result = forward_value(std::move(original));

    ASSERT_EQ(result.category, std::string("moved-from"));
    ASSERT_EQ(result.id, 42);
}

TEST("make_widget 完美转发构造") {
    auto s = make_widget<std::string>(5, 'a');
    ASSERT_EQ(s, std::string("aaaaa"));

    auto s2 = make_widget<std::string>("hello");
    ASSERT_EQ(s2, std::string("hello"));
}

TEST("make_widget 转发 ValueTracker") {
    auto vt = make_widget<ValueTracker>(99);
    ASSERT_EQ(vt.id, 99);
}

TEST("引用折叠规则验证") {
    CollapseResults r = ref_collapse_demo();

    // Case 1: T=int&,  传入左值 → int& + int& → int& (左值引用)
    ASSERT_TRUE(r.case1_is_lref);
    // Case 2: T=int&&, 传入左值 → int&& + int& → int& (左值引用)
    ASSERT_TRUE(r.case2_is_lref);
    // Case 3: T=int&,  传入右值 → int& + int&& → int& (左值引用)
    ASSERT_TRUE(r.case3_is_lref);
    // Case 4: T=int&&, 传入右值 → int&& + int&& → int&& (右值引用)
    ASSERT_TRUE(r.case4_is_rref);
}

CPPLINGS_MAIN
