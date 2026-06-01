# `std::format`

## 概述

C++20 引入 `<format>` 库，提供 Python 风格的类型安全格式化。编译期检查格式字符串，运行期高效拼接，是 `sprintf` 和 `iostream` 的现代替代。

## 基本用法

```cpp
#include <format>
#include <iostream>

int main() {
    auto s = std::format("Hello, {}! Age: {}", "Alice", 30);
    // "Hello, Alice! Age: 30"

    auto s2 = std::format("{1} and {0}", "world", "hello");
    // "hello and world"
}
```

## 格式说明符

语法：`{[位置][:格式说明]}`

### 整数与浮点

```cpp
std::format("{:d}", 42);          // "42"
std::format("{:x}", 255);         // "ff"
std::format("{:#x}", 255);        // "0xff"
std::format("{:08d}", 42);        // "00000042"
std::format("{:+d}", 42);         // "+42"

std::format("{:.2f}", 3.14159);   // "3.14"
std::format("{:.2e}", 1234.5);    // "1.23e+03"
```

### 对齐与填充

```cpp
std::format("{:<10}", "left");    // "left      "
std::format("{:>10}", "right");   // "     right"
std::format("{:^10}", "center");  // "  center  "
std::format("{:*^10}", "hi");     // "****hi****"
std::format("{:{}}", "pad", 10);  // "       pad"（动态宽度）
```

## 自定义类型格式化

特化 `std::formatter`：

```cpp
#include <format>

struct Color { uint8_t r, g, b; };

template <>
struct std::formatter<Color> : std::formatter<std::string> {
    auto format(const Color& c, auto& ctx) const {
        return std::formatter<std::string>::format(
            std::format("#{:02x}{:02x}{:02x}", c.r, c.g, c.b), ctx);
    }
};
// Color{255, 0, 0} → "#ff0000"
```

### 支持格式说明

```cpp
struct Point { double x, y; };

template <>
struct std::formatter<Point> {
    constexpr auto parse(auto& ctx) {
        auto it = ctx.begin();
        precision = -1;
        if (it != ctx.end() && *it == '.') {
            ++it; precision = 0;
            while (it != ctx.end() && *it >= '0' && *it <= '9')
                precision = precision * 10 + (*it++ - '0');
        }
        ctx.advance_to(it);
        return it;
    }

    auto format(const Point& p, auto& ctx) const {
        if (precision >= 0)
            return std::format_to(ctx.out(), "({:.{}f}, {:.{}f})",
                                   p.x, precision, p.y, precision);
        return std::format_to(ctx.out(), "({}, {})", p.x, p.y);
    }
    int precision = -1;
};

// std::format("{:.2f}", Point{3.14159, 2.71828}) → "(3.14, 2.72)"
```

## 编译期格式字符串检查

```cpp
std::string s1 = std::format("{} {}", 1, 2);           // OK
// std::string s2 = std::format("{} {} {}", 1, 2);     // 编译错误：缺参数
```

## `vformat` 运行期格式化

```cpp
std::string fmt = "{}";
auto s = std::vformat(fmt, std::make_format_args(42));  // 运行期

std::string dynamic(std::string_view fmt, auto&&... args) {
    return std::vformat(fmt, std::make_format_args(args...));
}
```

## 输出到迭代器

```cpp
std::vector<char> buf;
std::format_to(std::back_inserter(buf), "x={}, y={}", 10, 20);

char small[8];
auto r = std::format_to_n(small, sizeof(small) - 1, "Hello, {}!", "World");
*r.out = '\0';  // 截断保护
```

## 与 `iostream` / `sprintf` 对比

| 特性 | `std::format` | `iostream` | `sprintf` |
|------|---------------|------------|-----------|
| 类型安全 | **编译期检查** | 是 | 否 |
| 性能 | **快** | 慢 | 快 |
| 安全性 | 安全 | 安全 | 缓冲区溢出 |

```cpp
std::cout << std::setw(10) << std::setfill('0') << 42;  // iostream
std::cout << std::format("{:010}", 42);                   // format
```

## `format_string` 约束

```cpp
template <typename... Args>
void log(std::format_string<Args...> fmt, Args&&... args) {
    auto msg = std::format(fmt, std::forward<Args>(args)...);
}
log("User {} from {}", "alice", "127.0.0.1");  // 编译期检查
```

## 常见陷阱

```cpp
std::format("{{literal}}");      // "{literal}"（花括号转义）
std::format("{}", true);        // "true"，不是 "1"
std::format("{:d}", true);      // "1"

// 自定义类型必须特化 formatter
struct MyType { int x; };
// std::format("{}", MyType{1});  // 编译错误
```

## 总结

- 类型安全、编译期检查、高性能的格式化，语法与 Python `.format()` 相似。
- 自定义类型通过特化 `std::formatter`。
- 优先用 `std::format_string<Args...>` 约束函数参数。
- `vformat` 用于运行期格式字符串场景。
