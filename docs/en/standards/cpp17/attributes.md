---
title: "C++17 Attributes: `[[nodiscard]]`, `[[maybe_unused]]`, `[[fallthrough]]`"
topic: unknown
feature: attributes
standard: N/A
status_checked_at: 2026-06-02
---
# C++17 Attributes: `[[nodiscard]]`, `[[maybe_unused]]`, `[[fallthrough]]`

## Overview

C++17 standardized three practical attributes that provide semantic hints to compilers. `[[nodiscard]]` prevents ignoring important return values, `[[maybe_unused]]` suppresses unused variable warnings, and `[[fallthrough]]` explicitly marks the intent of switch case fallthrough.

## Syntax

```cpp
[[nodiscard]] int compute();
[[maybe_unused]] int debug_val = 42;
void callback([[maybe_unused]] int event_type) { /* ... */ }

switch (state) {
    case A:
        do_something();
        [[fallthrough]];    // semicolon is required
    case B:
        handle_b();
        break;
}
```

## `[[nodiscard]]`

### Basic Usage

```cpp
[[nodiscard]] bool validate(const Data& d);
[[nodiscard]] std::unique_ptr<Resource> acquire();

void process() {
    validate(data);           // warning: nodiscard return value ignored
    auto ok = validate(data); // correct
    acquire();                // warning
    auto r = acquire();       // correct
}
```

### Modifying Classes and Enums

```cpp
// class type—functions returning this type are automatically nodiscard
[[nodiscard]] struct ErrorCode {
    int code;
    explicit operator bool() const { return code == 0; }
};

ErrorCode do_work();  // automatically nodiscard

// enum type
enum [[nodiscard]] class Status { Ok, Error, Timeout };
Status connect(const std::string& host);
connect("localhost");  // warning
```

### `[[nodiscard]]` and `void`

`[[nodiscard]]` is only effective when the return type is non-`void`. `void` functions have no return value to ignore.

### Explicit Discarding

```cpp
[[nodiscard]] int important();
static_cast<void>(important()); // explicit discard, no warning
(void)important();              // same as above
```

## `[[maybe_unused]]`

### Variables and Parameters

```cpp
void debug_example() {
    [[maybe_unused]] int x = compute_something(); // only used in debug builds
}

// parameters—omitting the parameter name is often more concise
void on_event([[maybe_unused]] int event_type, void* user_data) {
    auto* ctx = static_cast<Context*>(user_data);
    ctx->notify();
}
```

### Types and Static Members

```cpp
[[maybe_unused]] using DebugType = std::vector<int>;

struct Traits {
    [[maybe_unused]] static constexpr bool is_special = true;
};
```

In most cases, omitting the parameter name is more concise than `[[maybe_unused]]`: `void f(int /*type*/, void* ctx)`.

## `[[fallthrough]]`

```cpp
void interpret(Token tok) {
    switch (tok) {
        case Token::Number:
            read_number();
            [[fallthrough]];  // intentional fallthrough
        case Token::Plus:
        case Token::Minus:
            apply_operator();
            break;
    }
}
```

Syntax note: `[[fallthrough]]` must be followed by a semicolon — it is an attribute statement, not a control flow statement.

## Combining Multiple Attributes

```cpp
[[nodiscard, maybe_unused]] int legacy_api();

// equivalent to
[[nodiscard]]
[[maybe_unused]]
int legacy_api();
```

## C++20 Enhancement

C++20 allows `[[nodiscard]]` to carry a diagnostic message:

```cpp
[[nodiscard("memory will leak if result is discarded")]]
void* allocate(std::size_t size);

allocate(1024);  // warning message: memory will leak if result is discarded
```

## Best Practices

1. **Use `[[nodiscard]]` for error code/status return values**: `[[nodiscard]] bool save(const Document& doc);`
2. **Use `[[nodiscard]]` for ownership-returning functions**: `[[nodiscard]] std::unique_ptr<Connection> connect();`
3. **`[[fallthrough]]` is mandatory**: all intentional case fallthroughs should be annotated; `-Wimplicit-fallthrough` enforces this.
4. **Prefer avoiding fallthrough in switch**: use separate function calls or refactored logic.
5. **Use parameter omission instead of `[[maybe_unused]]`**: `void f(int /*type*/, void* ctx);`

## Common Pitfalls

- **`[[nodiscard]]` is only a warning**: it cannot replace correctness checks; `-Werror` is needed to make it an error.
- **`[[maybe_unused]]` may mask bugs**: if a variable was supposed to be used but was forgotten, the attribute hides the logic error.
- **`[[fallthrough]]` placement must be correct**: it must be at the end of a case block; placing it in the middle has no effect.
- **Attributes do not affect semantics**: none of the three attributes change program behavior; removing them results in identical behavior.
- **`static_cast<void>(expr)` is the idiomatic way to discard return values**: use it when the return value is genuinely unneeded.
