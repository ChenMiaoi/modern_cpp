---
title: std::print / std::println
topic: cpp23
feature: print
standard: C++23
status_checked_at: 2026-06-02
standard_refs:
  - draft: N4950
    clause: "[print]"
proposals:
  - paper: P2093
    revision: R14
    status: accepted
exercises:
  - exercises/cpp23/print23.cpp
solutions:
  - exercises/solutions/print23.cpp
---
# std::print / std::println

C++23 introduces `std::print` and `std::println`, providing Python-style formatted output based on `std::format`, bypassing the complexity and performance overhead of iostream.

## Basic Usage

```cpp
#include <print>

int main() {
    std::print("Hello, {}!\n", "world");
    std::println("Hello, {}!", "world");  // Automatically appends newline

    int x = 42;
    std::println("x = {}", x);

    // Multiple arguments
    std::println("{} + {} = {}", 3, 4, 3 + 4);
}
```

## Format Syntax

`std::print` uses the `std::format` format string syntax:

```cpp
// Positional arguments
std::print("{0} is {1}", "Pi", 3.14);

// Alignment and fill
std::print("{:>10}", "right");    // "     right"
std::print("{:<10}", "left");     // "left      "
std::print("{:^10}", "center");   // "  center  "
std::print("{:*^10}", "hi");      // "****hi****"

// Numeric formatting
std::print("{:.2f}", 3.14159);    // "3.14"
std::print("{:#x}", 255);         // "0xff"
std::print("{:08b}", 42);         // "00101010"
std::print("{:+d}", 42);          // "+42"

// Type-specific formatting
std::print("{:s}", true);         // "true" (not "1")
```

## Output Targets

```cpp
#include <print>
#include <fstream>

int main() {
    // Output to stdout
    std::print("to stdout\n");

    // Output to stderr
    std::print(stderr, "to stderr\n");

    // Output to file
    std::ofstream file("output.txt");
    std::print(file, "to file: {}\n", 42);

    // Output to any output stream
    // Any output iterator supporting std::format_to works
}
```

## Comparison with std::cout

```cpp
// iostream approach — verbose
std::cout << "Name: " << name << ", Age: " << age << ", Score: "
          << std::fixed << std::setprecision(2) << score << "\n";

// print approach — concise
std::print("Name: {}, Age: {}, Score: {:.2f}", name, age, score);
```

| Property | `std::cout` | `std::print` |
|----------|-------------|--------------|
| Formatting | Manipulator chain | Python-style `{}` |
| Type safety | Runtime | Compile-time check |
| Performance | Slower (synchronization, facets) | Faster (direct formatting) |
| Localization | Via locale facet | Via `std::locale` parameter |

## Comparison with printf

```cpp
// printf — type-unsafe
printf("Name: %s, Age: %d, Score: %.2f", name, age, score);

// std::print — type-safe
std::print("Name: {}, Age: {}, Score: {:.2f}", name, age, score);
```

| Property | `printf` | `std::print` |
|----------|----------|--------------|
| Type safety | No (UB on type mismatch) | Yes (compile-time check) |
| User-defined types | No | Yes (via `std::formatter` specialization) |
| Unicode | Platform-dependent | Standardized support |
| Buffering | stdout buffer | Target stream's buffer |

## Performance Advantages

The performance advantages of `std::print` come from:

1. **No iostream overhead**: bypasses the `std::ostream` virtual function call chain
2. **Direct writes**: formatted results are written directly to the target buffer
3. **Compile-time format parsing**: format strings are parsed at compile time, with zero runtime overhead
4. **No synchronization overhead**: C++23 print does not enforce synchronization with C stdout

```cpp
// Significantly faster in performance-sensitive scenarios
for (int i = 0; i < 1000000; ++i) {
    std::print("{:8d}\n", i);  // Faster than cout << setw(8) << i
}
```

## Custom Type Support

```cpp
struct Point {
    double x, y;
};

template <>
struct std::formatter<Point> : std::formatter<std::string> {
    auto format(const Point& p, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "({}, {})", p.x, p.y);
    }
};

Point p{3.0, 4.0};
std::print("point = {}\n", p);  // "point = (3, 4)"
```

## Convenience of println

```cpp
std::println();                    // Just outputs a newline
std::println("simple message");    // With newline, no manual \n needed
std::println("{} items", count);   // Formatting + newline
```

`println` is equivalent to `print` plus a `"\n"`, avoiding the common problem of forgetting the newline.

## Caveats

- Requires compiler support: GCC 13+, MSVC 17.5+, Clang 17+
- `std::print` does not flush the buffer; manual `fflush` or `std::flush` is needed for immediate output
- The format string must be a compile-time constant (constexpr); invalid formats are caught at compile time
- Custom types must specialize `std::formatter`
