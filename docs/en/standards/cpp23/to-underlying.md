---
title: std::to_underlying
topic: cpp23
feature: to-underlying
standard: C++23
status_checked_at: 2026-06-20
standard_refs:
  - draft: N4950
    clause: "[utility.underlying]"
proposals:
  - paper: P1682
    revision: R3
    status: accepted
exercises: []
solutions: []
---
# std::to_underlying

C++23 introduces `std::to_underlying`, providing a safe way to obtain the underlying integer value from an enumeration class (`enum class`). It eliminates the redundancy of using `static_cast`, making code more concise and the intent clearer.

## Basic Usage

```cpp
#include <utility>
#include <iostream>

enum class Color : int {
    Red = 0,
    Green = 1,
    Blue = 2,
    Yellow = 3
};

int main() {
    Color c = Color::Green;

    // Old pattern
    int old = static_cast<int>(c);

    // C++23 new pattern
    int modern = std::to_underlying(c);

    std::cout << "old: " << old << "\n";     // old: 1
    std::cout << "modern: " << modern << "\n"; // modern: 1
}
```

## Supported Enumeration Types

```cpp
#include <utility>
#include <iostream>

enum class SmallEnum : uint8_t { A = 10, B = 20 };
enum class LargeEnum : uint64_t { Max = 0xFFFFFFFFFFFFFFFFULL };
enum class CharEnum : char { X = 'x', Y = 'y' };

int main() {
    // to_underlying preserves the underlying type
    uint8_t s = std::to_underlying(SmallEnum::B);      // 20
    uint64_t l = std::to_underlying(LargeEnum::Max);   // 18446744073709551615
    char ch = std::to_underlying(CharEnum::X);          // 'x'

    std::cout << s << " " << l << " " << ch << "\n";
}
```

## Use Cases

### Serialization and Persistence

```cpp
#include <utility>
#include <vector>
#include <cstdint>

enum class PacketType : uint8_t {
    Connect = 0x01,
    Disconnect = 0x02,
    Data = 0x03,
    Ack = 0x04
};

enum class Priority : uint8_t {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

struct Packet {
    PacketType type;
    Priority priority;
    std::vector<uint8_t> payload;
};

std::vector<uint8_t> serialize(const Packet& p) {
    std::vector<uint8_t> buffer;
    buffer.push_back(std::to_underlying(p.type));
    buffer.push_back(std::to_underlying(p.priority));
    buffer.insert(buffer.end(), p.payload.begin(), p.payload.end());
    return buffer;
}
```

### C API Interoperability

```cpp
#include <utility>
#include <iostream>

// C-style library functions
extern "C" {
    int c_create_handle(int type, int flags);
    int c_send_message(int handle, const char* msg, int priority);
}

enum class HandleType : int { Reader = 1, Writer = 2, Duplex = 3 };
enum class SendFlags : int { None = 0, Sync = 1, Async = 2 };

int create_handle(HandleType type, SendFlags flags) {
    // Safely convert enum class to int for C API
    return c_create_handle(
        std::to_underlying(type),
        std::to_underlying(flags)
    );
}
```

### Bitwise Operations

```cpp
#include <utility>
#include <iostream>

enum class Permission : uint8_t {
    Read    = 0b001,
    Write   = 0b010,
    Execute = 0b100
};

Permission operator|(Permission a, Permission b) {
    return static_cast<Permission>(
        std::to_underlying(a) | std::to_underlying(b)
    );
}

bool has_permission(Permission combined, Permission check) {
    return (std::to_underlying(combined) & std::to_underlying(check)) != 0;
}

int main() {
    Permission perms = Permission::Read | Permission::Write;
    std::cout << has_permission(perms, Permission::Read) << "\n";    // 1
    std::cout << has_permission(perms, Permission::Execute) << "\n"; // 0
}
```

### Switch and Mapping

```cpp
#include <utility>
#include <string>
#include <unordered_map>

enum class ErrorCode {
    Ok = 0,
    NotFound = 1,
    PermissionDenied = 2,
    Timeout = 3
};

std::string error_to_string(ErrorCode code) {
    static const std::unordered_map<int, std::string> map = {
        {0, "OK"}, {1, "Not Found"},
        {2, "Permission Denied"}, {3, "Timeout"}
    };

    auto it = map.find(std::to_underlying(code));
    return it != map.end() ? it->second : "Unknown";
}
```

### Constexpr Usage

```cpp
#include <utility>
#include <array>

enum class BufferSize : size_t {
    Small = 64,
    Medium = 256,
    Large = 1024
};

template <BufferSize N>
struct Buffer {
    std::array<char, std::to_underlying(N)> data{};
    static constexpr size_t capacity = std::to_underlying(N);
};

int main() {
    Buffer<BufferSize::Medium> buf;
    // buf.data has 256 bytes
    // buf.capacity == 256
}
```

## Comparison with static_cast

```cpp
#include <utility>

enum class Status : int { Idle = 0, Running = 1, Done = 2 };

void compare() {
    Status s = Status::Running;

    // static_cast — verbose and error-prone type
    int a = static_cast<int>(s);
    int b = static_cast<std::underlying_type_t<Status>>(s);

    // to_underlying — concise and type-safe
    int c = std::to_underlying(s);

    // All results are the same: 1
}
```

## When to Use

```
✓ Serializing enum class to binary format
✓ Calling C APIs that require underlying integer values
✓ Performing bitwise operations on enum class
✓ Building enum-to-string mappings
✓ Using underlying value as template argument
✗ Reverse conversion needed (use static_cast<EnumType>)
✗ Traditional enum (non-class) — direct conversion already works
```

## Notes

- `std::to_underlying` requires the `<utility>` header
- Only applicable to `enum class` (strongly-typed enumerations)
- Can also be used with traditional `enum`, but typically unnecessary
- Return type is `std::underlying_type_t<E>` — the enumeration's underlying type
- Does not modify the enumeration value; only extracts the underlying integer representation
- `to_underlying` is usable in constexpr contexts and can be evaluated at compile time
- For reverse conversion (integer to enumeration), `static_cast<EnumType>` is still needed
