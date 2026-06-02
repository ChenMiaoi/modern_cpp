// cpplings: raii1 — 解答
// 主题: C++11 类与资源管理 — RAII

#include "cpplings.h"
#include <cstdio>
#include <string>
#include <type_traits>

class FileGuard {
    FILE* handle_;

public:
    // 构造函数：获取资源
    explicit FileGuard(const char* path, const char* mode)
        : handle_(std::fopen(path, mode)) {}

    // 析构函数：释放资源
    ~FileGuard() {
        if (handle_) {
            std::fclose(handle_);
        }
    }

    // 禁止拷贝
    FileGuard(const FileGuard&) = delete;
    FileGuard& operator=(const FileGuard&) = delete;

    // 移动构造函数：偷取句柄
    FileGuard(FileGuard&& other) noexcept
        : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    // 移动赋值运算符：释放自身，偷取 other
    FileGuard& operator=(FileGuard&& other) noexcept {
        if (this != &other) {
            if (handle_) std::fclose(handle_);
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    FILE* get() const { return handle_; }
    explicit operator bool() const { return handle_ != nullptr; }
};

TEST("RAII 获取和释放资源") {
    {
        FileGuard fg("cpplings_test.txt", "w");
        ASSERT_TRUE(fg.get() != nullptr);
        std::fputs("test data", fg.get());
    }
    FileGuard reader("cpplings_test.txt", "r");
    ASSERT_TRUE(reader.get() != nullptr);
    char buf[64] = {};
    std::fgets(buf, sizeof(buf), reader.get());
    ASSERT_EQ(std::string(buf), "test data");
    std::remove("cpplings_test.txt");
}

TEST("RAII 失败时返回空句柄") {
    FileGuard fg("/nonexistent/path/file.txt", "r");
    ASSERT_TRUE(!fg);
    ASSERT_TRUE(fg.get() == nullptr);
}

TEST("RAII 禁止拷贝") {
    static_assert(!std::is_copy_constructible<FileGuard>::value,
                  "FileGuard 不可拷贝构造");
    static_assert(!std::is_copy_assignable<FileGuard>::value,
                  "FileGuard 不可拷贝赋值");
    ASSERT_TRUE(true);
}

TEST("RAII 允许移动") {
    static_assert(std::is_move_constructible<FileGuard>::value,
                  "FileGuard 应可移动构造");

    FileGuard fg("cpplings_move_test.txt", "w");
    ASSERT_TRUE(fg.get() != nullptr);

    FileGuard moved = std::move(fg);
    ASSERT_TRUE(fg.get() == nullptr);
    ASSERT_TRUE(moved.get() != nullptr);

    std::remove("cpplings_move_test.txt");
}

CPPLINGS_MAIN
