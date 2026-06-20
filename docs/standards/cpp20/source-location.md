---
title: C++20 std::source_location
topic: unknown
feature: source-location
standard: N/A
status_checked_at: 2026-06-20
---
# C++20 `std::source_location`

## 概述

C++20 在 `<source_location>` 中引入 `std::source_location`，提供编译期可获取的调用点元数据（文件名、行号、列号、函数名），取代丑陋的 `__FILE__` / `__LINE__` / `__func__` 宏。

核心优势：
- **类型安全**：作为参数传递，而非宏展开。
- **自动推导**：默认参数自动捕获调用点，而非定义点。
- **可组合**：函数可将 source_location 透传给子函数。

```cpp
#include <source_location>
#include <iostream>

void log(const char* msg, std::source_location loc = std::source_location::current()) {
    std::cout << loc.file_name() << ":" << loc.line() << " " << msg << "\n";
}

int main() {
    log("hello");  // 输出：main.cpp:8 hello
}
```

## 核心 API

`std::source_location` 提供以下静态成员函数：

| 方法 | 返回类型 | 说明 |
|------|----------|------|
| `current()` | `source_location` | 返回调用点的 source_location |
| `file_name()` | `const char*` | 源文件名 |
| `line()` | `std::uint_least32_t` | 行号 |
| `column()` | `std::uint_least32_t` | 列号（编译器支持有限） |
| `function_name()` | `const char*` | 函数名 |

### `current()` 作为默认参数

```cpp
#include <source_location>
#include <iostream>

void debug_log(const char* msg,
               std::source_location loc = std::source_location::current()) {
    std::cout << "[" << loc.function_name() << "] "
              << loc.file_name() << ":" << loc.line()
              << " - " << msg << "\n";
}

void do_work() {
    debug_log("starting work");  // 自动捕获 do_work 的调用信息
}

int main() {
    do_work();
    // 输出：[do_work] main.cpp:13 - starting work
}
```

## 替代 `__FILE__` / `__LINE__`

### 旧方式（宏）

```cpp
#define LOG(msg) \
    std::cout << __FILE__ << ":" << __LINE__ << " " << msg << "\n"

// 问题：
// 1. LOG 展开在调用点，但 __FILE__ / __LINE__ 是宏展开位置
// 2. 无法透传给子函数
// 3. 不是类型安全的
```

### 新方式（source_location）

```cpp
#include <source_location>
#include <iostream>
#include <string>

void log_impl(const std::string& msg,
              std::source_location loc = std::source_location::current()) {
    std::cout << loc.file_name() << ":" << loc.line() << " " << msg << "\n";
}

#define LOG(msg) log_impl(msg)

int main() {
    LOG("hello");  // 准确报告调用点，而非宏定义位置
}
```

## 在日志系统中使用

```cpp
#include <source_location>
#include <string>
#include <iostream>
#include <format>

class Logger {
public:
    static void info(const std::string& msg,
                     std::source_location loc = std::source_location::current()) {
        std::cout << std::format("[INFO] {}:{} {} - {}\n",
            loc.file_name(), loc.line(), loc.function_name(), msg);
    }

    static void error(const std::string& msg,
                      std::source_location loc = std::source_location::current()) {
        std::cerr << std::format("[ERROR] {}:{} {} - {}\n",
            loc.file_name(), loc.line(), loc.function_name(), msg);
    }
};

void process_data() {
    Logger::info("processing started");
    // 模拟错误
    Logger::error("data corruption detected");
}

int main() {
    process_data();
}
```

## 在断言中使用

```cpp
#include <source_location>
#include <iostream>
#include <string>
#include <stdexcept>

template <typename T>
void assert_impl(bool condition, const std::string& expr,
                 std::source_location loc = std::source_location::current()) {
    if (!condition) {
        throw std::runtime_error(
            std::string("Assertion failed: ") + expr +
            " at " + loc.file_name() + ":" + std::to_string(loc.line()) +
            " in " + loc.function_name());
    }
}

#define ASSERT(cond) assert_impl(cond, #cond)

int main() {
    int x = 5;
    ASSERT(x > 10);  // 抛出异常，包含文件名、行号、函数名
}
```

## 透传 source_location

函数可将 source_location 透传给子函数，保留原始调用点信息：

```cpp
#include <source_location>
#include <iostream>
#include <string>

void low_level_log(const std::string& msg,
                   std::source_location loc = std::source_location::current()) {
    std::cout << loc.file_name() << ":" << loc.line() << " " << msg << "\n";
}

void mid_level_log(const std::string& msg,
                   std::source_location loc = std::source_location::current()) {
    // 将原始调用点透传，而非 mid_level_log 的位置
    low_level_log(msg, loc);
}

void high_level_log(const std::string& msg,
                    std::source_location loc = std::source_location::current()) {
    mid_level_log(msg, loc);
}

int main() {
    high_level_log("deep call");  // 输出 main.cpp:23，而非 low_level_log 的位置
}
```

## 与 `__builtin_return_address` 等的区别

| 特性 | `__FILE__`/`__LINE__` | `__PRETTY_FUNCTION__` | `source_location` |
|------|----------------------|----------------------|-------------------|
| 类型 | 宏 | 宏 | 值类型 |
| 位置 | 定义点 | 定义点 | 调用点 |
| 可透传 | 否 | 否 | 是 |
| 类型安全 | 否 | 否 | 是 |
| 列号 | 否 | 否 | 部分支持 |

## 编译器支持

| 编译器 | 版本 | 支持状态 |
|--------|------|----------|
| GCC | 11+ | 完整支持（`-std=c++20`） |
| Clang | 16+ | 完整支持（`-std=c++20`） |
| MSVC | 19.29+ (VS 2019 16.10+) | 完整支持（`/std:c++20`） |

```cpp
// 编译验证
// g++ -std=c++20 source_loc.cpp -o source_loc
// clang++ -std=c++20 source_loc.cpp -o source_loc
// cl.exe /std:c++20 source_loc.cpp
```

## 常见陷阱

```cpp
// 1. default 参数只捕获调用点，不捕获定义点
void bad_log(std::source_location loc = std::source_location::current());
// loc 始终指向调用者的位置，而非 bad_log 的定义位置

// 2. 透传时需要显式传递
void outer(std::source_location loc = std::source_location::current()) {
    inner(loc);  // 正确：透传原始位置
    inner();     // 错误：inner 捕获 outer 的调用点
}

// 3. column() 在许多编译器上返回 0（未实现）
std::source_location loc = std::source_location::current();
std::cout << loc.column();  // 可能输出 0
```

## 总结

- `std::source_location` 是类型安全的编译期调用点元数据。
- 通过默认参数 `std::source_location::current()` 自动捕获调用点。
- 替代 `__FILE__` / `__LINE__` 宏，可透传、类型安全。
- 适用于日志系统、断言、调试工具。
- GCC 11+、Clang 16+、MSVC 19.29+ 均已支持。
