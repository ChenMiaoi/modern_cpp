// cpplings: perf1
// 主题: 性能优化技巧 — 移动语义, SBO, 缓存友好, string_view
//
// TODO: 实现各种性能优化技术
//
// 提示: 移动避免拷贝
//       SBO 小对象放在栈上避免堆分配
//       string_view 零拷贝引用字符串

#include "cpplings.h"
#include <string>
#include <cstring>
#include <type_traits>
#include <vector>
#include <utility>

// === 1. 移动语义 ===
// TODO: 实现 MoveTracker，追踪拷贝/移动次数
class MoveTracker {
    int* data_;
    static int copy_count_;
    static int move_count_;

public:
    static void reset_counts() { copy_count_ = 0; move_count_ = 0; }
    static int get_copy_count() { return copy_count_; }
    static int get_move_count() { return move_count_; }

    // TODO: 构造函数
    explicit MoveTracker(int val) {
        int _todo_ = "FILL IN THE TODO";
    }

    // TODO: 拷贝构造函数 — 增加 copy_count_
    MoveTracker(const MoveTracker& other) {
        int _todo_ = "FILL IN THE TODO";
    }

    // TODO: 移动构造函数 — 增加 move_count_，偷取资源
    MoveTracker(MoveTracker&& other) noexcept {
        int _todo_ = "FILL IN THE TODO";
    }

    // TODO: 拷贝赋值
    MoveTracker& operator=(const MoveTracker& other) {
        int _todo_ = "FILL IN THE TODO";
        return *this;
    }

    // TODO: 移动赋值
    MoveTracker& operator=(MoveTracker&& other) noexcept {
        int _todo_ = "FILL IN THE TODO";
        return *this;
    }

    ~MoveTracker() { delete data_; }
    int value() const { return data_ ? *data_ : 0; }
};

int MoveTracker::copy_count_ = 0;
int MoveTracker::move_count_ = 0;

// === 2. Small Buffer Optimization (SBO) ===
// TODO: 实现 SmallBuf<T, N>
// 当数据量 <= N 时存放在栈上，否则使用堆
template <typename T, std::size_t N = 8>
class SmallBuf {
    alignas(T) char local_[N * sizeof(T)];
    T* data_;
    std::size_t size_;
    bool is_local_;

public:
    // TODO: 默认构造 — 使用本地缓冲
    SmallBuf() {
        int _todo_ = "FILL IN THE TODO";
    }

    // TODO: push_back — 添加元素
    void push_back(T val) {
        int _todo_ = "FILL IN THE TODO";
    }

    // TODO: size — 返回元素数量
    std::size_t size() const {
        int _todo_ = "FILL IN THE TODO";
        return 0;
    }

    // TODO: operator[] — 访问元素
    T& operator[](std::size_t i) { return data_[i]; }
    const T& operator[](std::size_t i) const { return data_[i]; }

    // TODO: is_local — 是否使用本地缓冲
    bool is_local() const {
        int _todo_ = "FILL IN THE TODO";
        return true;
    }

    ~SmallBuf() {
        if (!is_local_) {
            for (std::size_t i = 0; i < size_; ++i) data_[i].~T();
            operator delete(data_);
        }
    }
};

// === 3. StringView 零拷贝 ===
// TODO: 实现简单 StringView — 零拷贝的字符串引用
class StringView {
    const char* data_;
    std::size_t size_;

public:
    // TODO: 默认构造 — 空视图
    constexpr StringView() : data_(nullptr), size_(0) {}

    // TODO: 从 C 字符串构造
    constexpr StringView(const char* str) {
        int _todo_ = "FILL IN THE TODO";
    }

    // TODO: 从 std::string 构造（不拷贝）
    StringView(const std::string& s) {
        int _todo_ = "FILL IN THE TODO";
    }

    // TODO: data() — 返回数据指针
    constexpr const char* data() const { return data_; }

    // TODO: size() — 返回长度
    constexpr std::size_t size() const { return size_; }

    // TODO: empty() — 是否为空
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
    MoveTracker b = a;  // 拷贝
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
    ASSERT_TRUE(buf.is_local());  // 仍在本地缓冲
}

TEST("SmallBuf 超出容量使用堆") {
    SmallBuf<int, 2> buf;  // 只能放 2 个
    buf.push_back(1);
    buf.push_back(2);
    ASSERT_TRUE(buf.is_local());
    buf.push_back(3);  // 超出，切换到堆
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
    ASSERT_EQ(sv.data(), s.data());  // 同一地址 — 零拷贝
    ASSERT_EQ(sv.size(), 5u);
}

TEST("StringView 空视图") {
    StringView sv;
    ASSERT_TRUE(sv.empty());
    ASSERT_EQ(sv.size(), 0u);
}

CPPLINGS_MAIN
