// Solution: filesystem1 — 文件系统基础
#include "cpplings.h"
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

TEST("filesystem — path 拼接") {
    fs::path base = "/usr";
    fs::path sub = "local";
    fs::path result = base / sub;
    ASSERT_EQ(result, fs::path("/usr/local"));
}

TEST("filesystem — path 组件") {
    fs::path p = "/home/user/document.txt";
    ASSERT_EQ(p.filename(), fs::path("document.txt"));
    ASSERT_EQ(p.extension(), fs::path(".txt"));
    ASSERT_EQ(p.stem(), fs::path("document"));
    ASSERT_EQ(p.parent_path(), fs::path("/home/user"));
}

TEST("filesystem — exists 和目录创建") {
    fs::path test_dir = fs::temp_directory_path() / "cpplings_fs_test";

    // 先清理（幂等）
    if (fs::exists(test_dir)) fs::remove(test_dir);

    bool created = fs::create_directory(test_dir);
    ASSERT_TRUE(created);
    ASSERT_TRUE(fs::exists(test_dir));
    ASSERT_TRUE(fs::is_directory(test_dir));

    // 再次创建应返回 false
    bool again = fs::create_directory(test_dir);
    ASSERT_FALSE(again);

    // 清理
    fs::remove(test_dir);
    ASSERT_FALSE(fs::exists(test_dir));
}

TEST("filesystem — directory_iterator") {
    fs::path test_dir = fs::temp_directory_path() / "cpplings_fs_iter_test";
    fs::create_directories(test_dir);

    std::ofstream(test_dir / "a.txt") << "hello";
    std::ofstream(test_dir / "b.txt") << "world";

    int count = 0;
    for (auto& entry : fs::directory_iterator(test_dir)) {
        if (entry.is_regular_file()) count++;
    }
    ASSERT_EQ(count, 2);

    // 清理
    fs::remove_all(test_dir);
}

CPPLINGS_MAIN
