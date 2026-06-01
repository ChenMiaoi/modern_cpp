// cpplings: raii1
// 主题: C++11 类与资源管理 — RAII
//
// TODO: 实现一个 RAII 风格的文件句柄包装类 FileGuard
// 让所有断言通过
//
// 提示: RAII = 资源获取即初始化。构造函数获取资源（fopen），
//       析构函数释放资源（fclose）。禁止拷贝，允许移动。

#include "cpplings.h"
#include <cstdio>
#include <string>
#include <type_traits>

class FileGuard {
    FILE* handle_;

public:
    // TODO: 构造函数 — 用 fopen 打开文件，获取资源
    explicit FileGuard(const char* path, const char* mode) {
        int _todo_ = "FILL IN THE TODO";
    }

    // TODO: 析构函数 — 如果 handle_ 有效，用 fclose 释放资源
    ~FileGuard() {
        int _todo_ = "FILL IN THE TODO";
    }

    // 禁止拷贝（已提供）
    FileGuard(const FileGuard&) = delete;
    FileGuard& operator=(const FileGuard&) = delete;

    // TODO: 移动构造函数 — 偷取 other 的句柄，将 other 置为空
    FileGuard(FileGuard&& other) noexcept {
        int _todo_ = "FILL IN THE TODO";
    }

    // TODO: 移动赋值运算符 — 先释放自身资源，再偷取 other 的句柄
    FileGuard& operator=(FileGuard&& other) noexcept {
        int _todo_ = "FILL IN THE TODO";
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
        // fg 在作用域结束时自动关闭文件
    }
    // 验证文件已写入
    FileGuard reader("cpplings_test.txt", "r");
    ASSERT_TRUE(reader.get() != nullptr);
    char buf[64] = {};
    std::fgets(buf, sizeof(buf), reader.get());
    ASSERT_EQ(std::string(buf), "test data");
    std::remove("cpplings_test.txt");
}

TEST("RAII 失败时返回空句柄") {
    FileGuard fg("/nonexistent/path/file.txt", "r");
    ASSERT_TRUE(!fg);                // operator bool 返回 false
    ASSERT_TRUE(fg.get() == nullptr);
}

TEST("RAII 禁止拷贝") {
    static_assert(!std::is_copy_constructible_v<FileGuard>,
                  "FileGuard 不可拷贝构造");
    static_assert(!std::is_copy_assignable_v<FileGuard>,
                  "FileGuard 不可拷贝赋值");
    ASSERT_TRUE(true);
}

TEST("RAII 允许移动") {
    static_assert(std::is_move_constructible_v<FileGuard>,
                  "FileGuard 应可移动构造");

    FileGuard fg("cpplings_move_test.txt", "w");
    ASSERT_TRUE(fg.get() != nullptr);

    FileGuard moved = std::move(fg);
    ASSERT_TRUE(fg.get() == nullptr);
    ASSERT_TRUE(moved.get() != nullptr);

    std::remove("cpplings_move_test.txt");
}

CPPLINGS_MAIN
