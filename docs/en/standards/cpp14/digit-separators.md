---
title: "C++14 Digit Separators"
topic: unknown
feature: digit-separators
standard: N/A
status_checked_at: 2026-06-02
---
# C++14 Digit Separators

## Overview

C++14 introduces the single quote `'` as a digit separator in numeric literals, allowing grouping markers to be inserted within numbers to improve readability. Separators do not affect the numeric value itself — the compiler ignores them entirely. This solves the poor readability of large numbers in C++11.

## Syntax

```cpp
Integer:     1'000'000
Floating:    3.141'592'653
Hexadecimal: 0xFF'FF'FF'FF
Binary:      0b1111'0000'1010'0101
Octal:       0'777'777
Custom grouping: 100'00  // Legal, but irregular grouping is not recommended
```

Rules:
- The separator `'` can appear between any two digits (including after `0x`/`0b`/`0` prefixes).
- The separator cannot appear at the beginning or end of a number.
- Separators cannot be adjacent (`1''000` is illegal).
- Separators do not affect the value — `1'000` equals `1000`.

## Code Examples

### Improving Readability of Large Numbers

```cpp
#include <cstdint>

// Hard-to-review version
constexpr uint64_t max_memory_b = 17179869184;

// Using thousands separators
constexpr uint64_t max_memory_b2 = 17'179'869'184;  // ~16 GB

// System constants
constexpr int us_population_approx = 331'000'000;
constexpr double speed_of_light_ms = 299'792'458.0;  // m/s
constexpr uint64_t avogadro = 602'214'076'000'000'000'000'000ULL;
```

### Hexadecimal and Binary

```cpp
#include <cstdint>

// 32-bit value for an IPv4 address
constexpr uint32_t localhost = 0x7F'00'00'01;  // 127.0.0.1

// Color values (RGBA)
constexpr uint32_t color_red   = 0xFF'00'00'FF;
constexpr uint32_t color_green = 0x00'FF'00'FF;
constexpr uint32_t color_alpha_50 = 0xFF'FF'FF'80;

// Binary flags
constexpr uint16_t flags = 0b0000'0001'1010'0101;

// Bit field mask
constexpr uint64_t mask = 0xFFFF'FFFF'0000'0000;
```

### Floating-Point Numbers

```cpp
// pi — grouped by three digits
constexpr double pi = 3.141'592'653'589'793;

// Scientific notation
constexpr double planck = 6.626'070'15e-34;   // J·s
constexpr double boltzmann = 1.380'649e-23;    // J/K

// Currency amounts (grouped by thousands)
constexpr long price_cents = 1'299'99;  // $1,299.99 (in cents)

// Precision grouping (by logical significance)
constexpr double fine_structure = 0.007'297'352'5693;
```

### Octal

```cpp
// Unix file permissions
constexpr int perm_all = 0777;
constexpr int perm_owner_only = 0700;
constexpr int perm_read_write = 0644;  // rw-r--r--

// Octal with separators (less common)
constexpr int perm_grouped = 0'7'5'5;  // rwxr-xr-x
```

### Practical Scenario: Lookup Tables

```cpp
#include <cstdint>

// CRC-32 lookup table excerpt
constexpr uint32_t crc32_table[] = {
    0x0000'0000, 0x7707'3096, 0xEE0E'612C, 0x9909'51BA,
    0x076D'C419, 0x706A'F48F, 0xE963'A535, 0x9E64'95A3,
    // ...
};

// Magic number constants
constexpr uint64_t fnv_offset = 0xCBF2'9CE4'8422'2325;
constexpr uint64_t fnv_prime  = 0x0000'0100'0000'01B3;
```

## Best Practices

1. **Group by semantic meaning**: Thousands (`1'000'000`) or bytes (`0xFF'AB'CD'EF`); choose a grouping that matches the business semantics rather than placing separators arbitrarily.
2. **Use separators with binary literals**: Group every 4 or 8 bits; `0b1111'0000` is far more readable than `0b11110000`.
3. **Group hexadecimal by bytes**: `0x7F'00'00'01` is easier to map to network addresses than `0x7F000001`.
4. **Do not overuse**: `1'0` is harder to read than `10`. Only use separators when numbers exceed 5–6 digits.
5. **Group floating-point numbers by precision semantics**: Scientific constants grouped by traditional precision (3 digits), currency amounts by thousands.
6. **Not supported in strings or macros**: Separators apply only to numeric literals; they cannot be used in `"1'000"` strings or in non-literal portions of `#define` replacement text.
