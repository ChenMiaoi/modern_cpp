---
title: "C++14 Binary Literals"
topic: unknown
feature: binary-literals
standard: N/A
status_checked_at: 2026-06-02
---
# C++14 Binary Literals

## Overview

C++14 introduces binary literals with the `0b` prefix, allowing integer constants to be written directly in binary form. This greatly improves readability in bit-operation-heavy code (hardware register configuration, protocol parsing, permission flags). Binary literals can be combined with `constexpr` for compile-time bitwise operations.

## Syntax

```cpp
0bbinary_digits      // int type (at least 32 bits)
0Bbinary_digits      // uppercase form, equivalent
0bbinary_digitsu     // unsigned int
0bbinary_digitsl     // long
0bbinary_digitsll    // long long
0bbinary_digitsull   // unsigned long long
```

Valid digits are `0` and `1`.

## Code Examples

### Basic Usage

```cpp
#include <iostream>

int main() {
    int a = 0b1010;       // 10
    int b = 0B1111'0000;  // 240 — using C++14 digit separator
    unsigned c = 0b11111111u; // 255

    std::cout << a << ' ' << b << ' ' << c << '\n';
    // Output: 10 240 255
}
```

### Permission Flag Definitions

```cpp
// Permission system — each bit represents a permission
constexpr int PERM_READ    = 0b0001;  // bit 0
constexpr int PERM_WRITE   = 0b0010;  // bit 1
constexpr int PERM_EXEC    = 0b0100;  // bit 2
constexpr int PERM_ADMIN   = 0b1000;  // bit 3

constexpr int PERM_ALL     = 0b1111;
constexpr int PERM_NONE    = 0b0000;

// Check permission
constexpr bool has_permission(int user_perm, int check) {
    return (user_perm & check) == check;
}

// Compile-time verification
static_assert(has_permission(PERM_ALL, PERM_READ));
static_assert(has_permission(PERM_ALL, PERM_WRITE));
static_assert(!has_permission(PERM_READ, PERM_WRITE));
```

### Hardware Register Configuration

```cpp
#include <cstdint>

// UART control register bit fields
constexpr uint8_t UART_ENABLE      = 0b0000'0001;  // bit 0: enable
constexpr uint8_t UART_TX_ENABLE   = 0b0000'0010;  // bit 1: transmit enable
constexpr uint8_t UART_RX_ENABLE   = 0b0000'0100;  // bit 2: receive enable
constexpr uint8_t UART_PARITY_EVEN = 0b0000'1000;  // bit 3: even parity
constexpr uint8_t UART_STOP_BITS_2 = 0b0001'0000;  // bit 4: 2 stop bits
constexpr uint8_t UART_DATA_8BIT   = 0b0110'0000;  // bit 6-5: 8-bit data

// Combined configuration
constexpr uint8_t UART_CONFIG_8N1 =
    UART_ENABLE | UART_TX_ENABLE | UART_RX_ENABLE | UART_DATA_8BIT;
// 0b0110'0111 = 0x67

// Configuration function
constexpr uint8_t configure_uart(bool parity_even, bool stop_2) {
    uint8_t cfg = UART_ENABLE | UART_TX_ENABLE | UART_RX_ENABLE | UART_DATA_8BIT;
    if (parity_even) cfg |= UART_PARITY_EVEN;
    if (stop_2)      cfg |= UART_STOP_BITS_2;
    return cfg;
}

static_assert(configure_uart(true, false) == 0b0000'1111);
```

### Network Subnet Masks

```cpp
#include <cstdint>

constexpr uint32_t MASK_24 = 0b11111111'11111111'11111111'00000000; // /24
constexpr uint32_t MASK_16 = 0b11111111'11111111'00000000'00000000; // /16
constexpr uint32_t MASK_8  = 0b11111111'00000000'00000000'00000000; // /8

constexpr bool in_subnet(uint32_t ip, uint32_t subnet, uint32_t mask) {
    return (ip & mask) == (subnet & mask);
}
```

### Bit Manipulation Utilities

```cpp
#include <iostream>
#include <bitset>

// Templatized bit count (compile-time)
template <typename T>
constexpr int popcount(T value) {
    int count = 0;
    while (value) {
        count += (value & 1);
        value >>= 1;
    }
    return count;
}

static_assert(popcount(0b1010) == 2);
static_assert(popcount(0b1111'1111) == 8);

// Print binary representation
void print_binary(unsigned char byte) {
    std::cout << std::bitset<8>(byte) << '\n';
}

int main() {
    print_binary(0b1010'0101);  // Output: 10100101
}
```

## Best Practices

1. **Use binary literals instead of hexadecimal for bit fields**: `0b0010` is more intuitive than `0x2` in bit-flag contexts.
2. **Combine with digit separators for readability**: `0b1111'0000'1010'0101` grouped by bytes is much easier to review than the version without separators.
3. **Prefer `constexpr` for bitwise results**: Compile-time computation eliminates runtime overhead and catches errors at compile time.
4. **Watch type suffixes**: Binary literals default to `int`; for large values, add `u`, `ull`, or other suffixes to avoid sign issues.
5. **Do not overuse binary literals**: For simple values like `0`, `1`, `255`, decimal or hexadecimal is more conventional and does not need to be changed to binary.
6. **Pair with `std::bitset`**: For runtime debug printing, `std::bitset<8>(value)` can directly output a binary string.
