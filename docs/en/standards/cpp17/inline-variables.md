---
title: "C++17 Inline Variables"
topic: unknown
feature: inline-variables
standard: N/A
status_checked_at: 2026-06-02
---
# C++17 Inline Variables

## Overview

C++17 introduces `inline` variables, solving the long-standing **One Definition Rule (ODR)** violation problem when defining global variables or static member variables in header files. Before C++17, global variables in headers had to be declared `extern` and defined in a `.cpp` file; otherwise, multiple translation units including the same header would cause duplicate definition errors at link time. `inline` variables allow direct definition in header files, with the linker guaranteeing only a single instance exists.

## Syntax

```cpp
// direct definition in header file (C++17)
inline int global_counter = 0;
inline const std::string app_name = "MyApp";

// in-class static member definition
struct Config {
    inline static int version = 1;        // C++17, no out-of-class definition needed
    static constexpr int max_size = 1024; // constexpr is implicitly inline
};

// namespace scope
namespace engine {
    inline double gravity = 9.80665;
}
```

## The ODR Problem and How Inline Variables Solve It

### The Pre-C++17 Predicament

```cpp
// ===== config.h =====
// Approach 1: extern (requires definition in some .cpp file)
extern int g_buffer_size;

// Approach 2: static (independent copy per translation unit, wasteful and inconsistent)
static int g_buffer_size = 1024;

// Approach 3: anonymous namespace (same problem as approach 2, copies)
namespace { int g_buffer_size = 1024; }
```

The `extern` approach is correct but verbose — requires an extra `.cpp` file; the `static` and anonymous namespace approaches cause each translation unit to have an independent copy, so modifications are not shared across units.

### C++17 Inline Variables

```cpp
// ===== config.h =====
#pragma once
inline int g_buffer_size = 1024; // all translation units share the same instance

// ===== main.cpp =====
#include "config.h"
void foo() { g_buffer_size = 2048; }

// ===== utils.cpp =====
#include "config.h"
void bar() { /* g_buffer_size is already 2048 */ }
```

Rules for `inline` variables:
- Must have the same definition in every translation unit (typically placed in header files)
- The linker selects one definition from multiple definitions, and all references bind to the same object
- The variable itself cannot be `static` (`static inline` is meaningless at namespace scope)

## Inline Static Members

Before C++17, a class's `static` members could only be declared inside the class and defined outside it:

```cpp
// C++11/14 approach
struct Logger {
    static int instance_count;  // declaration
};

// logger.cpp
int Logger::instance_count = 0; // definition, must be in some .cpp file
```

C++17 allows in-class definition:

```cpp
// C++17 approach
struct Logger {
    inline static int instance_count = 0;   // declaration + definition
    inline static std::mutex mtx;           // non-literal types also work
    static constexpr int max_level = 5;     // constexpr is implicitly inline
};

// no out-of-class definition needed, no extra .cpp file needed
```

## constexpr Variables Are Implicitly Inline

Since C++17, `static constexpr` data members implicitly have the `inline` attribute:

```cpp
struct Limits {
    static constexpr int max_retries = 3;     // implicitly inline
    static constexpr double epsilon = 1e-9;   // implicitly inline
};

// no out-of-class definition needed (C++14 still required it)
// C++14: constexpr int Limits::max_retries; // needed for ODR-use
// C++17: no longer needed
```

Note: `constexpr` variables being implicitly `inline` applies only to `static constexpr` members. `constexpr` variables at namespace scope inherently have internal linkage (`static`) and are not affected by this.

## Comparison with Anonymous Namespace Approach

```cpp
// ===== Anonymous Namespace Approach (C++11/14) =====
// globals.h
namespace {
    int buffer_size = 1024; // independent copy per TU
}

// ===== Inline Variable Approach (C++17) =====
// globals.h
inline int buffer_size = 1024; // all TUs share the same instance
```

| Feature | Anonymous Namespace | `inline` Variable |
|---------|-------------------|-------------------|
| Number of instances | One per translation unit | Globally unique |
| Cross-unit sharing | Not shared | Shared |
| Address consistency | Not guaranteed | Guaranteed |
| Linkage | Internal linkage | External linkage |
| Use case | Translation-unit-private data | True global state |

## Best Practices

1. **Prefer `inline` variables for header-only libraries**: use `inline` in header-only libraries to replace `extern` + `.cpp` definitions.
2. **Prefer `inline static` for class static members**: eliminates out-of-class definition boilerplate.
3. **Distinguish `inline` variables from internal linkage**: if a variable is only used in a single translation unit, use an anonymous namespace or `static`, not `inline`.
4. **`inline` variables should have external linkage**: combining `inline` and `static` at namespace scope is meaningless (`static` already restricts to internal linkage).
5. **Initialization order caveat**: the initialization order of `inline` variables across translation units is still undefined (static initialization order fiasco); use the Construct On First Use idiom when needed.

## Common Pitfalls

- **Initialization order is indeterminate**: when multiple `inline` variables depend on each other, initialization order across translation units is undefined, potentially leading to use of uninitialized values.
- **`inline` ≠ compiler inline expansion**: the `inline` semantics for variables are the same as for functions — allows multiple definitions rather than forcing inlining. The compiler does not perform "inline optimization" on variables.
- **Do not use `inline` variables in `.cpp` files**: the purpose of `inline` variables is for headers included in multiple places. Variables defined in a `.cpp` file are already unique; adding `inline` is meaningless.
- **Template static members are implicitly `inline`**: static data members of class templates are already `inline` when implicitly instantiated; no explicit annotation is needed.
- **ODR violations are still possible**: if an `inline` variable has different initialization values across translation units, behavior is undefined. Ensure header file contents are consistent.
