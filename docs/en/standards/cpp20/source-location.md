---
title: "C++20 std::source_location"
topic: unknown
feature: source-location
standard: N/A
status_checked_at: 2026-06-20
---
# C++20 `std::source_location`

## Overview

C++20 introduces `std::source_location` in `<source_location>`, providing compile-time call-site metadata (file name, line number, column number, function name) that replaces the awkward `__FILE__` / `__LINE__` / `__func__` macros.

Key advantages:
- **Type-safe**: Passed as a parameter, not a macro expansion.
- **Automatic deduction**: Default parameter automatically captures the call site, not the definition site.
- **Composable**: Functions can forward source_location to child functions.

```cpp
#include <source_location>
#include <iostream>

void log(const char* msg, std::source_location loc = std::source_location::current()) {
    std::cout << loc.file_name() << ":" << loc.line() << " " << msg << "\n";
}

int main() {
    log("hello");  // Output: main.cpp:8 hello
}
```

## Core API

`std::source_location` provides the following static member functions:

| Method | Return Type | Description |
|--------|-------------|-------------|
| `current()` | `source_location` | Returns the source_location at the call site |
| `file_name()` | `const char*` | Source file name |
| `line()` | `std::uint_least32_t` | Line number |
| `column()` | `std::uint_least32_t` | Column number (limited compiler support) |
| `function_name()` | `const char*` | Function name |

### `current()` as a Default Parameter

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
    debug_log("starting work");  // Automatically captures do_work's call info
}

int main() {
    do_work();
    // Output: [do_work] main.cpp:13 - starting work
}
```

## Replacing `__FILE__` / `__LINE__`

### Old Approach (Macros)

```cpp
#define LOG(msg) \
    std::cout << __FILE__ << ":" << __LINE__ << " " << msg << "\n"

// Problems:
// 1. LOG expands at the call site, but __FILE__ / __LINE__ are at macro definition
// 2. Cannot forward to child functions
// 3. Not type-safe
```

### New Approach (source_location)

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
    LOG("hello");  // Accurately reports call site, not macro definition location
}
```

## Using in Logging Systems

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
    Logger::error("data corruption detected");
}

int main() {
    process_data();
}
```

## Using in Assertions

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
    ASSERT(x > 10);  // Throws exception with file name, line number, function name
}
```

## Forwarding source_location

Functions can forward source_location to child functions, preserving the original call site:

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
    // Forward the original call site, not mid_level_log's location
    low_level_log(msg, loc);
}

void high_level_log(const std::string& msg,
                    std::source_location loc = std::source_location::current()) {
    mid_level_log(msg, loc);
}

int main() {
    high_level_log("deep call");  // Output: main.cpp:23, not low_level_log's location
}
```

## Comparison with `__builtin_return_address` etc.

| Feature | `__FILE__`/`__LINE__` | `__PRETTY_FUNCTION__` | `source_location` |
|---------|----------------------|----------------------|-------------------|
| Type | Macro | Macro | Value type |
| Location | Definition site | Definition site | Call site |
| Forwardable | No | No | Yes |
| Type-safe | No | No | Yes |
| Column number | No | No | Partially supported |

## Compiler Support

| Compiler | Version | Support Status |
|----------|---------|----------------|
| GCC | 11+ | Full support (`-std=c++20`) |
| Clang | 16+ | Full support (`-std=c++20`) |
| MSVC | 19.29+ (VS 2019 16.10+) | Full support (`/std:c++20`) |

```cpp
// Compile verification
// g++ -std=c++20 source_loc.cpp -o source_loc
// clang++ -std=c++20 source_loc.cpp -o source_loc
// cl.exe /std:c++20 source_loc.cpp
```

## Common Pitfalls

```cpp
// 1. Default parameter only captures call site, not definition site
void bad_log(std::source_location loc = std::source_location::current());
// loc always points to the caller's position, not bad_log's definition

// 2. Must explicitly forward when passing
void outer(std::source_location loc = std::source_location::current()) {
    inner(loc);  // Correct: forwards original location
    inner();     // Wrong: inner captures outer's call site
}

// 3. column() returns 0 on many compilers (not implemented)
std::source_location loc = std::source_location::current();
std::cout << loc.column();  // May output 0
```

## Summary

- `std::source_location` is a type-safe compile-time call-site metadata tool.
- Automatically captures call sites via default parameter `std::source_location::current()`.
- Replaces `__FILE__` / `__LINE__` macros — composable and type-safe.
- Ideal for logging systems, assertions, and debugging tools.
- Supported by GCC 11+, Clang 16+, and MSVC 19.29+.
