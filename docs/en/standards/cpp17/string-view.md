---
title: "C++17 `std::string_view`"
topic: unknown
feature: string-view
standard: N/A
status_checked_at: 2026-06-02
---
# C++17 `std::string_view`

## Overview

`std::string_view` is a non-owning string reference type introduced in C++17, providing a read-only view over a contiguous sequence of characters. It allocates no memory and copies no data. Composed of a pointer and a length, it replaces `const std::string&` parameters to eliminate unnecessary heap allocations, making it a quintessential zero-cost abstraction.

## Syntax

```cpp
#include <string_view>

std::string_view sv1 = "hello";                 // from string literal
std::string_view sv2("hello world", 5);          // from pointer + length
std::string_view sv3 = std::string("hello");     // from std::string

sv1.size();      // 5
sv1.empty();     // false
sv1[0];          // 'h'
sv1.data();      // const char* (not guaranteed to be null-terminated)
```

## Non-Owning Semantics and Lifetime

```cpp
std::string_view get_view() {
    std::string s = "temporary";
    return s;  // dangerous: view is dangling after s is destroyed
}

std::string_view safe_view(const std::string& s) {
    return s;  // safe: caller guarantees s's lifetime
}

// string literals have static storage duration, always safe
std::string_view sv = "compile-time literal";
```

## Null-Termination Awareness

`string_view` does **not** guarantee null termination:

```cpp
std::string_view sv("hello world", 5);  // "hello"
sv.data()[sv.size()];  // undefined behavior: may not be '\0'

// must convert when passing to C APIs
// C_API(sv.data());              // dangerous
C_API(std::string(sv).c_str());    // safe
```

## Substring Operations — Zero Allocation

```cpp
std::string_view sv = "Hello, World!";
std::string_view sub = sv.substr(7, 5);  // "World"
// sub.data() points to sv.data() + 7, just a pointer shift

// compare with std::string::substr—allocates heap memory every time
std::string s = "Hello, World!";
std::string sub2 = s.substr(7, 5);       // heap allocation
```

### remove_prefix / remove_suffix

```cpp
std::string_view sv = "  Hello, World!  ";
sv.remove_prefix(2);   // "Hello, World!  "
sv.remove_suffix(2);   // "Hello, World"

// useful for trim operations
auto trim_left(std::string_view sv) -> std::string_view {
    while (!sv.empty() && std::isspace(sv.front()))
        sv.remove_prefix(1);
    return sv;
}
```

:::warning Irreversible
`remove_prefix` and `remove_suffix` cannot be undone. Copy the original view first if you need to preserve it.
:::

## As Function Parameters — Performance Advantage

```cpp
// C++14: string literal implicitly constructs a temporary string, allocating heap memory
void process_old(const std::string& s);
process_old("hello");  // allocates

// C++17: zero allocation
void process_new(std::string_view s);
process_new("hello");           // zero allocation
process_new(std::string("x"));  // zero allocation
```

### Trade-offs with `const char*`

| Feature | `const char*` | `string_view` |
|---------|--------------|---------------|
| Includes length | No | Yes |
| Null-terminated | Guaranteed | Not guaranteed |
| `size()` complexity | O(n) | O(1) |
| Supports embedded `\0` | No | Yes |

## Search Operations

```cpp
std::string_view sv = "Hello, World!";
sv.find("World");              // 7
sv.find_first_of("aeiou");     // 1
sv.starts_with("Hello");       // true (C++20)
// returns npos if not found
```

## Conversion to std::string

```cpp
std::string_view sv = "hello";
std::string s(sv);              // explicit construction, copies data

// C++17 does not support implicit conversion (by design)
// C++23 supports direct assignment: std::string s = sv;
```

## Practical Applications

### Zero-Allocation Slicing in Parsers

```cpp
struct Token {
    std::string_view text;
    int line, column;
};

std::vector<Token> tokenize(std::string_view source) {
    std::vector<Token> tokens;
    while (!source.empty()) {
        auto pos = source.find_first_of(" \t\n");
        if (pos == std::string_view::npos) pos = source.size();
        tokens.push_back({source.substr(0, pos), 0, 0});
        source.remove_prefix(std::min(pos + 1, source.size()));
    }
    return tokens;
}
```

### Unified Function Parameters

```cpp
void configure(std::string_view key, std::string_view value) {
    // accepts: const char*, std::string, string_view, string literals
}
configure("host", "localhost");
```

## Best Practices

1. **Prefer `string_view` for function parameters**: replaces `const std::string&` and `const char*`.
2. **Still use `std::string` for stored copies**: class members should not use `string_view`.
3. **Convert to `std::string` before passing to C APIs**: `data()` is not null-terminated.
4. **`substr` returns `string_view` (zero allocation)** — different behavior from `string::substr`.

## Common Pitfalls

- **Dangling views**: pointing to a destroyed `std::string` is the most common mistake.
- **Not null-terminated**: do not assume `data()[size()] == '\0'`.
- **Read-only**: cannot modify the underlying data through `string_view`.
- **`remove_prefix/suffix` are irreversible**: copy the original view first if needed.
- **Container storage caveat**: `vector<string_view>` is suitable for short lifetimes; use `vector<string>` for long-term storage.
