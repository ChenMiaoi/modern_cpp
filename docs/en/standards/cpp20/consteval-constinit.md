---
title: "`consteval` and `constinit`"
topic: unknown
feature: consteval-constinit
standard: N/A
status_checked_at: 2026-06-02
---
# `consteval` and `constinit`

## Overview

C++20 introduces two new keywords:
- **`consteval`**: immediate function, **must** be evaluated at compile time, otherwise compilation fails.
- **`constinit`**: enforces constant initialization, preventing SIOF, but the variable itself is **not** `const`.

## `consteval`: Immediate Functions

```cpp
consteval int square(int n) { return n * n; }

int main() {
    constexpr int a = square(5);   // OK
    int x = 10;
    // int b = square(x);          // error: x is not constant

    int arr[square(3)];             // arr[9]
    static_assert(square(4) == 16);
}
```

### `consteval` vs `constexpr`

```cpp
constexpr int f1(int n) { return n * 2; }   // compile-time or runtime
consteval int f2(int n) { return n * 2; }   // must be compile-time

int main() {
    int rt = 42;
    constexpr int a = f1(5);     // OK
    int b = f1(rt);              // OK: runtime
    constexpr int c = f2(5);     // OK
    // int d = f2(rt);           // error
}
```

| Feature | `constexpr` | `consteval` |
|---------|-------------|-------------|
| Must evaluate at compile time | No | **Yes** |
| Callable at runtime | Yes | No |

## `constinit`: Constant Initialization

`constinit` guarantees **compile-time initialization** of a variable, avoiding SIOF, but does not require immutability.

### SIOF Problem

```cpp
// a.cpp
int compute() { return 42; }
int g_a = compute();  // runtime initialization, order indeterminate

// b.cpp
extern int g_a;
int g_b = g_a + 1;   // g_a may not be initialized yet → UB
```

### `constinit` Fix

```cpp
constexpr int compute() { return 42; }
constinit int g_value = compute();  // enforces compile-time initialization

void update() {
    g_value = 100;  // OK: constinit does not restrict mutability
}
```

### `constinit` vs `constexpr` Variables

```cpp
constexpr int ci = 42;     // const + constant initialization, immutable
constinit int cni = 42;    // constant initialization, mutable

void f() {
    // ci = 10;            // error: ci is const
    cni = 10;              // OK
}
```

## Three-Way Comparison

| Feature | `constexpr` | `consteval` | `constinit` |
|---------|-------------|-------------|-------------|
| Applicable to | Variables / Functions | Functions only | Variables only |
| Must evaluate at compile time | No | Yes | Yes (initialization) |
| Variable is const | Yes | — | No |
| Prevents SIOF | Indirect | — | **Direct** |
| First introduced | C++11 | C++20 | C++20 |

## Practical Use Cases

### Compile-Time Lookup Table

```cpp
consteval std::array<uint8_t, 256> make_crc_table() {
    std::array<uint8_t, 256> table{};
    for (int i = 0; i < 256; ++i) {
        uint8_t crc = static_cast<uint8_t>(i);
        for (int j = 0; j < 8; ++j)
            crc = (crc & 1) ? (crc >> 1) ^ 0x8C : (crc >> 1);
        table[i] = crc;
    }
    return table;
}

constinit auto crc_table = make_crc_table();
```

### Compile-Time Factorial

```cpp
template <int N>
consteval int factorial() {
    static_assert(N >= 0);
    int r = 1;
    for (int i = 2; i <= N; ++i) r *= i;
    return r;
}

static_assert(factorial<5>() == 120);
```

### `constinit` in Singletons

```cpp
struct Config { int timeout; int max_retries; };
constinit Config g_config = {30, 3};

void reconfigure(int t, int r) {
    g_config = {t, r};  // OK: mutable at runtime
}
```

## `consteval` Limitations

```cpp
constexpr int helper(int x) { return x + 1; }

consteval int caller(int x) {
    // return helper(x);   // error: helper may evaluate at runtime
    return helper(5);      // OK: argument is constant

// Cannot take address of immediate function
// auto fp = &square;      // error

// Virtual functions cannot be consteval
// struct S { virtual consteval int f(); };  // error
```

## Summary

- **`consteval`** is for functions that must execute at compile time, suitable for validation and lookup tables.
- **`constinit`** enforces compile-time initialization without adding `const`, suitable for preventing SIOF in global variables.
- **`constexpr`** maintains maximum flexibility, usable at both compile time and runtime.
- The three are complementary: `consteval` generates → `constinit` stores → `constexpr` reuses at runtime.
