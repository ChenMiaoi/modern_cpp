---
title: std::expected
topic: cpp23
feature: expected
standard: C++23
status_checked_at: 2026-06-02
standard_refs:
  - draft: N4950
    clause: "[expected]"
proposals:
  - paper: P0323
    revision: R12
    status: accepted
exercises: []
solutions: []
---
# std::expected

`std::expected<T, E>` 是 C++23 引入的错误处理工具，表示一个值要么是期望的结果 `T`，要么是错误 `E`。它提供了一种不依赖异常的显式错误传播机制。

## 基本用法

```cpp
#include <expected>
#include <string>
#include <iostream>

enum class ParseError { InvalidFormat, OutOfRange };

std::expected<int, ParseError> parse_int(const std::string& s) {
    try {
        size_t pos;
        int val = std::stoi(s, &pos);
        if (pos != s.size()) return std::unexpected(ParseError::InvalidFormat);
        return val;
    } catch (...) {
        return std::unexpected(ParseError::OutOfRange);
    }
}

int main() {
    auto result = parse_int("42");
    if (result.has_value()) {
        std::cout << "parsed: " << result.value() << "\n";
    }

    auto bad = parse_int("abc");
    if (!bad.has_value()) {
        std::cout << "error: " << static_cast<int>(bad.error()) << "\n";
    }
}
```

## 创建 expected

```cpp
std::expected<int, std::string> ok = 42;                    // 从值构造
std::expected<int, std::string> err =
    std::unexpected<std::string>("failed");                  // 从 unexpected 构造

auto e = std::unexpected(ParseError::InvalidFormat);
std::expected<int, ParseError> res = e;
```

## 访问值与错误

```cpp
std::expected<int, std::string> result = 42;

int v = result.value();          // 若含错误则抛 std::bad_expected_access
int v2 = *result;                // operator* — UB 若无值

std::expected<int, std::string> err = std::unexpected("fail");
std::string e = err.error();     // 无错误时行为未定义
int safe = err.value_or(0);      // 安全回退 → 0
```

## 单子操作（Monadic Operations）

单子操作避免手动 `if` 检查，支持链式组合：

### transform — 映射成功值

```cpp
auto result = parse_int("42");
auto doubled = result.transform([](int v) { return v * 2; });
// doubled 是 expected<int, ParseError>，值为 84
```

### and_then — 链接可能失败的操作

```cpp
std::expected<int, std::string> find_user_id(const std::string& name);
std::expected<User, std::string> load_user(int id);

auto user = find_user_id("Alice")
    .and_then([](int id) { return load_user(id); });
```

### or_else — 处理错误并可能恢复

```cpp
auto result = parse_int("abc").or_else(
    [](ParseError e) -> std::expected<int, ParseError> {
        if (e == ParseError::InvalidFormat) return 0;
        return std::unexpected(e);
    });
```

### 组合链

```cpp
auto final_result = parse_int(input)
    .and_then(validate_range)
    .transform(to_string)
    .or_else(handle_error);
```

## 与 std::optional 的区别

| 特性 | `std::optional<T>` | `std::expected<T, E>` |
|------|--------------------|-----------------------|
| 错误信息 | 无（仅 nullopt） | 携带类型化的错误 `E` |
| 使用场景 | 值可能不存在 | 操作可能失败并需知原因 |

## 与异常的对比

```cpp
// 异常方式 — 隐式控制流
int parse(const std::string& s) {
    int val = std::stoi(s);
    if (val < 0) throw std::out_of_range("negative");
    return val;
}

// expected 方式 — 显式错误传播
std::expected<int, ErrorCode> parse(const std::string& s) {
    int val = std::stoi(s);
    if (val < 0) return std::unexpected(ErrorCode::Negative);
    return val;
}
```

expected 的优势：性能上无异常展开开销，适合高频错误路径；可读性上函数签名明确列出错误类型；组合性上单子操作支持无分支的链式调用。

## 实际使用模式

```cpp
// 管道式处理
std::expected<Response, ApiError> process_request(const Request& req) {
    return validate(req)
        .and_then(authenticate)
        .and_then(execute)
        .transform(format_response);
}
```

## 注意事项

- `E` 不能是 `void`，不能是引用
- `expected<void, E>` 合法，表示操作可能无值返回但可能失败
- 移动语义完备，支持移动构造和移动赋值
- 不会隐式转换 `T` 和 `E`，需显式使用 `std::unexpected` 构造错误
