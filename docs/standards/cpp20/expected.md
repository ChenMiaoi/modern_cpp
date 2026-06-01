# `std::expected`：值或错误

## 概述

`std::expected<T, E>` 表示操作结果——要么包含成功值 `T`，要么包含失败错误 `E`。由 P0323 提出，最初目标 C++20，多次修订后纳入 **C++23**。本文基于 C++23 规范介绍，因其概念起源植根于 C++20 时期的工作。

## P0323 历史

| 版本 | 年份 | 变化 |
|------|------|------|
| P0323R0 | 2016 | 首次提出 |
| P0323R7 | 2019 | 进入 C++20 早期草案 |
| P0323R9 | 2021 | 移出 C++20（设计未收敛） |
| P0323R12 | 2022 | 纳入 C++23 |

移出原因：`expected<void, E>` 设计、与 `variant` 的交互、异常策略需更多讨论。

## 基本用法

```cpp
#include <expected>
#include <charconv>

enum class ParseError { InvalidFormat, OutOfRange };

std::expected<int, ParseError> parse_int(std::string_view sv) {
    int value;
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
    if (ec != std::errc{}) return std::unexpected(ParseError::InvalidFormat);
    return value;
}

auto ok = parse_int("42");       // has_value() == true
auto fail = parse_int("abc");    // has_value() == false
```

## 构造与访问

```cpp
std::expected<int, std::string> ok{42};
std::expected<int, std::string> err{std::unexpected("not found")};

ok.has_value();       // true
*ok;                  // 42
ok.value();           // 42
ok.value_or(0);       // 42

err.error();          // "not found"
err.value_or(0);      // 0
// err.value();       // 抛 std::bad_expected_access
```

## 单子操作

### `and_then`：链式成功路径

```cpp
enum class Error { Parse, DivByZero };

std::expected<int, Error> parse(std::string_view s);
std::expected<double, Error> divide(int a, int b) {
    if (b == 0) return std::unexpected(Error::DivByZero);
    return static_cast<double>(a) / b;
}

auto result = parse("100")
    .and_then([](int v) { return divide(v, 3); });
```

### `or_else`：错误恢复

```cpp
auto result = parse("abc")
    .or_else([](Error e) -> std::expected<int, Error> {
        return std::unexpected(Error::Parse);
    });
```

### `transform`：映射成功值

```cpp
auto doubled = parse("42")
    .transform([](int v) { return v * 2; });
// doubled 包含 84
```

### `transform_error`：映射错误类型

```cpp
auto mapped = parse("abc")
    .transform_error([](Error e) -> std::string {
        return "code " + std::to_string(static_cast<int>(e));
    });
```

## 与 `std::optional` 对比

| 特性 | `std::optional<T>` | `std::expected<T, E>` |
|------|--------------------|-----------------------|
| 错误信息 | 无 | 类型 `E` 携带 |
| 单子操作 | C++23 起有 | 完整支持 |
| 适用场景 | 简单有/无 | 区分错误原因 |

```cpp
// optional：错误原因丢失
std::optional<int> find(std::string_view key);

// expected：携带错误上下文
enum class LookupError { NotFound, Denied };
std::expected<int, LookupError> find_ex(std::string_view key);
```

## 与 `std::variant` 对比

| 特性 | `std::variant<Ts...>` | `std::expected<T, E>` |
|------|----------------------|-----------------------|
| 类型数量 | ≥ 2，任意 | 固定 2 |
| 语义 | 通用多类型 | 成功/失败 |
| 单子操作 | 无 | 有 |

## `void` 特化

```cpp
enum class ErrorCode { Timeout, Denied };

std::expected<void, ErrorCode> connect(std::string_view host) {
    if (host.empty()) return std::unexpected(ErrorCode::Timeout);
    return {};
}
```

## 错误处理模式

```cpp
// 链式传播
std::expected<int, ConfigError> read_config(std::string_view path) {
    return open_file(path)
        .and_then(parse)
        .and_then(extract_value);
}

// 显式检查
std::expected<int, ConfigError> read_config_v2(std::string_view path) {
    auto file = open_file(path);
    if (!file) return std::unexpected(file.error());
    auto data = parse(*file);
    if (!data) return std::unexpected(data.error());
    return extract_value(*data);
}
```

## 常见陷阱

```cpp
// 同类型区分成功/错误必须用 std::unexpected
std::expected<int, int> e1{42};                   // 成功
std::expected<int, int> e2{std::unexpected(42)};  // 错误

// error() 在有值时抛异常
// ok.error();  // 抛 std::bad_expected_access

// E 不能是引用类型
// std::expected<int, int&>  // 编译错误
```

## 总结

- C++23 标准化（P0323），概念起源植根 C++20。
- 比 `optional` 携带更丰富错误信息，比 `variant` 语义更明确。
- `and_then` / `or_else` / `transform` 支持链式错误传播。
- `void` 特化用于无返回值操作。
- 优先替代异常进行可恢复错误处理。
