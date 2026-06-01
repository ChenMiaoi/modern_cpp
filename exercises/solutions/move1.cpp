// cpplings: move1 — 解答
// 主题: C++11 类与资源管理 — 移动语义

#include "cpplings.h"
#include <cstring>
#include <utility>

class Buffer {
    int* data_;
    std::size_t size_;

public:
    explicit Buffer(std::size_t n)
        : data_(new int[n]()), size_(n) {}

    // 拷贝构造函数
    Buffer(const Buffer& other)
        : data_(new int[other.size_]), size_(other.size_) {
        std::memcpy(data_, other.data_, size_ * sizeof(int));
    }

    // 拷贝赋值运算符
    Buffer& operator=(const Buffer& other) {
        if (this != &other) {
            delete[] data_;
            size_ = other.size_;
            data_ = new int[size_];
            std::memcpy(data_, other.data_, size_ * sizeof(int));
        }
        return *this;
    }

    // 移动构造函数：偷取 other 的资源
    Buffer(Buffer&& other) noexcept
        : data_(other.data_), size_(other.size_) {
        other.data_ = nullptr;
        other.size_ = 0;
    }

    // 移动赋值运算符：释放自身，偷取 other
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

    ~Buffer() { delete[] data_; }

    std::size_t size() const { return size_; }
    int* data() { return data_; }
    const int* data() const { return data_; }

    void set(std::size_t i, int val) { if (i < size_) data_[i] = val; }
    int get(std::size_t i) const { return (i < size_) ? data_[i] : 0; }
};

TEST("移动构造转移所有权") {
    Buffer a(3);
    a.set(0, 10); a.set(1, 20); a.set(2, 30);

    Buffer b(std::move(a));

    ASSERT_EQ(b.size(), 3u);
    ASSERT_EQ(b.get(0), 10);
    ASSERT_EQ(b.get(1), 20);

    ASSERT_EQ(a.size(), 0u);
    ASSERT_TRUE(a.data() == nullptr);
}

TEST("移动赋值转移所有权") {
    Buffer a(2);
    a.set(0, 100); a.set(1, 200);

    Buffer b(1);
    b = std::move(a);

    ASSERT_EQ(b.size(), 2u);
    ASSERT_EQ(b.get(0), 100);
    ASSERT_EQ(a.size(), 0u);
    ASSERT_TRUE(a.data() == nullptr);
}

TEST("移动不丢失数据") {
    Buffer src(4);
    for (std::size_t i = 0; i < 4; ++i) src.set(i, static_cast<int>(i * 10));

    Buffer dst = std::move(src);

    for (std::size_t i = 0; i < 4; ++i) {
        ASSERT_EQ(dst.get(i), static_cast<int>(i * 10));
    }
}

TEST("自移动赋值安全") {
    Buffer a(2);
    a.set(0, 42);
    a = std::move(a);  // 不应崩溃
    ASSERT_EQ(a.size(), 2u);
    ASSERT_EQ(a.get(0), 42);
}

CPPLINGS_MAIN
