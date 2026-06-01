// Exercise: any1 — std::any
// 使用 std::any 存储任意类型的值，并安全地取回。
//
// 任务:
//   1. 创建 std::any，分别存储 int 和 string
//   2. 使用 std::any_cast 进行类型安全的取回
//   3. 使用 has_value() 检查是否有值
//   4. 处理 bad_any_cast 异常
// 提示: std::any_cast<T>(a) 失败时抛出 std::bad_any_cast

#include "cpplings.h"
#include <any>
#include <string>

TEST("any — 存储和取回 int") {
    std::any a = 42;
    ASSERT_TRUE(a.has_value());
    ASSERT_EQ(a.type(), typeid(int));

    // TODO: 使用 std::any_cast<int> 取回值
    int _todo_ = "请删除此行，实现上面的 TODO";
    // int val = std::any_cast<int>(a);
    // ASSERT_EQ(val, 42);
}

TEST("any — 存储和取回 string") {
    std::any a = std::string("hello");
    ASSERT_TRUE(a.has_value());

    // TODO: 使用 std::any_cast<std::string> 取回值
    int _todo_ = "请删除此行，实现上面的 TODO";
    // auto s = std::any_cast<std::string>(a);
    // ASSERT_EQ(s, "hello");
}

TEST("any — bad_any_cast") {
    std::any a = 42;
    // TODO: 尝试用错误的类型 cast，验证抛出 std::bad_any_cast
    // 提示: std::any_cast<std::string>(a) 会抛异常
    int _todo_ = "请删除此行，实现上面的 TODO";
    // ASSERT_THROWS(std::any_cast<std::string>(a), std::bad_any_cast);
}

TEST("any — reset 和 empty") {
    std::any a = 100;
    ASSERT_TRUE(a.has_value());

    a.reset();
    ASSERT_FALSE(a.has_value());

    std::any empty;
    ASSERT_FALSE(empty.has_value());

    // TODO: 对空 any 进行 any_cast 应抛出 bad_any_cast
    int _todo_ = "请删除此行，实现上面的 TODO";
    // ASSERT_THROWS(std::any_cast<int>(empty), std::bad_any_cast);
}

TEST("any — 指针形式 cast") {
    std::any a = 99;

    // TODO: 使用 std::any_cast<int>(&a) 指针形式（返回 nullptr 而非抛异常）
    int _todo_ = "请删除此行，实现上面的 TODO";
    // auto* p = std::any_cast<int>(&a);
    // ASSERT_TRUE(p != nullptr);
    // ASSERT_EQ(*p, 99);

    // auto* wrong = std::any_cast<double>(&a);
    // ASSERT_TRUE(wrong == nullptr);
}

CPPLINGS_MAIN
