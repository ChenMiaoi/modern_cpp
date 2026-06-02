---
title: "Attributes"
topic: unknown
feature: attributes
standard: N/A
status_checked_at: 2026-06-02
---
# Attributes

## Overview

C++11 introduced the standard attribute syntax `[[attr]]`, providing a unified, portable way to write compiler metadata. Previously this relied on each compiler's private extensions (GCC's `__attribute__((...))`, MSVC's `__declspec(...)`), which were mutually incompatible. C++11 standardized two attributes: `[[noreturn]]` and `[[carries_dependency]]`; C++14 added `[[deprecated]]`; C++17 added `[[nodiscard]]`, `[[maybe_unused]]`, and `[[fallthrough]]` in one go.

## `[[noreturn]]`

Marks a function as **never returning control flow to the caller** — it either terminates the program, throws an exception, or loops infinitely.

```cpp
[[noreturn]] void fatal_error(const char* msg) {
    std::fprintf(stderr, "FATAL: %s\n", msg);
    std::abort(); // never returns
}
```

The compiler uses this information: ① reports an error if a normal return path exists in the function body; ② marks code after the call site as unreachable, eliminating dead branches.

### Typical Scenarios

**Exception-only function:**

```cpp
[[noreturn]] void throw_invalid_arg(const std::string& detail) {
    throw std::invalid_argument(detail);
}
```

**Program-terminating wrapper:**

```cpp
[[noreturn]] void panic(const char* file, int line, const char* msg) {
    std::fprintf(stderr, "PANIC at %s:%d: %s\n", file, line, msg);
    std::abort();
}
#define PANIC(msg) panic(__FILE__, __LINE__, msg)
```

**Infinite-loop-driven thread:**

```cpp
[[noreturn]] void event_loop() {
    while (true) { /* poll events, process queue */ }
}
```

Incorrectly marking (when the function may actually return) causes **undefined behavior** — the compiler assumes subsequent code is unreachable.

## `[[carries_dependency]]`

The background is C++11's `memory_order_consume`: intended to leverage hardware data dependency ordering to avoid unnecessary memory barriers. This attribute allows dependency chains to **propagate across function calls**.

```cpp
extern std::atomic<int*> global_ptr;

// Parameter-level: dependency flows in through 'p'
void use_value(int* [[carries_dependency]] p) {
    int val = *p; // compiler may elide acquire barrier
}

// Return-level: dependency flows out
int* [[carries_dependency]] load_and_chain() {
    return global_ptr.load(std::memory_order_consume);
}
```

In practice, virtually all compilers promote `consume` to `acquire` (preserving dependency chains is extremely difficult), so this attribute has almost no practical effect in the current ecosystem. New code should use `memory_order_acquire` directly.

## Attribute Placement Rules

```cpp
// 1. Before declaration — applies to the declared entity
[[noreturn]] void abort_process();

// 2. After declarator — also valid
void abort_process() [[noreturn]];

// 3. Class/enum declarations
struct [[deprecated("Use NewParser")]] OldParser {};
enum class [[deprecated]] LegacyMode { A, B };

// 4. Multiple attributes stacked
[[noreturn, cold]] void unreachable_path();
```

Attributes **do not affect the type system**. Two function declarations differing only in attributes are the same entity (ODR-equivalent), producing no overload ambiguity.

## Attributes and Templates

Attributes can appear on template declarations, applying to the specialized entity:

```cpp
template <typename T>
[[deprecated("Use process_v2<T>")]]
void process(T value) { /* ... */ }
// process(42); — warning: 'process<int>' is deprecated

// Specialization can independently carry attributes
template <>
[[deprecated("Binary serialization replaced by JSON")]]
void serialize<int>(int val);
```

Attributes on class templates apply to the class itself and do not automatically propagate to members.

## Comparison with Compiler-Private Attributes

**GCC/Clang `__attribute__((...))`:**

```cpp
__attribute__((noreturn)) void die();
__attribute__((format(printf, 1, 2))) void log(const char* fmt, ...);
__attribute__((hot)) void critical_path();
```

**MSVC `__declspec(...)`:**

```cpp
__declspec(noreturn) void die();
__declspec(dllexport) void public_api();
__declspec(align(64)) struct CacheLine { char data[64]; };
```

Scenarios already covered by standard attributes (`noreturn`, `deprecated`) **should no longer use private syntax**. For functionality not covered by the standard (`format`, `visibility`, `dllexport`, etc.), wrap with conditional compilation macros:

```cpp
#if defined(__GNUC__) || defined(__clang__)
#  define HOT_FUNC __attribute__((hot))
#elif defined(_MSC_VER)
#  define HOT_FUNC
#endif
```

## C++14 / C++17 Attributes

**`[[deprecated]]`** (C++14) — marks deprecated APIs, optionally with a migration message:

```cpp
[[deprecated("Use NewEngine::init()")]] void initialize_legacy();
```

**`[[nodiscard]]`** (C++17) — prevents ignoring return values:

```cpp
[[nodiscard]] bool validate(const Config& cfg);
[[nodiscard("error code must be checked")]] ErrorCode open(const char* path);
validate(cfg);  // warning: discarding return value
```

**`[[maybe_unused]]`** (C++17) — suppresses unused warnings:

```cpp
void callback(int event, [[maybe_unused]] void* user_data) { /* ... */ }
```

**`[[fallthrough]]`** (C++17) — explicitly marks switch fallthrough:

```cpp
switch (state) {
    case State::Init:
        setup();
        [[fallthrough]]; // must be on its own statement with semicolon
    case State::Ready:
        run();
        break;
}
```

## Best Practices

1. **`[[noreturn]]` is a contract** — only mark functions that truly never return; incorrect marking causes UB.
2. **Prefer standard attributes** — works across compilers, no macro wrapping needed. For scenarios covered by the standard, deprecate private syntax.
3. **`[[carries_dependency]]` is nearly useless in practice** — understand it, but use `memory_order_acquire` in new code.
4. **Deprecate APIs with migration messages** — `[[deprecated("use new_func()")]]` is far more useful than a bare attribute.
5. **Attributes do not affect ABI** — removing an attribute does not change function signatures or linking.
6. **Attributes can be stacked** — `[[nodiscard, deprecated("use v2")]]` or written on separate lines both work.
7. **The three C++17 attributes should become daily habits** — `[[nodiscard]]` prevents ignoring return values, `[[fallthrough]]` marks intentional fallthrough, `[[maybe_unused]]` eliminates legitimate unused warnings. Combined with `-Wall -Werror`, they catch hidden bugs.
