// Exercise: filesystem1 — 文件系统基础
// 使用 C++17 std::filesystem 进行路径操作和文件系统查询。
//
// 任务:
//   1. 使用 path 拼接和分解路径
//   2. 使用 exists() 检查文件和目录
//   3. 使用 create_directory 创建临时目录
//   4. 使用 directory_iterator 遍历目录
// 提示: 编译时需要链接 -lstdc++fs（某些编译器），并 #include <filesystem>

#include "cpplings.h"
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

TEST("filesystem — path 拼接") {
    fs::path base = "/usr";
    fs::path sub = "local";

    // TODO: 使用 / 运算符拼接路径
    int _todo_ = "请删除此行，实现上面的 TODO";
    // fs::path result = base / sub;
    // ASSERT_EQ(result, fs::path("/usr/local"));
}

TEST("filesystem — path 组件") {
    fs::path p = "/home/user/document.txt";

    // TODO: 提取文件名、扩展名、父目录
    // 提示: p.filename(), p.extension(), p.parent_path(), p.stem()
    int _todo_ = "请删除此行，实现上面的 TODO";
    // ASSERT_EQ(p.filename(), fs::path("document.txt"));
    // ASSERT_EQ(p.extension(), fs::path(".txt"));
    // ASSERT_EQ(p.stem(), fs::path("document"));
    // ASSERT_EQ(p.parent_path(), fs::path("/home/user"));
}

TEST("filesystem — exists 和目录创建") {
    // 使用当前目录下的临时测试目录
    fs::path test_dir = fs::temp_directory_path() / "cpplings_fs_test";

    // TODO: 创建目录，验证它存在
    // 提示: fs::create_directory(test_dir) 返回 true 表示新建成功
    int _todo_ = "请删除此行，实现上面的 TODO";
    // fs::create_directory(test_dir);  // 如果已存在，返回 false 但不抛异常
    // ASSERT_TRUE(fs::exists(test_dir));
    // ASSERT_TRUE(fs::is_directory(test_dir));

    // 清理
    // fs::remove(test_dir);
}

TEST("filesystem — directory_iterator") {
    fs::path test_dir = fs::temp_directory_path() / "cpplings_fs_iter_test";
    fs::create_directories(test_dir);

    // 创建几个测试文件
    std::ofstream(test_dir / "a.txt") << "hello";
    std::ofstream(test_dir / "b.txt") << "world";

    // TODO: 使用 directory_iterator 计算文件数量
    // 提示: for (auto& entry : fs::directory_iterator(test_dir)) { count++; }
    int _todo_ = "请删除此行，实现上面的 TODO";
    // int count = 0;
    // for (auto& entry : fs::directory_iterator(test_dir)) {
    //     if (entry.is_regular_file()) count++;
    // }
    // ASSERT_EQ(count, 2);

    // 清理
    fs::remove_all(test_dir);
}

CPPLINGS_MAIN
