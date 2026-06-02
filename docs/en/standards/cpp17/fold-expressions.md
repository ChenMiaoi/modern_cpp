---
title: "C++17 Fold Expressions"
topic: unknown
feature: fold-expressions
standard: N/A
status_checked_at: 2026-06-02
---
# C++17 Fold Expressions

## Overview

Fold expressions are a language feature introduced in C++17 that allow applying a binary operator to all arguments in a parameter pack, "folding" them into a single value. They eliminate the verbose approach of using recursive templates or initializer list expansion to process variadic templates, making them one of the most practical syntactic sugars in C++17.

## Four Fold Forms

```cpp
// unary right fold
(args op ...)          →  (E₁ op (E₂ op (... op Eₙ)))

// unary left fold
(... op args)          →  (((E₁ op E₂) op ...) op Eₙ)

// binary right fold
(args op ... op init)  →  (E₁ op (E₂ op (... op (Eₙ op init))))

// binary left fold
(init op ... op args)  →  ((((init op E₁) op E₂) op ...) op Eₙ)
```

Supported operators include arithmetic, bitwise, comparison, `&&`, `||`, `,`, and more.

## Summation Example

```cpp
#include <iostream>

// unary fold: compile error on empty pack
template<typename... Args>
auto sum(Args... args) { return (args + ...); }

// binary fold: with initial value, safe for empty pack
template<typename... Args>
auto sum_safe(Args... args) { return (0 + ... + args); }

int main() {
    std::cout << sum(1, 2, 3, 4, 5) << "\n";      // 15
    std::cout << sum_safe() << "\n";               // 0
}
```

Unary folds have no default value for empty packs (except `&&`→`true`, `||`→`false`, `,`→`void()`); binary folds are always safe.

## Logical Folds

```cpp
#include <type_traits>

template<typename... Ts>
constexpr bool all_integral() {
    return (std::is_integral_v<Ts> && ...);
}

template<typename... Args>
bool all_positive(Args... args) {
    return ((args > 0) && ...);
}

static_assert(all_integral<int, long, char>());
static_assert(!all_integral<int, double>());
```

`&&` and `||` folds have short-circuit evaluation semantics.

## Comma Fold: Iterating Over a Parameter Pack

```cpp
#include <iostream>

template<typename... Args>
void print_all(Args... args) {
    ((std::cout << args << " "), ...);  // comma fold
    std::cout << "\n";
}

print_all(1, "hello", 3.14, 'X');  // 1 hello 3.14 X
```

## Lambda Folds

```cpp
#include <string>

template<typename... Args>
std::string join(const std::string& sep, Args... args) {
    std::string result;
    bool first = true;
    auto append = [&](const auto& val) {
        if (!first) result += sep;
        result += std::to_string(val);
        first = false;
    };
    (append(args), ...);
    return result;
}

template<typename Func, typename... Args>
void for_each(Func func, Args&&... args) {
    (func(std::forward<Args>(args)), ...);
}
```

## Left Fold vs. Right Fold

Results differ for non-associative operators:

```cpp
// left fold: (((1 - 2) - 3) - 4) = -8
(... - args)

// right fold: (1 - (2 - (3 - 4))) = -2
(args - ...)
```

For associative operators (`+`, `*`, `&&`, `||`), both directions produce the same result.

## Practical Examples

```cpp
// compile-time type checking
template<typename T, typename... Ts>
constexpr bool all_same() {
    return (std::is_same_v<T, Ts> && ...);
}

// batch container operations
template<typename Container, typename... Values>
void push_all(Container& c, Values&&... values) {
    (c.push_back(std::forward<Values>(values)), ...);
}

// multi-condition matching
template<typename T, typename... Args>
bool is_any_of(const T& val, const Args&... args) {
    return ((val == args) || ...);
}
```

## Empty Pack Handling Rules

| Fold Form | Empty Pack Result |
|-----------|-------------------|
| `(args && ...)` | `true` |
| `(args \|\| ...)` | `false` |
| `(args, ...)` | `void()` |
| Other unary folds | **Compile error** |
| Binary fold `(init op ... op args)` | Returns `init` |

## Best Practices

- **Prefer binary folds** for safe empty pack handling.
- **Short-circuit logic**: `&&`/`||` folds can terminate early.
- **Side-effect operations**: comma fold `(func(args), ...)` iterates over the parameter pack.
- **Perfect forwarding fold**: `(std::forward<Args>(args), ...)` is the standard pattern for variadic forwarding.
- **Ensure type consistency**: the operator must be valid for all argument types.

## Common Pitfalls

```cpp
// Pitfall 1: unary fold with empty pack
template<typename... Args>
auto bad_sum(Args... args) { return (args + ...); }
// bad_sum() compile error—fix: (0 + ... + args)

// Pitfall 2: fold direction affects non-associative operations
// (args - ...) ≠ (... - args)

// Pitfall 3: operator precedence
// wrong: return ((args == 42) && ...);  // parentheses needed for correct precedence
```
