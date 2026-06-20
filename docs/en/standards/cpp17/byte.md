---
title: C++17 std::byte
topic: unknown
feature: std-byte
standard: N/A
status_checked_at: 2026-06-20
---
# C++17 std::byte

## Overview

`std::byte` is a type introduced in C++17 within `<cstddef>` that represents **byte data in raw memory**. Unlike `unsigned char`, `std::byte` is a distinct enumeration type that does not implicitly convert to integers, clearly separating "byte data" from "small integers" in the type system and preventing accidental arithmetic operations.

## Basic Definition

```cpp
#include <cstddef>

enum class byte : unsigned char {};
```

`std::byte` is essentially a strongly-typed `unsigned char`, but as an enumeration class (`enum class`), it does not implicitly convert to integer types.

## Difference from unsigned char

```cpp
#include <cstddef>
#include <iostream>

int main() {
    unsigned char uc = 42;
    std::byte b{42};

    // unsigned char implicitly converts
    int i1 = uc;     // OK: implicit conversion to int

    // std::byte does not implicitly convert
    // int i2 = b;   // Compile error!

    // Explicit conversion required
    int i2 = static_cast<int>(b);  // OK: 42

    // std::byte does not support arithmetic
    // auto r1 = b + 1;       // Compile error!
    // auto r2 = b * 2;       // Compile error!

    // But bitwise operations are supported
    std::byte mask{0x0F};
    std::byte result = b & mask;  // OK
}
```

## Bitwise Operations

`std::byte` supports all bitwise operators, making it ideal for handling bit flags and masks:

```cpp
#include <cstddef>
#include <iostream>

int main() {
    std::byte a{0b1100'1100};
    std::byte b{0b1010'1010};

    // Supported bitwise operations
    auto and_result = a & b;   // 1000'1000
    auto or_result  = a | b;   // 1110'1110
    auto xor_result = a ^ b;   // 0110'0110
    auto not_result = ~a;      // 0011'0011

    // Bit shifting
    auto shifted = std::byte{0b0000'0001} << 3;  // 0000'1000

    // Output (requires conversion to integer)
    std::cout << static_cast<int>(and_result) << "\n";  // 136
}
```

## std::to_integer: Converting to Integers

```cpp
#include <cstddef>
#include <iostream>

int main() {
    std::byte b{0xAB};

    // Convert to different integer types
    unsigned char uc = std::to_integer<unsigned char>(b);  // 0xAB
    int i = std::to_integer<int>(b);                       // 0xAB
    unsigned int ui = std::to_integer<unsigned int>(b);    // 0xAB

    std::cout << std::hex << std::showbase;
    std::cout << "uc: " << static_cast<int>(uc) << "\n";  // 0xab
    std::cout << "i: " << i << "\n";                       // 0xab
}
```

## Raw Memory Operations

The most common use of `std::byte` is representing raw memory buffers:

```cpp
#include <cstddef>
#include <vector>
#include <iostream>
#include <cstring>

int main() {
    // byte represents raw memory
    std::vector<std::byte> buffer(64);

    // Write data
    int value = 42;
    std::memcpy(buffer.data(), &value, sizeof(value));

    // Read data
    int result;
    std::memcpy(&result, buffer.data(), sizeof(result));
    std::cout << result << "\n";  // 42

    // Inspect byte by byte
    for (size_t i = 0; i < sizeof(int); ++i) {
        std::cout << std::hex << std::setfill('0') << std::setw(2)
                  << static_cast<int>(buffer[i]) << " ";
    }
    std::cout << "\n";  // 2a 00 00 00 (little-endian)
}
```

## Serialization and Networking

```cpp
#include <cstddef>
#include <vector>
#include <cstdint>
#include <cstring>

std::vector<std::byte> serialize(uint32_t value) {
    std::vector<std::byte> buf(sizeof(value));
    std::memcpy(buf.data(), &value, sizeof(value));
    return buf;
}

uint32_t deserialize(const std::vector<std::byte>& buf) {
    uint32_t value;
    std::memcpy(&value, buf.data(), sizeof(value));
    return value;
}

// Use structured bindings to parse protocol headers
struct PacketHeader {
    uint16_t type;
    uint32_t length;
};

PacketHeader parse_header(const std::vector<std::byte>& buf) {
    PacketHeader h;
    std::memcpy(&h, buf.data(), sizeof(h));
    return h;
}
```

## Bit Manipulation

```cpp
#include <cstddef>
#include <iostream>

// Set a specific bit
std::byte set_bit(std::byte b, unsigned pos) {
    return b | (std::byte{1} << pos);
}

// Clear a specific bit
std::byte clear_bit(std::byte b, unsigned pos) {
    return b & ~(std::byte{1} << pos);
}

// Test a specific bit
bool test_bit(std::byte b, unsigned pos) {
    return (b & (std::byte{1} << pos)) != std::byte{0};
}

int main() {
    std::byte flags{0b0000'0000};

    flags = set_bit(flags, 3);    // 0000'1000
    flags = set_bit(flags, 7);    // 1000'1000

    std::cout << std::boolalpha;
    std::cout << "bit 3: " << test_bit(flags, 3) << "\n";  // true
    std::cout << "bit 4: " << test_bit(flags, 4) << "\n";  // false

    flags = clear_bit(flags, 3);  // 1000'0000
    std::cout << "bit 3: " << test_bit(flags, 3) << "\n";  // false
}
```

## Compiler Support

| Compiler | Minimum Version | Notes |
|----------|----------------|-------|
| GCC | 7.0 | Full support |
| Clang | 5.0 | Full support |
| MSVC | 19.11 (VS 2017 15.3) | Full support |

`std::byte` requires C++17 compilation mode. The header is `<cstddef>`.

## Best Practices

- **Use `std::byte` instead of `unsigned char`** for raw memory data to enhance type safety.
- **Don't do arithmetic on `std::byte`** — convert first with `std::to_integer` if numeric computation is needed.
- **Pair with `std::vector<std::byte>`** to manage dynamically-sized memory buffers.
- **Bit operations are its core use case**: bit flags, masks, protocol parsing, etc.
- **Use `memcpy` for serialization**: directly convert between `std::byte` arrays and structs.

## Common Pitfalls

```cpp
// Pitfall 1: cannot implicitly convert to integer
std::byte b{42};
// if (b) { }           // Compile error!
if (b != std::byte{0}) { }  // OK

// Pitfall 2: brace initialization
// std::byte b1 = {42};    // May have issues
std::byte b2{42};           // OK

// Pitfall 3: iterator types
std::vector<std::byte> buf(10);
// buf[0] = 5;          // Compile error!
buf[0] = std::byte{5};  // OK

// Pitfall 4: string literals cannot be directly assigned
// std::byte b = 'A';   // Compile error
std::byte b = std::byte{'A'};  // OK
```
