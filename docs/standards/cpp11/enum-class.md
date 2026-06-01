# 枚举类（enum class）— 作用域枚举

## 概述

C++11 引入了**作用域枚举**（scoped enum），语法为 `enum class`，解决了传统枚举的三大核心问题：枚举值泄露到外层作用域、隐式转换为整数、底层类型不可控。在现代 C++ 中，`enum class` 应当是枚举的默认选择。

## 传统枚举的问题

### 名称泄露

```cpp
enum Color { Red, Green, Blue };
enum TrafficLight { Red, Yellow, Green }; // ERROR: Red/Green already defined
```

未限定作用域的枚举将所有枚举项注入外层命名空间，大型项目中冲突不可避免。

### 隐式转换为整数

```cpp
enum Direction { Up, Down, Left, Right };
void process(int value);

Direction d = Up;
process(d);           // compiles! d implicitly converts to int (0)
int x = d + 1;        // compiles! meaningless arithmetic

enum Suit { Hearts, Diamonds, Clubs, Spades };
if (Up == Hearts) {}  // compiles — both 0, semantically meaningless
```

编译器无法捕获此类类型错误，bug 只能在运行时暴露。

### 底层类型不可控

```cpp
enum Flags { A = 1, B = 2, C = 4 };
// sizeof(Flags)? Compiler-dependent! Cannot forward declare without knowing size.
```

## `enum class` 基本语法

```cpp
enum class Color { Red, Green, Blue };

Color c = Color::Red;           // OK — must qualify with Color::
// Color c = Red;               // ERROR — Red not in enclosing scope
// int n = c;                   // ERROR — no implicit conversion to int
int n = static_cast<int>(c);    // OK — explicit cast
```

## 指定底层类型

```cpp
// Controls size and binary layout
enum class FilePermission : uint8_t {
    None    = 0,
    Read    = 1,
    Write   = 2,
    Execute = 4
};
static_assert(sizeof(FilePermission) == 1, "packed into one byte");

enum class PacketType : uint16_t {
    Heartbeat = 0x0001,
    Data      = 0x0002,
    Ack       = 0x0003
};
```

默认底层类型是 `int`。指定底层类型时，枚举值必须在该类型的表示范围内。

## 前向声明

传统枚举无法前向声明（编译器不知道大小），`enum class` 可以：

```cpp
// header.h — forward declaration (must specify underlying type)
enum class MeshFormat : uint32_t;

class Renderer {
public:
    void load(MeshFormat format);
};

// source.cpp — full definition
enum class MeshFormat : uint32_t {
    OBJ  = 0x4F424A00,
    FBX  = 0x46425800,
    GLTF = 0x474C5446
};
```

## 在 switch 语句中使用

```cpp
enum class Weekday { Mon, Tue, Wed, Thu, Fri, Sat, Sun };

const char* to_string(Weekday day) {
    switch (day) {
        case Weekday::Mon: return "Monday";
        case Weekday::Tue: return "Tuesday";
        case Weekday::Wed: return "Wednesday";
        case Weekday::Thu: return "Thursday";
        case Weekday::Fri: return "Friday";
        case Weekday::Sat: return "Saturday";
        case Weekday::Sun: return "Sunday";
    } // Omit default — -Wswitch warns on missing cases when enum grows
}
```

## 与 STL 的配合

`enum class` 支持内置关系运算符（`<`, `>`, `==`, `!=` 等），但**不**隐式转换为整数，也不能直接与整数比较。用作 `std::unordered_map` 键时需提供自定义哈希：
```cpp
#include <unordered_map>
#include <functional>

enum class LogLevel { Debug, Info, Warning, Error, Fatal };

struct LogLevelHash {
    std::size_t operator()(LogLevel level) const noexcept {
        return std::hash<int>()(static_cast<int>(level));
    }
};

std::unordered_map<LogLevel, std::string, LogLevelHash> prefixes = {
    { LogLevel::Debug,   "[DBG] " },
    { LogLevel::Info,    "[INF] " },
    { LogLevel::Warning, "[WRN] " },
    { LogLevel::Error,   "[ERR] " }
};
```

`std::map` 使用 `operator<`，无需额外适配。

## 位标志模式

对于位标志，需要手动重载运算符：

```cpp
enum class Access : uint8_t {
    None    = 0,
    Read    = 1 << 0,
    Write   = 1 << 1,
    Execute = 1 << 2
};

inline Access operator|(Access a, Access b) {
    return static_cast<Access>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline Access operator&(Access a, Access b) {
    return static_cast<Access>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}
inline Access operator~(Access a) {
    return static_cast<Access>(~static_cast<uint8_t>(a));
}

// Usage
Access perms = Access::Read | Access::Write;
if ((perms & Access::Read) == Access::Read) { /* granted */ }
```

C++11 中可提供 `to_underlying()` 辅助减少重复的 `static_cast`（C++23 标准库自带）：

```cpp
template <typename E>
constexpr auto to_underlying(E e) noexcept
    -> typename std::underlying_type<E>::type {
    return static_cast<typename std::underlying_type<E>::type>(e);
}
```

## 从旧枚举迁移指南

**步骤 1：识别** — `grep -rn '^\s*enum\s' src/ | grep -v 'enum class'`

**步骤 2：优先转换** — 用作函数参数/返回值的、多枚举共享名称的、跨模块传递的枚举。

**步骤 3：逐步替换**：

```cpp
// Before
enum Status { OK, Error, Pending };
// After
enum class Status { Ok, Error, Pending };
// Call sites: status == OK → status == Status::Ok
```

**步骤 4：Clang-Tidy 辅助** — `clang-tidy -checks='-*,modernize-use-enum-class'`

## 最佳实践

1. 新代码一律使用 `enum class`，除非有明确理由需要隐式整数转换。
2. 跨模块/序列化/存储场景**指定底层类型**（如 `: uint8_t`），确保 ABI 稳定。
3. switch 中**省略 `default`**，让编译器通过 `-Wswitch` 警告捕获遗漏。
4. 位标志枚举**提供 `operator|`、`operator&`、`operator~`**，放在枚举定义旁边。
5. 避免 `static_cast<int>()` 散落各处——频繁需要整数转换说明设计有问题，考虑 `to_underlying()` 辅助函数。
6. `std::hash` 不支持 `enum class`——用作 `unordered_map` 键时必须提供自定义哈希。
7. 不要用 `enum class` 替代布尔参数——`process(Flag::Enabled)` 只是换了种方式隐藏语义。
