// cpplings: ruleof501
// 主题: Rule of 5 与 Rule of 0
//
// TODO: 实现遵循 Rule of 5 的资源管理类和遵循 Rule of 0 的类
//
// 提示: Rule of 5 — 如果你定义了 dtor/copy-ctor/copy-assign/move-ctor/move-assign 中的任意一个，
//                  你应该定义全部五个
//       Rule of 0 — 让每个成员自行管理资源（unique_ptr, string, vector 等），
//                  类不需要自定义特殊成员函数
//       is_trivially_copyable — 类型可以逐位拷贝
//       is_move_constructible — 类型支持移动构造

#include "cpplings.h"
#include <type_traits>
#include <string>
#include <memory>
#include <cstring>
#include <utility>
#include <vector>
#include <algorithm>

int _todo_ = "请删除此行，实现所有 TODO";  // 编译错误：类型不匹配

// === Rule of 5: 手动管理原始内存 ===

class Buffer {
    char* data_;
    std::size_t size_;

public:
    // 构造函数
    explicit Buffer(std::size_t n) : size_(n) {
        data_ = new char[n]{};
    }

    // 从 C 字符串构造
    Buffer(const char* s) : size_(std::strlen(s) + 1) {
        data_ = new char[size_];
        std::memcpy(data_, s, size_);
    }

    // TODO: 实现析构函数
    // 提示: delete[] data_
    ~Buffer() {
        // TODO
    }

    // TODO: 实现拷贝构造函数（深拷贝）
    // 提示: 分配新内存，复制内容
    Buffer(const Buffer& other) : size_(/* TODO */) {
        // TODO: 分配内存，复制数据
    }

    // TODO: 实现拷贝赋值运算符（深拷贝，自赋值安全）
    // 提示: copy-and-swap 惯用法
    Buffer& operator=(const Buffer& other) {
        // TODO: copy-and-swap 或手动实现
        return *this;
    }

    // TODO: 实现移动构造函数
    // 提示: 接管 other 的指针，将 other 置为空状态
    Buffer(Buffer&& other) noexcept : data_(/* TODO */), size_(/* TODO */) {
        // TODO: 将 other 置为空
    }

    // TODO: 实现移动赋值运算符
    // 提示: 释放当前资源，接管 other
    Buffer& operator=(Buffer&& other) noexcept {
        // TODO
        return *this;
    }

    const char* c_str() const { return data_ ? data_ : ""; }
    std::size_t size() const { return size_; }
};

// === Rule of 0: 使用智能指针和标准容器 ===

// TODO: 实现 ManagedRecord 类
// 使用 unique_ptr、string、vector 等 RAII 成员，不需要定义任何特殊成员函数
// 编译器生成的默认版本就是正确的
struct ManagedRecord {
    // TODO: 用 unique_ptr<std::string> 替代原始指针管理 name
    std::unique_ptr<std::string> name;

    // TODO: 用 vector 管理数据（而非原始数组）
    std::vector<int> scores;

    // TODO: 用 string 管理标签
    std::string tag;

    // TODO: 构造函数
    ManagedRecord(const std::string& n, std::vector<int> s, const std::string& t)
        : /* TODO */ name(/* TODO */), scores(/* TODO */), tag(/* TODO */) {}

    // 不需要定义 dtor、copy、move — Rule of 0!
    // 但因为有 unique_ptr，编译器不会生成拷贝构造/赋值
    // 所以我们显式实现拷贝（深拷贝）
    // TODO: 拷贝构造函数 — 深拷贝 unique_ptr
    ManagedRecord(const ManagedRecord& other)
        : /* TODO */, scores(other.scores), tag(other.tag) {}

    // TODO: 拷贝赋值
    ManagedRecord& operator=(const ManagedRecord& other) {
        // TODO
        return *this;
    }

    // 移动构造和移动赋值由编译器自动生成（Rule of 0!）

    std::string get_name() const {
        return name ? *name : "";
    }
};

// === Trait 检测 ===

TEST("Buffer 深拷贝独立") {
    Buffer a("hello");
    Buffer b(a);  // 拷贝构造
    ASSERT_EQ(std::string(b.c_str()), "hello");
    // 修改 a 不影响 b
}

TEST("Buffer 拷贝赋值自赋值安全") {
    Buffer a("self");
    a = a;
    ASSERT_EQ(std::string(a.c_str()), "self");
}

TEST("Buffer 移动构造转移所有权") {
    Buffer a("moved");
    Buffer b(std::move(a));
    ASSERT_EQ(std::string(b.c_str()), "moved");
    ASSERT_EQ(std::string(a.c_str()), "");  // 被移动后为空
}

TEST("Buffer 移动赋值") {
    Buffer a("source");
    Buffer b("dest");
    b = std::move(a);
    ASSERT_EQ(std::string(b.c_str()), "source");
    ASSERT_EQ(std::string(a.c_str()), "");
}

TEST("Buffer 拷贝后独立（修改不影响副本）") {
    Buffer a("original");
    Buffer b(a);
    // a 和 b 是独立的内存
    ASSERT_EQ(std::string(a.c_str()), std::string(b.c_str()));
    // b 析构不应影响 a
    {
        Buffer c(a);
        ASSERT_EQ(std::string(c.c_str()), "original");
    }
    ASSERT_EQ(std::string(a.c_str()), "original");
}

TEST("ManagedRecord 拷贝深拷贝 name") {
    ManagedRecord r1("Alice", {90, 85}, "A");
    ManagedRecord r2 = r1;
    ASSERT_EQ(r2.get_name(), "Alice");
    ASSERT_EQ(r2.scores.size(), 2u);
    ASSERT_EQ(r2.tag, "A");
}

TEST("ManagedRecord 移动构造") {
    ManagedRecord r1("Bob", {100}, "B");
    ManagedRecord r2 = std::move(r1);
    ASSERT_EQ(r2.get_name(), "Bob");
    ASSERT_TRUE(r1.name == nullptr);  // 移动后为空
}

TEST("ManagedRecord Rule of 0 trait 检查") {
    // 移动构造应由编译器生成
    static_assert(std::is_move_constructible_v<ManagedRecord>,
                  "ManagedRecord 应可移动构造");
    static_assert(std::is_move_assignable_v<ManagedRecord>,
                  "ManagedRecord 应可移动赋值");
    // unique_ptr 成员阻止 trivially_copyable，但应该可移动
    static_assert(std::is_nothrow_move_constructible_v<ManagedRecord>,
                  "ManagedRecord 应可 noexcept 移动构造");
    ASSERT_TRUE(true);
}

TEST("Buffer 不是 trivially_copyable（有自定义特殊成员）") {
    static_assert(!std::is_trivially_copyable_v<Buffer>,
                  "Buffer 有自定义 dtor/copy/move，不是 trivially copyable");
    ASSERT_TRUE(true);
}

TEST("Buffer 支持移动构造") {
    static_assert(std::is_move_constructible_v<Buffer>);
    static_assert(std::is_move_assignable_v<Buffer>);
    ASSERT_TRUE(true);
}

CPPLINGS_MAIN
