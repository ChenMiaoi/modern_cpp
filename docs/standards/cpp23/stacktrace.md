---
title: "std::stacktrace"
topic: unknown
feature: stacktrace
standard: N/A
status_checked_at: 2026-06-02
---
# std::stacktrace

C++23 引入 `<stacktrace>`，提供标准化的堆栈跟踪功能，用于调试、日志记录和错误报告。

## 基本用法

```cpp
#include <stacktrace>
#include <iostream>

void bar() {
    auto trace = std::stacktrace::current();
    std::cout << trace << "\n";
}

void foo() { bar(); }

int main() { foo(); }
```

输出类似：
```
0: bar() at main.cpp:4
1: foo() at main.cpp:8
2: main at main.cpp:10
```

## stacktrace_entry

```cpp
std::stacktrace trace = std::stacktrace::current();

for (const auto& entry : trace) {
    std::cout << "description: " << entry.description() << "\n";  // 函数名
    std::cout << "source_file: " << entry.source_file() << "\n";  // 文件路径
    std::cout << "source_line: " << entry.source_line() << "\n";  // 行号（0=未知）
}
```

## 在异常处理中使用

```cpp
#include <stacktrace>
#include <stdexcept>

struct traced_error : std::runtime_error {
    std::stacktrace trace;
    traced_error(const std::string& msg,
                 std::stacktrace st = std::stacktrace::current())
        : std::runtime_error(msg), trace(st) {}
};

void process() {
    throw traced_error("processing failed");
}

int main() {
    try {
        process();
    } catch (const traced_error& e) {
        std::cerr << "Error: " << e.what() << "\n";
        std::cerr << "Stack trace:\n" << e.trace << "\n";
    }
}
```

## 日志集成

```cpp
class Logger {
public:
    enum Level { Debug, Info, Warning, Error };

    static void log(Level level, const std::string& msg,
                    std::stacktrace st = std::stacktrace::current()) {
        if (level >= Warning) {
            std::cerr << format_level(level) << ": " << msg << "\n";
            size_t count = 0;
            for (const auto& entry : st) {
                if (count++ >= 5) break;
                std::cerr << "  " << entry << "\n";
            }
        } else {
            std::cout << format_level(level) << ": " << msg << "\n";
        }
    }
private:
    static const char* format_level(Level l) {
        switch (l) {
            case Debug: return "DEBUG"; case Info: return "INFO";
            case Warning: return "WARN"; case Error: return "ERROR";
        }
        return "?";
    }
};
```

## 空栈跟踪与跳过帧

```cpp
std::stacktrace empty;
assert(empty.empty());  // 默认构造的为空

// 跳过前 N 帧
auto trace = std::stacktrace::current(2);  // 跳过 current() 和调用者
```

## 格式化

```cpp
std::stacktrace trace = std::stacktrace::current();

std::cout << trace;                    // 流输出
std::string str = std::to_string(trace);  // 转字符串
std::print("Current: {}\n", trace[0]);    // std::format 支持
```

## 平台支持

| 平台 | 编译器 | 状态 | 备注 |
|------|--------|------|------|
| Linux | GCC 12+ | 支持 | `-lstdc++_libbacktrace` 或 `-rdynamic` |
| Linux | Clang 15+ | 部分 | 需要 libunwind |
| Windows | MSVC 17+ | 支持 | 需 PDB 文件获取符号 |
| macOS | Clang 15+ | 实验性 | `-lunwind` |

```bash
# GCC/Linux
g++ -std=c++23 -lstdc++_libbacktrace main.cpp

# MSVC
cl /std:c++latest /Zi main.cpp
```

## 性能考量

`std::stacktrace::current()` 不是零开销操作，典型开销为几微秒到几十微秒，取决于栈深度。不要在热路径中无条件获取栈跟踪：

```cpp
void hot_function() {
    #ifdef DEBUG
    auto trace = std::stacktrace::current();
    #endif
}
```

## 注意事项

- `source_file()` 和 `source_line()` 依赖调试信息（`-g` / `/Zi`）
- 优化可能内联掉某些栈帧
- `description()` 的格式是实现定义的
- 析构函数关键路径慎用（可能分配内存）
