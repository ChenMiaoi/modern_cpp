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

C++23 引入 `std::to_underlying`，提供从枚举类（`enum class`）获取底层整数值的安全方式。它消除了使用 `static_cast` 的冗余，使代码更简洁、意图更明确。

## 基本用法

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

    // 旧写法
    int old = static_cast<int>(c);

    // C++23 新写法
    int modern = std::to_underlying(c);

    std::cout << "old: " << old << "\n";     // old: 1
    std::cout << "modern: " << modern << "\n"; // modern: 1
}
```

## 支持的枚举类型

```cpp
#include <utility>
#include <iostream>

enum class SmallEnum : uint8_t { A = 10, B = 20 };
enum class LargeEnum : uint64_t { Max = 0xFFFFFFFFFFFFFFFFULL };
enum class CharEnum : char { X = 'x', Y = 'y' };

int main() {
    // to_underlying 保留底层类型
    uint8_t s = std::to_underlying(SmallEnum::B);      // 20
    uint64_t l = std::to_underlying(LargeEnum::Max);   // 18446744073709551615
    char ch = std::to_underlying(CharEnum::X);          // 'x'

    std::cout << s << " " << l << " " << ch << "\n";
}
```

## 使用场景

### 序列化与持久化

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

### C API 互操作

```cpp
#include <utility>
#include <iostream>

// C 风格的库函数
extern "C" {
    int c_create_handle(int type, int flags);
    int c_send_message(int handle, const char* msg, int priority);
}

enum class HandleType : int { Reader = 1, Writer = 2, Duplex = 3 };
enum class SendFlags : int { None = 0, Sync = 1, Async = 2 };

int create_handle(HandleType type, SendFlags flags) {
    // 安全地将 enum class 转换为 C API 所需的 int
    return c_create_handle(
        std::to_underlying(type),
        std::to_underlying(flags)
    );
}
```

### 位运算

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

### switch 和映射

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

### 与 constexpr 配合

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
    // buf.data 有 256 字节
    // buf.capacity == 256
}
```

## 与 static_cast 对比

```cpp
#include <utility>

enum class Status : int { Idle = 0, Running = 1, Done = 2 };

void compare() {
    Status s = Status::Running;

    // static_cast — 冗长且容易写错类型
    int a = static_cast<int>(s);
    int b = static_cast<std::underlying_type_t<Status>>(s);

    // to_underlying — 简洁、类型安全
    int c = std::to_underlying(s);

    // 所有结果相同：1
}
```

## 适用场景

```
✓ 序列化 enum class 到二进制格式
✓ 调用 C API 需要底层整数值
✓ 对 enum class 进行位运算
✓ 构建 enum 到 string 的映射
✓ 作为模板参数的底层值
✗ 需要反向转换（用 static_cast<EnumType>）
✗ 非 enum class 的传统枚举（直接可用）
```

## 注意事项

- `std::to_underlying` 头文件是 `<utility>`
- 仅适用于 `enum class`（强类型枚举）
- 对传统枚举（`enum`）也可使用，但通常不需要
- 返回类型是 `std::underlying_type_t<E>`，即枚举的底层类型
- 不修改枚举值，仅提取底层整数表示
- `to_underlying` 在编译时即可求值，可用于 constexpr 上下文
- 如果需要反向转换（整数到枚举），仍需使用 `static_cast<EnumType>`
