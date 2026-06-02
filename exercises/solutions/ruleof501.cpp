// cpplings: ruleof501 — 解答
// 主题: Rule of 5 与 Rule of 0

#include "cpplings.h"
#include <type_traits>
#include <string>
#include <memory>
#include <cstring>
#include <utility>
#include <vector>
#include <algorithm>

// === Rule of 5: 手动管理原始内存 ===

class Buffer {
    char* data_;
    std::size_t size_;

public:
    explicit Buffer(std::size_t n) : size_(n) {
        data_ = new char[n]{};
    }

    Buffer(const char* s) : size_(std::strlen(s) + 1) {
        data_ = new char[size_];
        std::memcpy(data_, s, size_);
    }

    ~Buffer() {
        delete[] data_;
    }

    Buffer(const Buffer& other) : size_(other.size_) {
        data_ = new char[size_];
        std::memcpy(data_, other.data_, size_);
    }

    Buffer& operator=(const Buffer& other) {
        if (this != &other) {
            Buffer tmp(other);
            std::swap(data_, tmp.data_);
            std::swap(size_, tmp.size_);
        }
        return *this;
    }

    Buffer(Buffer&& other) noexcept : data_(other.data_), size_(other.size_) {
        other.data_ = nullptr;
        other.size_ = 0;
    }

    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            data_ = other.data_;
            size_ = other.size_;
            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    const char* c_str() const { return data_ ? data_ : ""; }
    std::size_t size() const { return size_; }
};

// === Rule of 0: 使用智能指针和标准容器 ===

struct ManagedRecord {
    std::unique_ptr<std::string> name;
    std::vector<int> scores;
    std::string tag;

    ManagedRecord(const std::string& n, std::vector<int> s, const std::string& t)
        : name(new std::string(n)),
          scores(std::move(s)), tag(t) {}

    ManagedRecord(const ManagedRecord& other)
        : name(other.name ? new std::string(*other.name) : nullptr),
          scores(other.scores), tag(other.tag) {}

    ManagedRecord& operator=(const ManagedRecord& other) {
        if (this != &other) {
            name.reset(other.name ? new std::string(*other.name) : nullptr);
            scores = other.scores;
            tag = other.tag;
        }
        return *this;
    }

    ManagedRecord(ManagedRecord&& other) noexcept = default;
    ManagedRecord& operator=(ManagedRecord&& other) noexcept = default;

    // 移动构造和移动赋值需要显式恢复；自定义拷贝操作会抑制它们

    std::string get_name() const {
        return name ? *name : "";
    }
};

TEST("Buffer 深拷贝独立") {
    Buffer a("hello");
    Buffer b(a);
    ASSERT_EQ(std::string(b.c_str()), "hello");
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
    ASSERT_EQ(std::string(a.c_str()), "");
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
    ASSERT_EQ(std::string(a.c_str()), std::string(b.c_str()));
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
    ASSERT_TRUE(r1.name == nullptr);
}

TEST("ManagedRecord Rule of 0 trait 检查") {
    static_assert(std::is_move_constructible<ManagedRecord>::value,
                  "ManagedRecord 应可移动构造");
    static_assert(std::is_move_assignable<ManagedRecord>::value,
                  "ManagedRecord 应可移动赋值");
    static_assert(std::is_nothrow_move_constructible<ManagedRecord>::value,
                  "ManagedRecord 应可 noexcept 移动构造");
    ASSERT_TRUE(true);
}

TEST("Buffer 不是 trivially_copyable（有自定义特殊成员）") {
    static_assert(!std::is_trivially_copyable<Buffer>::value,
                  "Buffer 有自定义 dtor/copy/move，不是 trivially copyable");
    ASSERT_TRUE(true);
}

TEST("Buffer 支持移动构造") {
    static_assert(std::is_move_constructible<Buffer>::value);
    static_assert(std::is_move_assignable<Buffer>::value);
    ASSERT_TRUE(true);
}

CPPLINGS_MAIN
