---
title: C++17 std::optional
topic: cpp17-basics
feature: optional
standard: C++17
status_checked_at: 2026-06-01
exercises:
  - exercises/cpp17-basics/optional1.cpp
solutions:
  - exercises/solutions/optional1.cpp
---
# C++17 std::optional

## Overview

`std::optional<T>` is a utility class template introduced in C++17 within `<optional>`, representing a **value that may or may not be present**. It provides a type-safe way to express "no value", replacing traditional techniques such as sentinel values (`-1`, `nullptr`), `std::pair<T, bool>`, or output parameters. It is particularly suitable as a function return type when an operation may successfully return a result or return nothing.

## Basic Construction

```cpp
#include <optional>
#include <string>

std::optional<int> opt_a{42};           // has value
std::optional<std::string> opt_b{"hello"};
std::optional<int> empty{};             // empty
std::optional<int> empty2 = std::nullopt; // empty, using nullopt constant
std::optional<double> opt_c = 3.14;     // implicit conversion
```

`std::nullopt` is a special constant representing "no value", with type `std::nullopt_t`.

## State Querying and Value Access

```cpp
std::optional<int> opt{42};
std::optional<int> empty;

// has_value() / operator bool
if (opt.has_value()) { /* has value */ }
if (opt) { /* equivalent form */ }

// operator* — does not check for emptiness, UB if empty
int v1 = *opt;

// value() — throws std::bad_optional_access if no value
try {
    int v2 = empty.value();
} catch (const std::bad_optional_access& e) {
    std::cout << e.what() << "\n";
}

// value_or(default) — returns default value if empty
int v3 = opt.value_or(0);      // 42
int v4 = empty.value_or(0);    // 0
```

| Method | With Value | Without Value |
|--------|-----------|---------------|
| `operator*` / `operator->` | Returns value/pointer | **Undefined behavior** |
| `value()` | Returns reference | Throws `bad_optional_access` |
| `value_or(default)` | Returns value copy | Returns `default` |

## emplace: In-Place Construction

```cpp
#include <optional>

struct Widget {
    int id;
    std::string name;
    Widget(int i, std::string n) : id(i), name(std::move(n)) {}
};

std::optional<Widget> opt;
opt.emplace(1, "first");    // constructs directly inside the optional
opt.emplace(2, "second");   // destructs old value first, then constructs new object
```

`emplace` avoids extra moves or copies, which is particularly important when construction is expensive.

## Comparison Operators

```cpp
std::optional<int> a{1}, b{2}, empty;

a < b;                    // true: compared by value
a == 1;                   // true: compared with a raw value
empty == std::nullopt;    // true
empty < a;                // true: nullopt is less than any optional with a value
```

Full `<`, `>`, `==`, `!=`, `<=`, `>=` comparisons are supported.

## Typical Usage: Replacing Error Codes

```cpp
#include <optional>
#include <map>
#include <string>

std::optional<int> find_user_id(
    const std::map<std::string, int>& db, const std::string& name)
{
    auto it = db.find(name);
    if (it != db.end()) return it->second;
    return std::nullopt;
}

void demo() {
    std::map<std::string, int> users = {{"alice", 1}, {"bob", 2}};

    if (auto id = find_user_id(users, "alice")) {
        std::cout << "found: " << *id << "\n";
    }

    // use value_or to provide a default value
    int uid = find_user_id(users, "eve").value_or(-1);
}
```

Compared to returning `-1` or `nullptr`, `std::optional` makes the "possibly no value" semantics explicitly visible in the type system.

## C++23 Monadic Operations (Mentioned)

C++23 added chaining operations to reduce boilerplate:

```cpp
// C++23 — transform (map)
auto doubled = std::optional{42}.transform([](int n) { return n * 2; });
// optional<int>{84}

// C++23 — and_then (flat_map)
auto result = std::optional{42}.and_then(
    [](int n) -> std::optional<std::string> {
        return n > 0 ? std::optional{std::to_string(n)} : std::nullopt;
    });

// C++23 — or_else
auto fallback = std::optional<int>{}.or_else(
    []() -> std::optional<int> { return 0; });
```

## reference_wrapper as a Reference Alternative

`std::optional<T&>` is not supported by the standard. Alternative:

```cpp
#include <optional>
#include <functional>

int x = 42;
std::optional<std::reference_wrapper<int>> opt_ref{x};
if (opt_ref) {
    opt_ref->get() = 100;  // x is now 100
}
```

## Best Practices

- **When a function may return "no result"**, prefer `std::optional<T>` as the return type.
- **Before accessing the value**, always check `has_value()` or use `value_or()`.
- **Use `emplace`** to construct complex objects in-place, avoiding unnecessary copies/moves.
- **Do not use `optional` to represent errors** — if error information is needed, use `std::expected` (C++23) or a custom error type.
- **Do not nest `optional<optional<T>>`** — two kinds of "empty" semantics are confusing.

## Common Pitfalls

```cpp
// Pitfall 1: dereferencing an empty optional (UB)
std::optional<int> empty;
// int x = *empty;  // undefined behavior! Must check first

// Pitfall 2: value() is unavailable in projects with exceptions disabled
// int x = empty.value();  // throws—use value_or or manual checking instead

// Pitfall 3: copy overhead of optional's value
std::optional<std::string> opt{"hello"};  // constructs a string
std::optional<std::string> opt2;
opt2.emplace("hello");                    // constructs directly inside, more efficient

// Pitfall 4: behavior of operator->
std::optional<std::string> opt_s{"test"};
opt_s->size();    // OK, equivalent to (*opt_s).size()
// std::optional<std::string> empty_s;
// empty_s->size();  // UB!
```
