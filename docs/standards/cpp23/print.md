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

C++23 引入 `std::print` 和 `std::println`，提供 Python 风格的格式化输出，基于 `std::format` 实现，绕过了 iostream 的复杂性和性能开销。

## 基本用法

```cpp
#include <print>

int main() {
    std::print("Hello, {}!\n", "world");
    std::println("Hello, {}!", "world");  // 自动追加换行

    int x = 42;
    std::println("x = {}", x);

    // 多个参数
    std::println("{} + {} = {}", 3, 4, 3 + 4);
}
```

## 格式化语法

`std::print` 使用 `std::format` 的格式化字符串语法：

```cpp
// 位置参数
std::print("{0} is {1}", "Pi", 3.14);

// 对齐与填充
std::print("{:>10}", "right");    // "     right"
std::print("{:<10}", "left");     // "left      "
std::print("{:^10}", "center");   // "  center  "
std::print("{:*^10}", "hi");      // "****hi****"

// 数值格式
std::print("{:.2f}", 3.14159);    // "3.14"
std::print("{:#x}", 255);         // "0xff"
std::print("{:08b}", 42);         // "00101010"
std::print("{:+d}", 42);          // "+42"

// 类型特定格式
std::print("{:s}", true);         // "true" (非 "1")
```

## 输出目标

```cpp
#include <print>
#include <fstream>

int main() {
    // 输出到 stdout
    std::print("to stdout\n");

    // 输出到 stderr
    std::print(stderr, "to stderr\n");

    // 输出到文件
    std::ofstream file("output.txt");
    std::print(file, "to file: {}\n", 42);

    // 输出到任意 output stream
    // 任何支持 std::format_to 的输出迭代器均可
}
```

## 与 std::cout 的对比

```cpp
// iostream 方式 — 冗长
std::cout << "Name: " << name << ", Age: " << age << ", Score: "
          << std::fixed << std::setprecision(2) << score << "\n";

// print 方式 — 简洁
std::print("Name: {}, Age: {}, Score: {:.2f}", name, age, score);
```

| 特性 | `std::cout` | `std::print` |
|------|-------------|--------------|
| 格式化 | manipulator 链 | Python 风格 `{}` |
| 类型安全 | 运行时 | 编译期检查 |
| 性能 | 较慢（同步、facet） | 更快（直接格式化） |
| 本地化 | 通过 locale facet | 通过 `std::locale` 参数 |

## 与 printf 的对比

```cpp
// printf — 类型不安全
printf("Name: %s, Age: %d, Score: %.2f", name, age, score);

// std::print — 类型安全
std::print("Name: {}, Age: {}, Score: {:.2f}", name, age, score);
```

| 特性 | `printf` | `std::print` |
|------|----------|--------------|
| 类型安全 | 否（UB 于类型不匹配） | 是（编译期检查） |
| 用户自定义类型 | 否 | 是（通过 `std::formatter` 特化） |
| Unicode | 平台依赖 | 标准化支持 |
| 缓冲 | stdout 缓冲区 | 目标流的缓冲区 |

## 性能优势

`std::print` 的性能优势主要来自：

1. **无 iostream 开销**：不经过 `std::ostream` 的虚函数调用链
2. **直接写入**：格式化结果直接写入目标缓冲区
3. **编译期格式解析**：格式字符串在编译期解析，运行时零额外开销
4. **无同步开销**：C++23 的 print 不强制与 C stdout 同步

```cpp
// 性能敏感场景下显著更快
for (int i = 0; i < 1000000; ++i) {
    std::print("{:8d}\n", i);  // 比 cout << setw(8) << i 快
}
```

## 自定义类型支持

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

## println 的便利

```cpp
std::println();                    // 只输出换行
std::println("simple message");    // 带换行，无需手动 \n
std::println("{} items", count);   // 格式化 + 换行
```

`println` 等价于 `print` 加一个 `"\n"`，避免了忘记换行的常见问题。

## 注意事项

- 需要编译器支持：GCC 13+、MSVC 17.5+、Clang 17+
- `std::print` 不刷新缓冲区，若需立即输出需手动 `fflush` 或使用 `std::flush`
- 格式字符串必须是编译期常量（constexpr），非法格式在编译期报错
- 对于自定义类型，必须特化 `std::formatter`
