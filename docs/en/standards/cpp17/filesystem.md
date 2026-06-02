---
title: "C++17 Filesystem (`std::filesystem`)"
topic: unknown
feature: filesystem
standard: N/A
status_checked_at: 2026-06-02
---
# C++17 Filesystem (`std::filesystem`)

## Overview

C++17 incorporated the filesystem library (originating from `boost::filesystem`) into the standard, providing a cross-platform interface for file and directory operations. `std::filesystem::path` represents file paths, and combined with `directory_iterator`, file operation functions, and other tools, it covers common needs such as traversing directories, creating/deleting files, and querying file status. Defined in the `<filesystem>` header, it is typically aliased as `fs`.

## Syntax

```cpp
#include <filesystem>
namespace fs = std::filesystem;

fs::path p = fs::current_path() / "data" / "config.json";

fs::exists(p);               // exists?
fs::is_regular_file(p);     // is regular file?
fs::is_directory(p);        // is directory?
fs::file_size(p);            // file size

fs::create_directories("a/b/c");     // create nested directories
fs::remove("file.txt");              // delete file
fs::remove_all("build");            // recursive delete
fs::copy(src, dst);                  // copy
fs::rename(old, new_path);          // rename
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

// path concatenation—use the / operator
fs::path base = "/usr/local";
fs::path full = base / "bin" / "myapp"; // "/usr/local/bin/myapp"
// avoid string concatenation: base.string() + "bin" yields "/usr/localbin"
```

### Path Iteration and Cross-Platform

```cpp
// iterate over path components
for (const auto& part : fs::path("/home/user/file.txt")) {
    std::cout << part << '\n'; // "/", "home", "user", "file.txt"
}

// forward slash works on Windows too
fs::path p = "C:/Users/test/file.txt";
p.string();          // platform-native format
p.generic_string();  // "C:/Users/test/file.txt"
```

## Directory Traversal

```cpp
// single-level traversal
for (const auto& entry : fs::directory_iterator("/tmp")) {
    if (entry.is_regular_file()) {
        std::cout << entry.path() << " size=" << entry.file_size() << '\n';
    }
}

// recursive traversal
for (const auto& entry : fs::recursive_directory_iterator("/project/src")) {
    if (entry.path().extension() == ".h")
        std::cout << entry.path() << '\n';
}

// control recursion depth—skip subdirectories
auto it = fs::recursive_directory_iterator("/project");
for (auto end = fs::recursive_directory_iterator(); it != end; ++it) {
    if (it->is_directory() && it->path().filename() == "build")
        it.disable_recursion_pending();
}
```

## File Operations

### Querying and Status

```cpp
fs::path p = "config.json";
fs::exists(p);                  // exists or not
fs::is_regular_file(p);         // regular file
fs::is_directory(p);            // directory
fs::is_symlink(p);              // symbolic link
fs::is_empty(p);                // empty file or empty directory
fs::file_size(p);               // size in bytes

fs::file_status status = fs::status(p);
status.type();                   // file_type enum
status.permissions();            // perms enum
```

### Creation, Deletion, and Copying

```cpp
fs::create_directory("logs");                // single level
fs::create_directories("a/b/c/d");           // nested levels

fs::remove("file.txt");                      // delete file/empty directory
fs::remove_all("build");                     // recursive delete

fs::copy("src.txt", "dst.txt");              // copy file
fs::copy("src_dir", "dst_dir", fs::copy_options::recursive); // recursive copy

fs::rename("old.txt", "new.txt");
```

### Temporary Directory and Current Path

```cpp
fs::path tmp = fs::temp_directory_path();
fs::path cwd = fs::current_path();
fs::current_path("/tmp");  // change process working directory
```

## Error Handling

```cpp
// exception approach
try {
    fs::create_directory("/root/protected");
} catch (const fs::filesystem_error& e) {
    std::cerr << e.what() << " path1=" << e.path1() << '\n';
}

// error_code approach (recommended for library code and performance-sensitive scenarios)
std::error_code ec;
fs::create_directory("/root/protected", ec);
if (ec) {
    std::cerr << ec.message() << '\n';
}
```

| Scenario | Recommended Approach |
|----------|---------------------|
| File must exist (missing is a bug) | Exception |
| May not exist (normal flow) | `error_code` |
| Batch operations | `error_code` |

## Practical Application: Recursively Finding Specific Files

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

## Best Practices

1. **Use the `namespace fs = std::filesystem` alias**.
2. **Use the `/` operator for path concatenation** — do not use string concatenation.
3. **Prefer the `error_code` overload in library code**: avoids exceptions, more controllable.
4. **Use `path::stem()` and `path::extension()`**: do not use string operations to split paths.
5. **Always use `/` as path separator**: forward slashes work on all platforms; do not hardcode `\\`.

## Common Pitfalls

- **`path::string()` is platform-dependent**: use `u8string()` or `generic_string()` for cross-platform code.
- **`file_size` requires the file to exist**: check with `exists()` first.
- **`remove` vs `remove_all`**: `remove` only deletes files or empty directories; `remove_all` recursively deletes all contents.
- **`copy` is not recursive by default**: must specify `copy_options::recursive`.
- **TOCTOU race condition**: the file may have been deleted after `exists()`; using the `error_code` version directly is safer.
- **Iterator caching**: `directory_iterator` does not guarantee real-time reflection of filesystem changes.
