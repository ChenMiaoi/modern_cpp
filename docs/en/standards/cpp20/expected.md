---
title: "`std::expected`: Value or Error"
topic: unknown
feature: expected
standard: N/A
status_checked_at: 2026-06-02
---
# `std::expected`: Value or Error

## Overview

`std::expected<T, E>` represents an operation result — either containing a success value `T` or a failure error `E`. Proposed by P0323, initially targeting C++20, it was incorporated into **C++23** after multiple revisions. This article is based on the C++23 specification, as its conceptual origins are rooted in C++20-era work.

## P0323 History

| Version | Year | Changes |
|---------|------|---------|
| P0323R0 | 2016 | Initial proposal |
| P0323R7 | 2019 | Entered early C++20 draft |
| P0323R9 | 2021 | Removed from C++20 (design not converged) |
| P0323R12 | 2022 | Incorporated into C++23 |

Reason for removal: `expected<void, E>` design, interaction with `variant`, and exception policy needed more discussion.

## Basic Usage

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

## Construction and Access

```cpp
std::expected<int, std::string> ok{42};
std::expected<int, std::string> err{std::unexpected("not found")};

ok.has_value();       // true
*ok;                  // 42
ok.value();           // 42
ok.value_or(0);       // 42

err.error();          // "not found"
err.value_or(0);      // 0
// err.value();       // throws std::bad_expected_access
```

## Monadic Operations

### `and_then`: Chain the Success Path

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

### `or_else`: Error Recovery

```cpp
auto result = parse("abc")
    .or_else([](Error e) -> std::expected<int, Error> {
        return std::unexpected(Error::Parse);
    });
```

### `transform`: Map the Success Value

```cpp
auto doubled = parse("42")
    .transform([](int v) { return v * 2; });
// doubled contains 84
```

### `transform_error`: Map the Error Type

```cpp
auto mapped = parse("abc")
    .transform_error([](Error e) -> std::string {
        return "code " + std::to_string(static_cast<int>(e));
    });
```

## Comparison with `std::optional`

| Feature | `std::optional<T>` | `std::expected<T, E>` |
|---------|--------------------|-----------------------|
| Error information | None | Type `E` carries it |
| Monadic operations | Available from C++23 | Full support |
| Use case | Simple present/absent | Distinguish error reasons |

```cpp
// optional: error reason is lost
std::optional<int> find(std::string_view key);

// expected: carries error context
enum class LookupError { NotFound, Denied };
std::expected<int, LookupError> find_ex(std::string_view key);
```

## Comparison with `std::variant`

| Feature | `std::variant<Ts...>` | `std::expected<T, E>` |
|---------|----------------------|-----------------------|
| Number of types | ≥ 2, arbitrary | Fixed at 2 |
| Semantics | Generic multi-type | Success/failure |
| Monadic operations | None | Yes |

## `void` Specialization

```cpp
enum class ErrorCode { Timeout, Denied };

std::expected<void, ErrorCode> connect(std::string_view host) {
    if (host.empty()) return std::unexpected(ErrorCode::Timeout);
    return {};
}
```

## Error Handling Patterns

```cpp
// Chained propagation
std::expected<int, ConfigError> read_config(std::string_view path) {
    return open_file(path)
        .and_then(parse)
        .and_then(extract_value);
}

// Explicit checking
std::expected<int, ConfigError> read_config_v2(std::string_view path) {
    auto file = open_file(path);
    if (!file) return std::unexpected(file.error());
    auto data = parse(*file);
    if (!data) return std::unexpected(data.error());
    return extract_value(*data);
}
```

## Common Pitfalls

```cpp
// Same type for success/error must use std::unexpected
std::expected<int, int> e1{42};                   // success
std::expected<int, int> e2{std::unexpected(42)};  // error

// error() throws when a value is present
// ok.error();  // throws std::bad_expected_access

// E cannot be a reference type
// std::expected<int, int&>  // compile error
```

## Summary

- Standardized in C++23 (P0323), conceptual origins rooted in C++20.
- Carries richer error information than `optional`, with clearer semantics than `variant`.
- `and_then` / `or_else` / `transform` support chained error propagation.
- `void` specialization for operations with no return value.
- Prefer over exceptions for recoverable error handling.
