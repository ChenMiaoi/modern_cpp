---
title: "User-Defined Literals"
topic: unknown
feature: user-defined-literals
standard: N/A
status_checked_at: 2026-06-02
---
# User-Defined Literals

## Overview

C++11 introduced User-Defined Literals (UDLs), allowing developers to define custom operators for literal suffixes. This enables code to express domain concepts with intuitive syntax, such as `42_km`, `3.14_rad`, `"hello"_sv`, etc., greatly improving readability while maintaining type safety.

The core value of UDLs is: elevating the binding of "value + unit/semantics" from runtime to compile time, while letting type conversion happen at compile time rather than runtime, reducing boilerplate code.

## Basic Syntax

User-defined literals are defined via `operator""_suffix`, where `_suffix` is the literal suffix name:

```cpp
// return_type operator"" _suffix(parameter_type)
long double operator"" _km(long double value) {
    return value * 1000.0;  // convert km to meters
}

auto distance = 5.0_km;  // distance == 5000.0
```

> **Naming convention**: C++11 requires user-defined suffixes to begin with an underscore `_` (suffixes without underscores are reserved for the standard library). The C++11 standard reserves several underscore-free prefixes (like `_i`, `_id`, `_if`), but in practice it is recommended to always use the `_` prefix.

## Parameter Forms

`operator""` supports several parameter forms, each suitable for different literal inputs:

### 1. Integer Literal Parameter

```cpp
// unsigned long long form: matches integer literals
unsigned long long operator"" _bin(unsigned long long value) {
    // value is already a decimal number; this just passes it through
    return value;
}

auto flags = 1010_bin;  // flags == 10 (binary 1010)
```

The compiler parses the digits in the literal as `unsigned long long`. This form cannot truly do binary parsing (the conversion is done on the compiler side), but it can be used for semantic tagging.

### 2. Floating-Point Literal Parameter

```cpp
long double operator"" _deg(long double degrees) {
    return degrees * 3.14159265358979323846L / 180.0L;
}

constexpr long double angle = 90.0_deg;  // π/2 radians
```

### 3. Character Literal Parameter

```cpp
// char form: matches single character literals
char operator"" _rot13(char c) {
    if (c >= 'a' && c <= 'm') return c + 13;
    if (c >= 'n' && c <= 'z') return c - 13;
    if (c >= 'A' && c <= 'M') return c + 13;
    if (c >= 'N' && c <= 'Z') return c - 13;
    return c;
}

auto encrypted = 'H'_rot13;  // 'U'
```

### 4. String Literal Parameter

```cpp
// const char* + size_t form: matches string literals
std::string operator"" _upper(const char* str, size_t len) {
    std::string result(str, len);
    for (auto& c : result)
        c = static_cast<char>(std::toupper(c));
    return result;
}

auto greeting = "hello"_upper;  // "HELLO"
```

### 5. Raw String/Number Parameter (Template Form)

```cpp
// Template form: matches the raw character sequence of a literal
template <char... Chars>
constexpr int operator"" _hex() {
    return hex_to_int<Chars...>::value;
}
```

This is the most flexible but also the most complex form. Each character of the literal is passed as a template parameter, requiring recursive template metaprogramming to parse.

## constexpr User-Defined Literals

Declaring UDLs as `constexpr` enables compile-time computation:

```cpp
struct Distance {
    long long meters;
};

constexpr Distance operator"" _km(unsigned long long v) {
    return Distance{static_cast<long long>(v) * 1000};
}

constexpr Distance operator"" _m(unsigned long long v) {
    return Distance{static_cast<long long>(v)};
}

constexpr Distance operator"" _cm(unsigned long long v) {
    return Distance{static_cast<long long>(v) / 100};
}

// Compile-time computation
constexpr auto marathon = 42_km + 195_m;
static_assert(marathon.meters == 42195, "marathon distance");
```

`constexpr` UDLs push all computation to compile time, with zero runtime overhead.

## Real-World Examples

### Example 1: Time Units

```cpp
using namespace std::chrono_literals;

// Standard library provides: operator""s, operator""ms, operator""us, operator""ns
auto timeout = 500ms;
auto duration = 2s + 300ms;
std::this_thread::sleep_for(1s);
```

### Example 2: Binary Literal Helper

```cpp
namespace bin_literals {

// Compile-time binary string parsing
template <char... Chars>
struct bin_parser;

template <char High, char... Rest>
struct bin_parser<High, Rest...> {
    static_assert(High == '0' || High == '1', "Only 0 and 1 allowed");
    static constexpr unsigned long long value =
        ((High - '0') << (1 + sizeof...(Rest) - 1)) +
        bin_parser<Rest...>::value;  // Note: simplified; real impl needs proper shift
};

template <>
struct bin_parser<> {
    static constexpr unsigned long long value = 0;
};

template <char... Chars>
constexpr unsigned long long operator"" _b() {
    return bin_parser<Chars...>::value;
}

} // namespace bin_literals

// Usage: using namespace bin_literals;
// constexpr auto mask = 1010_b;
```

### Example 3: String View Literal (Standard Library `sv`)

```cpp
using namespace std::string_literals;
using namespace std::string_view_literals;

auto str = "hello"s;          // std::string
auto view = "hello"sv;        // std::string_view (C++17)
auto multi = "hello"
             " world"s;        // std::string "hello world"
```

### Example 4: Complex Number Literal (Standard Library `i`)

```cpp
using namespace std::complex_literals;

auto z = 1.0 + 2.0i;          // std::complex<double>
auto pure = 3.0i;              // std::complex<double>(0, 3.0)
auto f = 1.0f + 2.0if;        // std::complex<float>
```

## Standard Library User-Defined Literals

| Header | Suffix | Return Type | Introduced In |
|--------|--------|-------------|---------------|
| `<chrono>` | `h`, `min`, `s`, `ms`, `us`, `ns` | Corresponding `chrono::duration` | C++14 |
| `<string>` | `s` | `std::string` | C++14 |
| `<string_view>` | `sv` | `std::string_view` | C++17 |
| `<complex>` | `i`, `if`, `il` | `std::complex<float/double/long double>` | C++14 |

Note: `operator""s` in C++11 belongs to `<chrono>` and works on `const char*`; it was moved to `<string>` in C++14 with an added `size_t` overload.

## Best Practices

1. **Name suffixes with a leading underscore**: Standard library suffixes have no underscore (like `s`, `sv`), user-defined suffixes use underscores (like `_km`, `_deg`). This avoids naming conflicts.

2. **Declare as `constexpr`**: Try to have UDLs evaluate at compile time for zero runtime overhead.

3. **Provide strong types rather than raw types**:
   ```cpp
   // ❌ Weak: returns double, loses unit information
   double operator"" _km(long double v) { return v * 1000; }

   // ✅ Strong: returns a type-safe distance type
   constexpr Distance operator"" _km(unsigned long long v) {
       return Distance{v * 1000};
   }
   ```

4. **Encapsulate in namespaces**: Avoid global namespace pollution:
   ```cpp
   namespace units {
       constexpr Distance operator"" _km(unsigned long long v) { /*...*/ }
       constexpr Distance operator"" _m(unsigned long long v) { /*...*/ }
   }

   // using namespace units;  // import on demand
   ```

5. **Use only for readability**: If a UDL is not clearer than a regular constructor, don't use it.

## Common Pitfalls

### Pitfall 1: Choosing Between Integer and Floating-Point Parameter Forms

```cpp
// Only accepts floating-point literals
long double operator"" _r(long double v) { return v; }
// 5.0_r ✅   5_r ❌ compilation error

// Only accepts integer literals
unsigned long long operator"" _r(unsigned long long v) { return v; }
// 5_r ✅   5.0_r ❌ compilation error
```

If both types need to be accepted, two overloads must be defined.

### Pitfall 2: String Literal Storage Duration

```cpp
// ⚠️ Dangerous: returns a pointer to a static buffer
const char* operator"" _warn(const char* str, size_t) {
    static char buf[256];
    snprintf(buf, 256, "WARNING: %s", str);
    return buf;  // Not thread-safe, and overwritten on next call
}
```

The `const char*` parameter of a string UDL points to compile-time static storage with valid lifetime; but the return value should use `std::string` or `std::string_view` for management.

### Pitfall 3: `operator""` Conflicts with Reserved Suffixes

```cpp
// ❌ Not allowed: standard-reserved underscore-free suffix
// long double operator"" km(long double v) { return v; }

// ✅ Correct: leading underscore
long double operator"" _km(long double v) { return v; }
```

### Pitfall 4: Compilation Time Overhead of Template Form

```cpp
template <char... Chars>
constexpr int operator"" _as_int() {
    // Recursive template expansion — slow compilation for many characters
    // Compilers limit template recursion depth
    return parse_int<Chars...>::value;
}
```

Using template-form UDLs for long literal strings significantly increases compilation time. Prefer the `const char*, size_t` form with runtime parsing, or use `constexpr` functions.

### Pitfall 5: ADL and `using namespace` Interaction

```cpp
namespace A {
    struct Meter { double v; };
    constexpr Meter operator"" _m(unsigned long long v) { return Meter{static_cast<double>(v)}; }
}

namespace B {
    struct Meter { double v; };
    constexpr Meter operator"" _m(unsigned long long v) { return Meter{static_cast<double>(v)}; }
}

// using namespace A;
// using namespace B;
// auto x = 5_m;  // ❌ ambiguity: two candidates
```

Place UDLs in independent namespaces and import them via `using` on demand to avoid ambiguity.
