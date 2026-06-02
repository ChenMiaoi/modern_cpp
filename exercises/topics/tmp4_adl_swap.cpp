// cpplings: tmp4
// 主题: 模板元编程 — ADL (Argument-Dependent Lookup) 与 swap 惯用法
//
// TODO: 理解 ADL 和 swap 的惯用写法
//       using std::swap; swap(a, b); — 允许用户自定义 swap 被找到
//
// 提示: ADL 在函数调用时根据参数类型查找同名函数
//       swap 惯用法: 先 using std::swap，再调用 unqualified swap

#include "cpplings.h"
#include <utility>
#include <string>

namespace mylib {

class Widget {
    int value_;
public:
    explicit Widget(int v) : value_(v) {}
    int value() const { return value_; }

    // TODO: 定义成员 swap
    void swap(Widget& other) {
        // 提示: std::swap(value_, other.value_);
        int _todo_ = "FILL IN THE TODO";
    }
};

// TODO: 定义非成员 swap (ADL 可找到)
inline void swap(Widget& a, Widget& b) {
    // 提示: a.swap(b);
    int _todo_ = "FILL IN THE TODO";
}

} // namespace mylib

template <typename T>
void adl_swap(T& a, T& b) {
    // TODO: swap 惯用法
    // 提示: using std::swap; swap(a, b);
    int _todo_ = "FILL IN THE TODO";
}

TEST("ADL swap — 自定义类型") {
    mylib::Widget a(1), b(2);
    adl_swap(a, b);
    ASSERT_EQ(a.value(), 2);
    ASSERT_EQ(b.value(), 1);
}

TEST("ADL swap — 标准类型回退到 std::swap") {
    int x = 1, y = 2;
    adl_swap(x, y);
    ASSERT_EQ(x, 2);
    ASSERT_EQ(y, 1);
}

TEST("ADL swap — string") {
    std::string s1 = "hello", s2 = "world";
    adl_swap(s1, s2);
    ASSERT_EQ(s1, "world");
    ASSERT_EQ(s2, "hello");
}

CPPLINGS_MAIN
