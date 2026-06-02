---
title: import std
topic: cpp23
feature: import_std
standard: C++23
status_checked_at: 2026-06-02
standard_refs:
  - draft: N4950
    clause: "[std.modules]"
proposals:
  - paper: P2465
    revision: R3
    status: accepted
exercises: []
solutions: []
---
# import std

C++23 introduces `import std;`, allowing the entire C++ standard library to be imported in a single statement. It is a major milestone in standard library modularization, significantly reducing compilation time and simplifying project configuration.

## Basic Usage

```cpp
import std;

int main() {
    std::vector<int> v = {3, 1, 4, 1, 5};
    std::ranges::sort(v);

    for (auto x : v | std::views::reverse) {
        std::println("{}", x);
    }
}
```

This is equivalent to no longer needing `#include <vector>`, `#include <algorithm>`, `#include <ranges>`, `#include <print>`, and dozens of other header files.

## import std.compat

`import std.compat;` provides everything `import std;` does, plus global namespace names for the C standard library:

```cpp
import std.compat;

int main() {
    printf("hello %d\n", 42);   // ::printf directly available
    auto p = malloc(100);       // ::malloc
    free(p);

    std::vector<int> v = {1, 2, 3};  // C++ library in std::
}
```

Difference:
- `import std;` — C functions are in the `std::` namespace
- `import std.compat;` — C functions are in both `::` and `std::`

## Compilation Speed Advantage

```cpp
// Traditional: each translation unit re-parses headers
#include <iostream>     // ~30K lines of preprocessor output
#include <vector>       // ~25K lines
#include <algorithm>    // ~20K lines
// 100 source files = millions of lines re-parsed

// Module approach: compiler loads precompiled module interfaces
import std;  // Compile once, reuse many times
```

Measured compilation time improvements (order-of-magnitude reference):
- Small projects (< 10 files): 20–40% reduction
- Medium projects (50–200 files): 40–60% reduction
- Large projects (> 500 files): 50–70%+ reduction

## Compiler Support Status

| Compiler | Version | Status | Notes |
|----------|---------|--------|-------|
| MSVC | 17.5+ | Basic support | `/std:c++latest` |
| GCC | 14+ | Experimental | Requires manual standard library module build |
| Clang | 18+ | Experimental | libc++ support in progress |

### CMake Configuration

```cmake
cmake_minimum_required(VERSION 3.30)
project(myproject LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
add_executable(app main.cpp)
target_sources(app PRIVATE FILE_SET CXX_MODULES FILES main.cpp)
```

## Comparison with #include

| Property | `#include` | `import std;` |
|----------|-----------|---------------|
| Macro isolation | No (macro leakage) | Yes (does not export macros) |
| Compilation speed | Slow (re-parsing) | Fast (precompiled modules) |
| Order dependency | Yes | No |
| Symbol visibility | All exported | Per-module rules |

## Migration Strategy

```cpp
// Incremental migration
// New files: directly use import std;
import std;
#include "my_header.h"  // User headers still use include

// Existing files: replace gradually
// Before
#include <iostream>
#include <vector>
#include <algorithm>
#include <memory>
// After
import std;
```

## Macro Caveats

`import std;` does not export standard library macros; include them separately if needed:

```cpp
#include <cassert>  // assert macro requires a separate include
#include <cerrno>   // errno macro
import std;         // Rest of the standard library
```

## Module Interface Unit

Behind `import std;` is a compiler-provided precompiled standard library module interface, conceptually similar to:

```cpp
// Simplified internal module interface
export module std;
export import std.core;
export import std.algorithm;
export import std.ranges;
export import std.io;
// ...
```

Users do not need to worry about these files; the compiler and build system handle them automatically.

## Caveats

- Not all compilers have fully implemented it; validate in production environments
- Macros (`assert`, `errno`, `NDEBUG`) are not exported through modules
- Build systems must support C++23 modules for correct compilation
- Still in the early adoption phase; recommended for small-scale trial use in new projects
