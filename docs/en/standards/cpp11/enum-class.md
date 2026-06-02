---
title: "enum class — Scoped Enumerations"
topic: unknown
feature: enum-class
standard: N/A
status_checked_at: 2026-06-02
---
# enum class — Scoped Enumerations

## Overview

C++11 introduced **scoped enumerations**, with the `enum class` syntax, solving three core problems of traditional enumerations: enumerator values leaking into the enclosing scope, implicit conversion to integers, and uncontrollable underlying types. In modern C++, `enum class` should be the default choice for enumerations.

## Problems with Traditional Enumerations

### Name Leakage

```cpp
enum Color { Red, Green, Blue };
enum TrafficLight { Red, Yellow, Green }; // ERROR: Red/Green already defined
```

Unscoped enumerations inject all enumerators into the enclosing namespace; in large projects, conflicts are inevitable.

### Implicit Conversion to Integer

```cpp
enum Direction { Up, Down, Left, Right };
void process(int value);

Direction d = Up;
process(d);           // compiles! d implicitly converts to int (0)
int x = d + 1;        // compiles! meaningless arithmetic

enum Suit { Hearts, Diamonds, Clubs, Spades };
if (Up == Hearts) {}  // compiles — both 0, semantically meaningless
```

The compiler cannot catch such type errors; bugs can only surface at runtime.

### Uncontrollable Underlying Type

```cpp
enum Flags { A = 1, B = 2, C = 4 };
// sizeof(Flags)? Compiler-dependent! Cannot forward declare without knowing size.
```

## `enum class` Basic Syntax

```cpp
enum class Color { Red, Green, Blue };

Color c = Color::Red;           // OK — must qualify with Color::
// Color c = Red;               // ERROR — Red not in enclosing scope
// int n = c;                   // ERROR — no implicit conversion to int
int n = static_cast<int>(c);    // OK — explicit cast
```

## Specifying the Underlying Type

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

The default underlying type is `int`. When specifying the underlying type, enumerator values must be within the representable range of that type.

## Forward Declaration

Traditional enumerations cannot be forward-declared (the compiler doesn't know the size); `enum class` can:

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

## Usage in switch Statements

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

## Working with STL

`enum class` supports built-in relational operators (`<`, `>`, `==`, `!=`, etc.), but does **not** implicitly convert to integers and cannot be directly compared with integers. When used as a key for `std::unordered_map`, a custom hash must be provided:

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

`std::map` uses `operator<`, so no additional adaptation is needed.

## Bitflag Pattern

For bitflags, operators must be manually overloaded:

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

In C++11, a `to_underlying()` helper can reduce repetitive `static_cast` usage (standardized in C++23):

```cpp
template <typename E>
constexpr auto to_underlying(E e) noexcept
    -> typename std::underlying_type<E>::type {
    return static_cast<typename std::underlying_type<E>::type>(e);
}
```

## Migration Guide from Legacy Enumerations

**Step 1: Identify** — `grep -rn '^\s*enum\s' src/ | grep -v 'enum class'`

**Step 2: Prioritize conversion** — Enumerations used as function parameters/return values, those sharing names across multiple enums, and those passed across modules.

**Step 3: Replace incrementally**:

```cpp
// Before
enum Status { OK, Error, Pending };
// After
enum class Status { Ok, Error, Pending };
// Call sites: status == OK → status == Status::Ok
```

**Step 4: Clang-Tidy assistance** — `clang-tidy -checks='-*,modernize-use-enum-class'`

## Best Practices

1. Always use `enum class` in new code, unless there is a clear reason for implicit integer conversion.
2. For cross-module/serialization/storage scenarios, **specify the underlying type** (e.g., `: uint8_t`) to ensure ABI stability.
3. In switch statements, **omit `default`** to let the compiler catch omissions via `-Wswitch`.
4. For bitflag enumerations, **provide `operator|`, `operator&`, `operator~`** next to the enumeration definition.
5. Avoid scattering `static_cast<int>()` — frequent need for integer conversion indicates a design problem; consider a `to_underlying()` helper function.
6. `std::hash` does not support `enum class` — a custom hash must be provided when used as a key for `unordered_map`.
7. Do not use `enum class` as a substitute for boolean parameters — `process(Flag::Enabled)` merely hides semantics in a different way.
