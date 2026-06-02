---
title: "C++17 文件系统（`std::filesystem`）"
topic: unknown
feature: filesystem
standard: N/A
status_checked_at: 2026-06-02
---
# C++17 文件系统（`std::filesystem`）

## 概述

C++17 将文件系统库（源自 `boost::filesystem`）纳入标准，提供跨平台的文件和目录操作接口。`std::filesystem::path` 表示文件路径，配合 `directory_iterator`、文件操作函数等工具，覆盖遍历目录、创建/删除文件、查询文件状态等常见需求。定义在 `<filesystem>` 头文件中，通常别名为 `fs`。

## 语法

```cpp
#include <filesystem>
namespace fs = std::filesystem;

fs::path p = fs::current_path() / "data" / "config.json";

fs::exists(p);               // 是否存在
fs::is_regular_file(p);     // 是否普通文件
fs::is_directory(p);        // 是否目录
fs::file_size(p);            // 文件大小

fs::create_directories("a/b/c");     // 创建多级目录
fs::remove("file.txt");              // 删除文件
fs::remove_all("build");            // 递归删除
fs::copy(src, dst);                  // 拷贝
fs::rename(old, new_path);          // 重命名
```

## `std::filesystem::path`

```cpp
fs::path p = "/home/user/documents/report.pdf";

p.root_path();      // "/"
p.parent_path();    // "/home/user/documents"
p.filename();       // "report.pdf"
p.stem();           // "report"
p.extension();      // ".pdf"
p.is_absolute();    // true

// 路径拼接——使用 / 操作符
fs::path base = "/usr/local";
fs::path full = base / "bin" / "myapp"; // "/usr/local/bin/myapp"
// 避免字符串拼接：base.string() + "bin" 会得到 "/usr/localbin"
```

### 路径迭代与跨平台

```cpp
// 路径部分迭代
for (const auto& part : fs::path("/home/user/file.txt")) {
    std::cout << part << '\n'; // "/", "home", "user", "file.txt"
}

// 正斜杠在 Windows 也有效
fs::path p = "C:/Users/test/file.txt";
p.string();          // 平台格式
p.generic_string();  // "C:/Users/test/file.txt"
```

## 目录遍历

```cpp
// 单层遍历
for (const auto& entry : fs::directory_iterator("/tmp")) {
    if (entry.is_regular_file()) {
        std::cout << entry.path() << " size=" << entry.file_size() << '\n';
    }
}

// 递归遍历
for (const auto& entry : fs::recursive_directory_iterator("/project/src")) {
    if (entry.path().extension() == ".h")
        std::cout << entry.path() << '\n';
}

// 控制递归深度——跳过子目录
auto it = fs::recursive_directory_iterator("/project");
for (auto end = fs::recursive_directory_iterator(); it != end; ++it) {
    if (it->is_directory() && it->path().filename() == "build")
        it.disable_recursion_pending();
}
```

## 文件操作

### 查询与状态

```cpp
fs::path p = "config.json";
fs::exists(p);                  // 存在与否
fs::is_regular_file(p);         // 普通文件
fs::is_directory(p);            // 目录
fs::is_symlink(p);              // 符号链接
fs::is_empty(p);                // 空文件或空目录
fs::file_size(p);               // 字节大小

fs::file_status status = fs::status(p);
status.type();                   // file_type 枚举
status.permissions();            // perms 枚举
```

### 创建、删除、拷贝

```cpp
fs::create_directory("logs");                // 单级
fs::create_directories("a/b/c/d");           // 多级

fs::remove("file.txt");                      // 删除文件/空目录
fs::remove_all("build");                     // 递归删除

fs::copy("src.txt", "dst.txt");              // 拷贝文件
fs::copy("src_dir", "dst_dir", fs::copy_options::recursive); // 递归拷贝

fs::rename("old.txt", "new.txt");
```

### 临时目录与当前路径

```cpp
fs::path tmp = fs::temp_directory_path();
fs::path cwd = fs::current_path();
fs::current_path("/tmp");  // 修改进程工作目录
```

## 错误处理

```cpp
// 异常方式
try {
    fs::create_directory("/root/protected");
} catch (const fs::filesystem_error& e) {
    std::cerr << e.what() << " path1=" << e.path1() << '\n';
}

// error_code 方式（推荐用于库代码和性能敏感场景）
std::error_code ec;
fs::create_directory("/root/protected", ec);
if (ec) {
    std::cerr << ec.message() << '\n';
}
```

| 场景 | 推荐方式 |
|------|---------|
| 文件必须存在（缺失是 bug） | 异常 |
| 可能不存在（正常流程） | `error_code` |
| 批量操作 | `error_code` |

## 实际应用：递归查找特定文件

```cpp
std::vector<fs::path> find_files(fs::path root, std::string_view ext) {
    std::vector<fs::path> result;
    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        if (entry.is_regular_file() && entry.path().extension() == ext)
            result.push_back(entry.path());
    }
    return result;
}
```

## 最佳实践

1. **使用 `namespace fs = std::filesystem` 别名**。
2. **路径拼接用 `/` 操作符**，不要字符串拼接。
3. **库代码优先 `error_code` 重载**：避免异常，更可控。
4. **用 `path::stem()` 和 `path::extension()`**：不要用字符串操作拆分路径。
5. **路径分隔符始终用 `/`**：正斜杠在所有平台有效，不要硬编码 `\\`。

## 常见陷阱

- **`path::string()` 平台相关**：跨平台使用 `u8string()` 或 `generic_string()`。
- **`file_size` 要求文件存在**：先用 `exists()` 检查。
- **`remove` vs `remove_all`**：`remove` 只删文件或空目录，`remove_all` 递归删除全部内容。
- **`copy` 默认不递归**：必须指定 `copy_options::recursive`。
- **TOCTOU 竞态**：`exists()` 后文件可能已被删除，直接用 `error_code` 版本操作更安全。
- **迭代器缓存**：`directory_iterator` 不保证实时反映文件系统变化。
