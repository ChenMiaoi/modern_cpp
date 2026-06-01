// Exercise: 完美转发 (std::forward)
// std::forward 是 C++11 实现完美转发的关键工具，
// 它能保持参数的值类别（左值/右值）不变地传递给下一个函数。
//
// 任务:
//   1. 实现 forward_value，使用 std::forward 保持值类别
//   2. 实现 make_widget，用可变参数模板和完美转发构造对象
//   3. 实现 ref_collapse_demo 展示引用折叠规则
//
// 提示: std::forward<T>(x) 当 T 是左值引用时返回左值，否则返回右值
//       模板参数 T&& 是转发引用（universal reference），不是右值引用
//       引用折叠规则: & + & = &, && + & = &, & + && = &, && + && = &&

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

    // 移动构造函数 — 当此函数被调用时，参数是右值
    ValueTracker(ValueTracker&& other) noexcept
        : category("moved-from"), id(other.id) {
        other.category = "moved-from";
        other.id = 0;
    }

    // 拷贝构造函数 — 当此函数被调用时，参数是左值
    ValueTracker(const ValueTracker& other)
        : category("copied-from"), id(other.id) {}
};

// TODO 1: 实现 forward_value
//   template <typename T>
//   ValueTracker forward_value(T&& arg) {
//       return ValueTracker(std::forward<T>(arg));
//   }
//   当传入左值时，arg 绑定为左值引用，T 被推导为 ValueTracker&，
//   std::forward<T>(arg) 返回左值 → 调用拷贝构造。
//   当传入右值时，arg 绑定为右值引用，T 被推导为 ValueTracker，
//   std::forward<T>(arg) 返回右值 → 调用移动构造。

int _todo_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

// TODO 2: 实现 make_widget
//   template <typename T, typename... Args>
//   T make_widget(Args&&... args) {
//       return T(std::forward<Args>(args)...);
//   }
//   完美转发所有参数给 T 的构造函数。

int _todo2_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

// TODO 3: 实现 ref_collapse_demo
//   // 引用折叠规则验证函数 — 返回 4 个 bool 表示 4 种情况
//   // 1. T = int&,  arg 类型 = int&         → int& + int& = int&
//   // 2. T = int&&, arg 类型 = int&         → int&& + int& = int&
//   // 3. T = int&,  arg 类型 = int&&        → int& + int&& = int&
//   // 4. T = int&&, arg 类型 = int&&        → int&& + int&& = int&&
//   //
//   struct CollapseResults {
//       bool case1_is_lref;  // T=int&, 传入左值 → 应为 true（左值引用）
//       bool case2_is_lref;  // T=int&&,传入左值 → 应为 true（折叠为左值引用）
//       bool case3_is_lref;  // T=int&, 传入右值 → 应为 true（折叠为左值引用）
//       bool case4_is_rref;  // T=int&&,传入右值 → 应为 true（右值引用）
//   };
//   //
//   // 提示: 用一个辅助模板检测类型
//   //   template <typename T, typename Arg>
//   //   constexpr bool deduces_to_lref() {
//   //       return std::is_lvalue_reference<
//   //           decltype(std::forward<T>(std::declval<Arg>()))
//   //       >::value;
//   //   }
//   //   然后分别用不同的 T 和 Arg 实例化调用它。

int _todo3_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

// --- Tests ---

TEST("forward 保持左值类别") {
    ValueTracker original(42);
    original.category = "lvalue";

    // 传入左值 → 应该调用拷贝构造
    ValueTracker result = forward_value(original);

    ASSERT_EQ(result.category, std::string("copied-from"));
    ASSERT_EQ(result.id, 42);
    // 原始对象不应被修改
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
    // make_widget<string>(5, 'a') 等价于 string(5, 'a')
    auto s = make_widget<std::string>(5, 'a');
    ASSERT_EQ(s, std::string("aaaaa"));

    // make_widget<string>("hello") 等价于 string("hello")
    auto s2 = make_widget<std::string>("hello");
    ASSERT_EQ(s2, std::string("hello"));
}

TEST("make_widget 转发 ValueTracker") {
    // 传入右值 → 移动构造
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
