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

`std::expected<T, E>` is an error handling utility introduced in C++23, representing a value that is either the expected result `T` or an error `E`. It provides an explicit error propagation mechanism that does not rely on exceptions.

## Basic Usage

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

## Creating an expected

```cpp
std::expected<int, std::string> ok = 42;                    // Construct from value
std::expected<int, std::string> err =
    std::unexpected<std::string>("failed");                  // Construct from unexpected

auto e = std::unexpected(ParseError::InvalidFormat);
std::expected<int, ParseError> res = e;
```

## Accessing Values and Errors

```cpp
std::expected<int, std::string> result = 42;

int v = result.value();          // Throws std::bad_expected_access if it holds an error
int v2 = *result;                // operator* — UB if no value

std::expected<int, std::string> err = std::unexpected("fail");
std::string e = err.error();     // Undefined behavior if no error
int safe = err.value_or(0);      // Safe fallback → 0
```

## Monadic Operations

Monadic operations avoid manual `if` checks, enabling chained composition:

### transform — Map the success value

```cpp
auto result = parse_int("42");
auto doubled = result.transform([](int v) { return v * 2; });
// doubled is expected<int, ParseError> with value 84
```

### and_then — Chain operations that may fail

```cpp
std::expected<int, std::string> find_user_id(const std::string& name);
std::expected<User, std::string> load_user(int id);

auto user = find_user_id("Alice")
    .and_then([](int id) { return load_user(id); });
```

### or_else — Handle errors and optionally recover

```cpp
auto result = parse_int("abc").or_else(
    [](ParseError e) -> std::expected<int, ParseError> {
        if (e == ParseError::InvalidFormat) return 0;
        return std::unexpected(e);
    });
```

### Composition chain

```cpp
auto final_result = parse_int(input)
    .and_then(validate_range)
    .transform(to_string)
    .or_else(handle_error);
```

## Differences from std::optional

| Property | `std::optional<T>` | `std::expected<T, E>` |
|----------|--------------------|-----------------------|
| Error info | None (only nullopt) | Carries typed error `E` |
| Use case | Value may not exist | Operation may fail with a reason |

## Comparison with Exceptions

```cpp
// Exception approach — implicit control flow
int parse(const std::string& s) {
    int val = std::stoi(s);
    if (val < 0) throw std::out_of_range("negative");
    return val;
}

// expected approach — explicit error propagation
std::expected<int, ErrorCode> parse(const std::string& s) {
    int val = std::stoi(s);
    if (val < 0) return std::unexpected(ErrorCode::Negative);
    return val;
}
```

Advantages of expected: no exception unwinding overhead, suitable for high-frequency error paths; the function signature explicitly lists error types; monadic operations support branchless chaining.

## Practical Usage Patterns

```cpp
// Pipeline-style processing
std::expected<Response, ApiError> process_request(const Request& req) {
    return validate(req)
        .and_then(authenticate)
        .and_then(execute)
        .transform(format_response);
}
```

## Caveats

- `E` cannot be `void` or a reference
- `expected<void, E>` is valid, indicating an operation may return no value but may fail
- Move semantics are complete, supporting move construction and move assignment
- Does not implicitly convert between `T` and `E`; errors must be explicitly constructed with `std::unexpected`
