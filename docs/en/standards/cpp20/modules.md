---
title: "C++20 Modules"
topic: unknown
feature: modules
standard: N/A
status_checked_at: 2026-06-02
---
# C++20 Modules

## Overview

C++20 Modules aim to replace the traditional header + source file separation model. Through explicit interface export and compilation context isolation, they solve the three major problems of `#include`: **macro pollution**, **order dependency**, and **redundant compilation overhead**.

Traditional `#include` is text substitution — the same header file is re-parsed in every translation unit. Modules split compilation units into **interface units** and **implementation units**. Interface information is compiled only once, and users import the compiled BMI (Binary Module Interface) cache via `import`. Modules were finalized in C++20 (P1103R3). As of 2025, MSVC and Clang have production-level support, while GCC still has limitations.

## Basic Syntax: export module and import

```cpp
// math_utils.cppm  (module interface unit)
export module math_utils;          // declare module name

export int add(int a, int b) {     // export marks declarations for export
    return a + b;
}

int internal_helper(int x) {       // no export: invisible outside the module
    return x * 2;
}
```

```cpp
// main.cpp
import math_utils;
import <iostream>;

int main() {
    std::cout << add(1, 2) << '\n';
    // internal_helper(5);  // compile error: not exported
}
```

Core differences between `import` and `#include`:

| Feature | `#include` | `import` |
|---------|-----------|----------|
| Mechanism | Text substitution | Reference compiled BMI |
| Macro propagation | Yes | No |
| Compilation cost | O(N × M) | O(N), only parse BMI |
| Order dependency | Sensitive | Not sensitive |
| Symbol isolation | None | Automatic isolation at module boundary |

## Module Partitions

Large modules can be split into partitions, compiled independently but sharing the module name:

```cpp
// math:arithmetic.cppm
export module math:arithmetic;
export int add(int a, int b) { return a + b; }
export int sub(int a, int b) { return a - b; }
```

```cpp
// math:geometry.cppm
export module math:geometry;
export double circle_area(double r) { return 3.14159265358979 * r * r; }
```

```cpp
// math.cppm — main module interface unit, aggregating partitions
export module math;
export import :arithmetic;   // re-export partition declarations
export import :geometry;
```

Users only need `import math;`. External code cannot directly `import math:arithmetic;` unless the main module explicitly uses `export import`.

## Interface Unit vs Implementation Unit

```cpp
// network.cppm  (interface unit)
export module network;

export class Socket {
public:
    Socket();
    void connect(const char* host, int port);
    void send(const char* data, size_t len);
    ~Socket();
};
```

```cpp
// network.cpp  (implementation unit)
module network;              // no export

Socket::Socket() { /* ... */ }
void Socket::connect(const char* host, int port) { /* ... */ }
void Socket::send(const char* data, size_t len) { /* ... */ }
Socket::~Socket() { /* ... */ }
```

Implementation units use `module <name>;` (without `export`); definitions in them should not be marked `export`.

## Global Module Fragment and Module Purview

When a module needs to call legacy header files, use the Global Module Fragment. From `export module` to the end of the translation unit is the **module purview**:

```cpp
module;                          // Global Module Fragment begins

#include <cstdio>                // belongs to the global module, won't leak to importers
#include <cstdint>

export module file_io;           // module purview begins

export int read_file(const char* path, char* buf, size_t max) {
    FILE* f = std::fopen(path, "rb");  // cstdio symbols available here
    if (!f) return -1;
    auto n = std::fread(buf, 1, max, f);
    std::fclose(f);
    return static_cast<int>(n);
}

export struct Pixel {            // only exported declarations in the purview are accessible externally
    uint8_t r, g, b, a;         // uint8_t comes from cstdint but is not part of this module
};
```

Between `module;` and `export module` is the Global Module Fragment; `#include` content belongs to the global module, not to this module. Do not directly `#include` in the module purview — place it in the Global Module Fragment or use `import <header>;` instead.

## Compilation Speed and BMI Caching

```
Traditional model:  header.h fully parsed in every .cpp → O(N×M)
Modules:            header.cppm → compile → .bmi cache; each .cpp reads BMI → O(N)
```

Large projects can achieve **30%–70% full build reduction**. Incremental builds are even more significant — modifying an implementation unit does not trigger interface recompilation.

## Compiler Support Status (2025)

| Feature | MSVC (19.38+) | Clang (18+) | GCC (14+) |
|---------|:---:|:---:|:---:|
| `export module` / `import` | Full | Full | Full |
| Module partition | Full | Full | Partial |
| Global module fragment | Full | Full | Experimental |
| `import std;` | Full | Partial | Experimental |
| BMI caching / incremental compilation | Full | Full | Limited |

MSVC has the most mature support; Clang is suitable for Linux/macOS; GCC's Modules are still not recommended for production use.

## Migrating from Headers to Modules

**Phase 1 — Start with base libraries**: Convert utility libraries with no external dependencies to modules.

```cpp
export module utils.string;
export std::string trim(const std::string& s);
export std::vector<std::string> split(const std::string& s, char delim);
```

**Phase 2 — Bridging transition**: Import new modules in legacy header files.

```cpp
// compat_header.h
#pragma once
import math_utils;
inline int legacy_add(int a, int b) { return add(a, b); }
```

**Phase 3 — Bottom-up replacement**: Start from leaf nodes, replacing `#include` with `import` layer by layer, verifying after each replacement.

Migration notes: macros cannot cross module boundaries — use `constexpr` instead; build systems must be module-aware (CMake 3.28+ `CXX_SCAN_FOR_MODULES`); BMI cache directories should be added to `.gitignore`.

## Best Practices

1. One module file corresponds to one logical functional unit, not mechanically converting every `.h` to `.cppm`.
2. Prefer `constexpr` / `inline` over macro constants — macros cannot propagate across modules.
3. Use partitions to organize large modules, avoiding single files that are too large and affect incremental compilation.
4. Implementation units should not `export`; keep the interface minimal.
5. Test and production should use the same compiler version — BMI format is not cross-version compatible.

## Common Pitfalls

1. **Header Unit ≠ Module**: `import <vector>;` is a header unit, equivalent to preprocessed `#include`, and does not have macro isolation capability.
2. **No `#include` in purview**: Header file content will be absorbed into the module, causing ODR issues. Use the Global Module Fragment or `import <header>;` instead.
3. **Anonymous namespace behavior differs**: In module implementation units, anonymous namespace symbols are unique only within that translation unit, but behavior is inconsistent across compilers.
4. **BMI is not ABI stable**: Changing compiler versions or modifying interfaces requires recompiling all dependents.
5. **`export import` vs `import`**: `export import :part;` makes a partition externally visible; plain `import :part;` is only available within the module.
