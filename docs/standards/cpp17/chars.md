---
title: C++17 std::from_chars / std::to_chars
topic: unknown
feature: chars-conversion
standard: N/A
status_checked_at: 2026-06-20
---
# C++17 std::from_chars / std::to_chars

## 概述

`std::from_chars` 和 `std::to_chars` 是 C++17 在 `<charconv>` 中引入的函数，用于**高性能的数值与字符串转换**。它们不分配内存、不使用 locale、不开抛异常，因此在解析大量数值数据（如 JSON、CSV、日志）时比 `stoi`、`sscanf` 等传统方式快一个数量级。

## 函数签名

```cpp
#include <charconv>

// 字符串 → 数值
std::from_chars_result from_chars(
    const char* first, const char* last,
    T& value, int base = 10);

// 数值 → 字符串
std::to_chars_result to_chars(
    char* first, char* last,
    T value, int base = 10);
```

返回值结构：
```cpp
struct from_chars_result {
    const char* ptr;   // 解析结束位置
    std::errc ec;      // 错误码
};

struct to_chars_result {
    char* ptr;         // 写入结束位置
    std::errc ec;      // 错误码
};
```

## std::from_chars：字符串转数值

### 整数解析

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

    // 解析部分字符串
    std::string mixed = "42abc";
    int v;
    auto [p, e] = std::from_chars(mixed.data(), mixed.data() + mixed.size(), v);
    if (e == std::errc()) {
        std::cout << "partial: " << v << "\n";  // 42
        std::cout << "remaining: " << std::string(p, mixed.data() + mixed.size()) << "\n";
    }
}
```

### 不同进制

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

### 浮点数解析

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

    // 科学计数法
    const char* sci = "1.5e10";
    std::from_chars(sci, sci + 6, value);
    std::cout << "scientific: " << value << "\n";  // 1.5e+10
}
```

## std::to_chars：数值转字符串

### 整数格式化

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

    // 不同进制
    std::to_chars(buf.data(), buf.data() + buf.size(), 255, 16);
    std::cout << "hex: " << std::string(buf.data(), ptr) << "\n";  // ff

    std::to_chars(buf.data(), buf.data() + buf.size(), 10, 2);
    std::cout << "bin: " << std::string(buf.data(), ptr) << "\n";  // 1010
}
```

### 浮点数格式化

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

    // C++26: std::to_chars 支持 format_to 风格的精度控制
    // C++17: 使用 to_chars 的默认精度
}
```

## 性能对比

```cpp
#include <charconv>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <chrono>
#include <vector>

int main() {
    // 生成测试数据
    std::vector<std::string> numbers;
    for (int i = 0; i < 1000000; ++i) {
        numbers.push_back(std::to_string(i));
    }

    // std::from_chars（最快）
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

    // std::stoi（较慢，会分配内存、使用 locale）
    auto start2 = std::chrono::steady_clock::now();
    int sum2 = 0;
    for (const auto& s : numbers) {
        sum2 += std::stoi(s);
    }
    auto end2 = std::chrono::steady_clock::now();
    auto ms2 = std::chrono::duration_cast<std::chrono::milliseconds>(end2 - start2);
    std::cout << "stoi: " << ms2.count() << " ms\n";

    // atoi（最慢，无错误检查）
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

| 方法 | 特点 | 相对性能 |
|------|------|---------|
| `std::from_chars` | 无分配、无 locale、无异常 | 最快 |
| `std::stoi` | 分配 string、使用 locale、抛异常 | 中等 |
| `std::atoi` | 无错误检查、全局 locale | 最慢 |
| `std::sscanf` | 格式化解析、使用 locale | 慢 |

## 解析自定义格式

```cpp
#include <charconv>
#include <string>
#include <iostream>

struct Color {
    int r, g, b;
};

std::from_chars_result parse_color(const char* first, const char* last, Color& c) {
    // 解析 "#RRGGBB" 格式
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

## 错误处理

```cpp
#include <charconv>
#include <iostream>

int main() {
    // 溢出
    const char* too_big = "99999999999999999999";
    int v1;
    auto [p1, e1] = std::from_chars(too_big, too_big + 20, v1);
    std::cout << "overflow: " << std::make_error_code(e1).message() << "\n";

    // 无效输入
    const char* invalid = "abc";
    int v2;
    auto [p2, e2] = std::from_chars(invalid, invalid + 3, v2);
    std::cout << "invalid: " << std::make_error_code(e2).message() << "\n";

    // 缓冲区太小
    char small_buf[2];
    auto [p3, e3] = std::to_chars(small_buf, small_buf + 2, 12345);
    std::cout << "too_small: " << std::make_error_code(e3).message() << "\n";
}
```

## 编译器支持

| 编译器 | 整数支持 | 浮点数支持 | 备注 |
|--------|---------|-----------|------|
| GCC | 7.0 | 11.0 | 浮点数需要较新版本 |
| Clang | 5.0 | 16.0 | 浮点数需要较新版本 |
| MSVC | 19.14 (VS 2017 15.7) | 19.14 | 完整支持 |

**注意**：整数的 `from_chars`/`to_chars` 在所有主流编译器的 C++17 模式下都可用。浮点数支持在 GCC 11 和 Clang 16 之前可能不完整。

## 最佳实践

- **性能敏感场景**首选 `from_chars`/`to_chars`：解析日志、CSV、JSON 等大量数值时性能优势明显。
- **不需要 null 终止符**：直接使用指针范围 `[first, last)`。
- **错误处理使用 `errc`**：比异常更高效，适合禁用异常的项目。
- **不支持 locale**：这是特性不是缺陷——locale 在数值解析中通常不需要。
- **缓冲区大小**：`to_chars` 需要足够大的缓冲区，最大 `sizeof(T) * 8` 字符（二进制）。

## 常见陷阱

```cpp
// 陷阱 1：不检查错误码
const char* s = "abc";
int v;
// std::from_chars(s, s + 3, v);  // 必须检查 ec

// 陷阱 2：缓冲区不足
char buf[2];
// std::to_chars(buf, buf + 2, 10000);  // ec 会是 std::errc::value_too_large

// 陷阱 3：浮点数精度
double d;
const char* pi = "3.141592653589793238";
std::from_chars(pi, pi + 20, d);
// 精度受限于 double 的表示能力

// 陷阱 4：from_chars 不跳过前导空格
const char* padded = "  42";
int v;
auto [ptr, ec] = std::from_chars(padded, padded + 5, v);
// ec == std::errc::invalid_argument，因为第一个字符是空格
```
