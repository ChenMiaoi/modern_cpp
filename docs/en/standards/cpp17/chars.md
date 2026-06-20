---
title: C++17 std::from_chars / std::to_chars
topic: unknown
feature: chars-conversion
standard: N/A
status_checked_at: 2026-06-20
---
# C++17 std::from_chars / std::to_chars

## Overview

`std::from_chars` and `std::to_chars` are functions introduced in C++17 within `<charconv>` that provide **high-performance numeric-string conversions**. They allocate no memory, use no locale, and throw no exceptions, making them an order of magnitude faster than `stoi`, `sscanf`, and other traditional methods when parsing large volumes of numeric data (JSON, CSV, logs).

## Function Signatures

```cpp
#include <charconv>

// string → number
std::from_chars_result from_chars(
    const char* first, const char* last,
    T& value, int base = 10);

// number → string
std::to_chars_result to_chars(
    char* first, char* last,
    T value, int base = 10);
```

Return value structures:
```cpp
struct from_chars_result {
    const char* ptr;   // parse end position
    std::errc ec;      // error code
};

struct to_chars_result {
    char* ptr;         // write end position
    std::errc ec;      // error code
};
```

## std::from_chars: String to Number

### Integer Parsing

```cpp
#include <charconv>
#include <string>
#include <iostream>
#include <system_error>

int main() {
    std::string s = "12345";

    int value;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);

    if (ec == std::errc()) {
        std::cout << "parsed: " << value << "\n";  // 12345
    } else {
        std::cout << "parse error\n";
    }

    // Parse partial string
    std::string mixed = "42abc";
    int v;
    auto [p, e] = std::from_chars(mixed.data(), mixed.data() + mixed.size(), v);
    if (e == std::errc()) {
        std::cout << "partial: " << v << "\n";  // 42
        std::cout << "remaining: " << std::string(p, mixed.data() + mixed.size()) << "\n";
    }
}
```

### Different Bases

```cpp
#include <charconv>
#include <iostream>

int main() {
    const char* hex = "ff";
    int val;
    std::from_chars(hex, hex + 2, val, 16);
    std::cout << "hex: " << val << "\n";  // 255

    const char* bin = "1010";
    std::from_chars(bin, bin + 4, val, 2);
    std::cout << "bin: " << val << "\n";  // 10

    const char* oct = "77";
    std::from_chars(oct, oct + 2, val, 8);
    std::cout << "oct: " << val << "\n";  // 63
}
```

### Floating-Point Parsing

```cpp
#include <charconv>
#include <iostream>

int main() {
    const char* s = "3.14159";
    double value;
    auto [ptr, ec] = std::from_chars(s, s + 7, value);

    if (ec == std::errc()) {
        std::cout << "pi: " << value << "\n";  // 3.14159
    }

    // Scientific notation
    const char* sci = "1.5e10";
    std::from_chars(sci, sci + 6, value);
    std::cout << "scientific: " << value << "\n";  // 1.5e+10
}
```

## std::to_chars: Number to String

### Integer Formatting

```cpp
#include <charconv>
#include <array>
#include <iostream>

int main() {
    int value = 42;

    std::array<char, 20> buf{};
    auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);

    if (ec == std::errc()) {
        std::cout << "decimal: " << std::string(buf.data(), ptr) << "\n";
    }

    // Different bases
    std::to_chars(buf.data(), buf.data() + buf.size(), 255, 16);
    std::cout << "hex: " << std::string(buf.data(), ptr) << "\n";  // ff

    std::to_chars(buf.data(), buf.data() + buf.size(), 10, 2);
    std::cout << "bin: " << std::string(buf.data(), ptr) << "\n";  // 1010
}
```

### Floating-Point Formatting

```cpp
#include <charconv>
#include <array>
#include <iostream>

int main() {
    double pi = 3.141592653589793;

    std::array<char, 50> buf{};
    auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), pi);

    if (ec == std::errc()) {
        std::cout << "default: " << std::string(buf.data(), ptr) << "\n";
    }

    // C++26: std::to_chars supports format_to-style precision control
    // C++17: uses default precision
}
```

## Performance Comparison

```cpp
#include <charconv>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <chrono>
#include <vector>

int main() {
    // Generate test data
    std::vector<std::string> numbers;
    for (int i = 0; i < 1000000; ++i) {
        numbers.push_back(std::to_string(i));
    }

    // std::from_chars (fastest)
    auto start1 = std::chrono::steady_clock::now();
    int sum1 = 0;
    for (const auto& s : numbers) {
        int v;
        std::from_chars(s.data(), s.data() + s.size(), v);
        sum1 += v;
    }
    auto end1 = std::chrono::steady_clock::now();
    auto ms1 = std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1);
    std::cout << "from_chars: " << ms1.count() << " ms\n";

    // std::stoi (slower, allocates memory, uses locale)
    auto start2 = std::chrono::steady_clock::now();
    int sum2 = 0;
    for (const auto& s : numbers) {
        sum2 += std::stoi(s);
    }
    auto end2 = std::chrono::steady_clock::now();
    auto ms2 = std::chrono::duration_cast<std::chrono::milliseconds>(end2 - start2);
    std::cout << "stoi: " << ms2.count() << " ms\n";

    // atoi (slowest, no error checking)
    auto start3 = std::chrono::steady_clock::now();
    int sum3 = 0;
    for (const auto& s : numbers) {
        sum3 += std::atoi(s.c_str());
    }
    auto end3 = std::chrono::steady_clock::now();
    auto ms3 = std::chrono::duration_cast<std::chrono::milliseconds>(end3 - start3);
    std::cout << "atoi: " << ms3.count() << " ms\n";
}
```

| Method | Characteristics | Relative Performance |
|--------|----------------|---------------------|
| `std::from_chars` | No allocation, no locale, no exceptions | Fastest |
| `std::stoi` | Allocates string, uses locale, throws exceptions | Medium |
| `std::atoi` | No error checking, global locale | Slowest |
| `std::sscanf` | Format parsing, uses locale | Slow |

## Parsing Custom Formats

```cpp
#include <charconv>
#include <string>
#include <iostream>

struct Color {
    int r, g, b;
};

std::from_chars_result parse_color(const char* first, const char* last, Color& c) {
    // Parse "#RRGGBB" format
    if (first == last || *first != '#') {
        return {first, std::errc::invalid_argument};
    }
    ++first;

    uint32_t hex = 0;
    auto [ptr, ec] = std::from_chars(first, last, hex, 16);
    if (ec != std::errc()) return {ptr, ec};

    c.r = (hex >> 16) & 0xFF;
    c.g = (hex >> 8) & 0xFF;
    c.b = hex & 0xFF;
    return {ptr, std::errc()};
}

int main() {
    Color c;
    const char* s = "#FF8800";
    auto [ptr, ec] = parse_color(s, s + 7, c);
    if (ec == std::errc()) {
        std::cout << "R:" << c.r << " G:" << c.g << " B:" << c.b << "\n";
        // R:255 G:136 B:0
    }
}
```

## Error Handling

```cpp
#include <charconv>
#include <iostream>

int main() {
    // Overflow
    const char* too_big = "99999999999999999999";
    int v1;
    auto [p1, e1] = std::from_chars(too_big, too_big + 20, v1);
    std::cout << "overflow: " << std::make_error_code(e1).message() << "\n";

    // Invalid input
    const char* invalid = "abc";
    int v2;
    auto [p2, e2] = std::from_chars(invalid, invalid + 3, v2);
    std::cout << "invalid: " << std::make_error_code(e2).message() << "\n";

    // Buffer too small
    char small_buf[2];
    auto [p3, e3] = std::to_chars(small_buf, small_buf + 2, 12345);
    std::cout << "too_small: " << std::make_error_code(e3).message() << "\n";
}
```

## Compiler Support

| Compiler | Integer Support | Floating-Point Support | Notes |
|----------|----------------|----------------------|-------|
| GCC | 7.0 | 11.0 | FP needs newer version |
| Clang | 5.0 | 16.0 | FP needs newer version |
| MSVC | 19.14 (VS 2017 15.7) | 19.14 | Full support |

**Note**: Integer `from_chars`/`to_chars` is available in C++17 mode on all major compilers. Floating-point support may be incomplete before GCC 11 and Clang 16.

## Best Practices

- **Performance-sensitive scenarios**: Use `from_chars`/`to_chars` for parsing logs, CSV, JSON, and other bulk numeric data.
- **No null terminator needed**: Use pointer range `[first, last)` directly.
- **Error handling with `errc`**: More efficient than exceptions; suitable for exception-disabled projects.
- **No locale support**: This is a feature, not a deficiency — locale is usually unnecessary for numeric parsing.
- **Buffer size**: `to_chars` needs sufficient buffer space, maximum `sizeof(T) * 8` characters (binary).

## Common Pitfalls

```cpp
// Pitfall 1: not checking error code
const char* s = "abc";
int v;
// std::from_chars(s, s + 3, v);  // Must check ec

// Pitfall 2: insufficient buffer
char buf[2];
// std::to_chars(buf, buf + 2, 10000);  // ec will be std::errc::value_too_large

// Pitfall 3: floating-point precision
double d;
const char* pi = "3.141592653589793238";
std::from_chars(pi, pi + 20, d);
// Precision limited by double's representation

// Pitfall 4: from_chars doesn't skip leading whitespace
const char* padded = "  42";
int v;
auto [ptr, ec] = std::from_chars(padded, padded + 5, v);
// ec == std::errc::invalid_argument because first character is space
```
