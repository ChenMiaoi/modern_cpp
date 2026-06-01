// cpplings: perf1 — 解答
// 主题: 性能优化技巧 — 移动语义, SBO, 缓存友好, string_view

#include "cpplings.h"
#include <string>
#include <cstring>
#include <type_traits>
#include <utility>

// === 1. MoveTracker ===
class MoveTracker {
    int* data_;
    static int copy_count_;
    static int move_count_;

public:
    static void reset_counts() { copy_count_ = 0; move_count_ = 0; }
    static int get_copy_count() { return copy_count_; }
    static int get_move_count() { return move_count_; }

    explicit MoveTracker(int val) : data_(new int(val)) {}

    MoveTracker(const MoveTracker& other) : data_(new int(*other.data_)) {
        ++copy_count_;
    }

    MoveTracker(MoveTracker&& other) noexcept : data_(other.data_) {
        other.data_ = nullptr;
        ++move_count_;
    }

    MoveTracker& operator=(const MoveTracker& other) {
        if (this != &other) {
            delete data_;
            data_ = new int(*other.data_);
            ++copy_count_;
        }
        return *this;
    }

    MoveTracker& operator=(MoveTracker&& other) noexcept {
        if (this != &other) {
            delete data_;
            data_ = other.data_;
            other.data_ = nullptr;
            ++move_count_;
        }
        return *this;
    }

    ~MoveTracker() { delete data_; }
    int value() const { return data_ ? *data_ : 0; }
};

int MoveTracker::copy_count_ = 0;
int MoveTracker::move_count_ = 0;

// === 2. SmallBuf (SBO) ===
template <typename T, std::size_t N = 8>
class SmallBuf {
    alignas(T) char local_[N * sizeof(T)];
    T* data_;
    std::size_t size_;
    bool is_local_;

public:
    SmallBuf()
        : data_(reinterpret_cast<T*>(local_)), size_(0), is_local_(true) {}

    void push_back(T val) {
        if (is_local_ && size_ < N) {
            new (data_ + size_) T(std::move(val));
            ++size_;
        } else if (is_local_) {
            // 切换到堆
            T* heap = static_cast<T*>(operator new((size_ + 1) * sizeof(T)));
            for (std::size_t i = 0; i < size_; ++i) {
                new (heap + i) T(std::move(data_[i]));
                data_[i].~T();
            }
            new (heap + size_) T(std::move(val));
            data_ = heap;
            ++size_;
            is_local_ = false;
        } else {
            // TODO: 简化实现，实际应扩容
            T* heap = static_cast<T*>(operator new((size_ + 1) * sizeof(T)));
            for (std::size_t i = 0; i < size_; ++i) {
                new (heap + i) T(std::move(data_[i]));
                data_[i].~T();
            }
            operator delete(data_);
            new (heap + size_) T(std::move(val));
            data_ = heap;
            ++size_;
        }
    }

    std::size_t size() const { return size_; }
    T& operator[](std::size_t i) { return data_[i]; }
    const T& operator[](std::size_t i) const { return data_[i]; }
    bool is_local() const { return is_local_; }

    ~SmallBuf() {
        if (!is_local_) {
            for (std::size_t i = 0; i < size_; ++i) data_[i].~T();
            operator delete(data_);
        }
    }
};

// === 3. StringView ===
class StringView {
    const char* data_;
    std::size_t size_;

public:
    constexpr StringView() : data_(nullptr), size_(0) {}

    constexpr StringView(const char* str)
        : data_(str), size_(str ? __builtin_strlen(str) : 0) {}

    StringView(const std::string& s)
        : data_(s.data()), size_(s.size()) {}

    constexpr const char* data() const { return data_; }
    constexpr std::size_t size() const { return size_; }
    constexpr bool empty() const { return size_ == 0; }
};

TEST("MoveTracker 使用移动避免拷贝") {
    MoveTracker::reset_counts();
    MoveTracker a(42);
    MoveTracker b = std::move(a);
    ASSERT_EQ(b.value(), 42);
    ASSERT_EQ(MoveTracker::get_move_count(), 1);
    ASSERT_EQ(MoveTracker::get_copy_count(), 0);
}

TEST("MoveTracker 拷贝计数") {
    MoveTracker::reset_counts();
    MoveTracker a(10);
    MoveTracker b = a;
    ASSERT_EQ(b.value(), 10);
    ASSERT_EQ(MoveTracker::get_copy_count(), 1);
}

TEST("SmallBuf 小对象不使用堆") {
    SmallBuf<int, 8> buf;
    ASSERT_TRUE(buf.is_local());
    buf.push_back(1);
    buf.push_back(2);
    ASSERT_EQ(buf.size(), 2u);
    ASSERT_EQ(buf[0], 1);
    ASSERT_EQ(buf[1], 2);
    ASSERT_TRUE(buf.is_local());
}

TEST("SmallBuf 超出容量使用堆") {
    SmallBuf<int, 2> buf;
    buf.push_back(1);
    buf.push_back(2);
    ASSERT_TRUE(buf.is_local());
    buf.push_back(3);
    ASSERT_FALSE(buf.is_local());
    ASSERT_EQ(buf[2], 3);
}

TEST("StringView 从 C 字符串构造") {
    StringView sv("hello");
    ASSERT_EQ(sv.size(), 5u);
    ASSERT_FALSE(sv.empty());
    ASSERT_EQ(std::string(sv.data(), sv.size()), "hello");
}

TEST("StringView 从 std::string 构造零拷贝") {
    std::string s = "world";
    StringView sv(s);
    ASSERT_EQ(sv.data(), s.data());
    ASSERT_EQ(sv.size(), 5u);
}

TEST("StringView 空视图") {
    StringView sv;
    ASSERT_TRUE(sv.empty());
    ASSERT_EQ(sv.size(), 0u);
}

CPPLINGS_MAIN
