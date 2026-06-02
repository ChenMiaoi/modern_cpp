---
title: "std::stacktrace"
topic: unknown
feature: stacktrace
standard: N/A
status_checked_at: 2026-06-02
---
# std::stacktrace

C++23 introduces `<stacktrace>`, providing standardized stack trace functionality for debugging, logging, and error reporting.

## Basic Usage

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

Output similar to:
```
0: bar() at main.cpp:4
1: foo() at main.cpp:8
2: main at main.cpp:10
```

## stacktrace_entry

```cpp
std::stacktrace trace = std::stacktrace::current();

for (const auto& entry : trace) {
    std::cout << "description: " << entry.description() << "\n";  // Function name
    std::cout << "source_file: " << entry.source_file() << "\n";  // File path
    std::cout << "source_line: " << entry.source_line() << "\n";  // Line number (0=unknown)
}
```

## Usage in Exception Handling

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

## Logging Integration

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

## Empty Stack Traces and Skipping Frames

```cpp
std::stacktrace empty;
assert(empty.empty());  // Default-constructed is empty

// Skip the first N frames
auto trace = std::stacktrace::current(2);  // Skips current() and the caller
```

## Formatting

```cpp
std::stacktrace trace = std::stacktrace::current();

std::cout << trace;                    // Stream output
std::string str = std::to_string(trace);  // Convert to string
std::print("Current: {}\n", trace[0]);    // std::format support
```

## Platform Support

| Platform | Compiler | Status | Notes |
|----------|----------|--------|-------|
| Linux | GCC 12+ | Supported | `-lstdc++_libbacktrace` or `-rdynamic` |
| Linux | Clang 15+ | Partial | Requires libunwind |
| Windows | MSVC 17+ | Supported | Requires PDB files for symbols |
| macOS | Clang 15+ | Experimental | `-lunwind` |

```bash
# GCC/Linux
g++ -std=c++23 -lstdc++_libbacktrace main.cpp

# MSVC
cl /std:c++latest /Zi main.cpp
```

## Performance Considerations

`std::stacktrace::current()` is not a zero-overhead operation; typical overhead ranges from a few microseconds to tens of microseconds depending on stack depth. Do not unconditionally capture stack traces in hot paths:

```cpp
void hot_function() {
    #ifdef DEBUG
    auto trace = std::stacktrace::current();
    #endif
}
```

## Caveats

- `source_file()` and `source_line()` depend on debug information (`-g` / `/Zi`)
- Optimization may inline away some stack frames
- The format of `description()` is implementation-defined
- Use with caution in destructor critical paths (may allocate memory)
