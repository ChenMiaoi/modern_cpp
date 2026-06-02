---
title: "static_assert"
topic: unknown
feature: static-assert
standard: N/A
status_checked_at: 2026-06-02
---
# static_assert

## Overview

`static_assert` is a compile-time assertion mechanism introduced in C++11, allowing developers to check conditions at compile time. When the condition is `false`, the compiler produces a compilation error containing a specified message. Unlike runtime `assert`, `static_assert` executes at compile time, has zero runtime overhead, and can catch design errors that only manifest under specific template instantiations or platform configurations.

`static_assert` is most commonly used for: type trait checks, struct size/alignment verification, template parameter constraints, enum value range validation, and as a complementary means to SFINAE for early diagnostics.

## Basic Syntax

```cpp
static_assert(constant-expression, string-literal);
```

- **constant-expression**: Must be a constant expression evaluable at compile time, typically involving `sizeof`, `alignof`, type traits, template parameters, `constexpr` functions, etc.
- **string-literal**: Error message displayed when the assertion fails (required in C++11; optional from C++17)

```cpp
static_assert(sizeof(int) == 4, "int must be 4 bytes");
static_assert(sizeof(void*) == 8, "64-bit pointer required");
```

## C++17 Message-less Version

C++17 relaxed the syntax requirement, allowing the message string to be omitted:

```cpp
// C++11: message required
static_assert(sizeof(int) == 4, "int must be 4 bytes");

// C++17: message optional
static_assert(sizeof(int) == 4);
```

When omitted, some compilers still output readable diagnostic information (such as the expression text).

## Working with Type Traits

`static_assert` combined with `<type_traits>` is the most common compile-time constraint mechanism:

```cpp
#include <type_traits>

// Constraining template parameter to arithmetic types
template <typename T>
T safe_add(T a, T b) {
    static_assert(std::is_arithmetic<T>::value,
                  "safe_add requires arithmetic types");
    return a + b;
}

// Constraining to non-const, move-constructible types
template <typename T>
void process(T&& value) {
    static_assert(!std::is_const<std::remove_reference_t<T>>::value,
                  "process does not accept const values");
    static_assert(std::is_move_constructible<T>::value,
                  "T must be move-constructible");
    // ...
}
```

### Common Type Trait Assertions

```cpp
// Pointer type check
static_assert(std::is_pointer<int*>::value, "must be a pointer");

// Inheritance check
static_assert(std::is_base_of<Base, Derived>::value,
              "Derived must inherit from Base");

// Trivially copyable (ensures memcpy safety)
template <typename T>
void binary_copy(T* dst, const T* src, size_t n) {
    static_assert(std::is_trivially_copyable<T>::value,
                  "binary_copy requires trivially copyable types");
    std::memcpy(dst, src, n * sizeof(T));
}

// Nothrow destructible
static_assert(std::is_nothrow_destructible<Widget>::value,
              "Widget must have a non-throwing destructor");
```

## Static Assertions in Templates

In templates, `static_assert` is only evaluated when the template is instantiated — uninstantiated templates do not trigger the assertion. This makes it a precise tool for template constraints:

```cpp
template <typename T, size_t N>
class FixedVector {
    static_assert(N > 0, "FixedVector size must be positive");
    static_assert(N <= 1024, "FixedVector size exceeds maximum (1024)");
    static_assert(std::is_default_constructible<T>::value,
                  "T must be default-constructible for FixedVector");

    T data_[N];
    size_t size_ = 0;

public:
    void push_back(const T& value) {
        // ...
    }
};

// FixedVector<int, 0>    → compilation error: "size must be positive"
// FixedVector<int, 2048> → compilation error: "exceeds maximum"
```

Unlike SFINAE, `static_assert` provides clear error messages but **hard-blocks** compilation. SFINAE makes overload candidates "silently drop out," while `static_assert` explicitly declares "this is illegal usage."

### static_assert vs SFINAE

```cpp
// SFINAE style: overload disappears, compiler may give vague "no matching function" error
template <typename T,
          typename = std::enable_if_t<std::is_arithmetic<T>::value>>
T add(T a, T b) { return a + b; }

// static_assert style: clear error message
template <typename T>
T multiply(T a, T b) {
    static_assert(std::is_arithmetic<T>::value,
                  "multiply only works with arithmetic types");
    return a * b;
}
```

Practical advice: **Use SFINAE for function overload resolution** (when other candidates are available), **use `static_assert` for "never allowed" constraints** (illegal means error).

## Struct Size and Alignment Assertions

Ensuring data layout matches external ABI or serialization protocols:

```cpp
struct PacketHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;
    uint32_t length;
};

// Ensure no padding causes layout changes
static_assert(sizeof(PacketHeader) == 12,
              "PacketHeader must be exactly 12 bytes (no padding)");

// Ensure alignment meets hardware DMA requirements
static_assert(alignof(PacketHeader) >= 4,
              "PacketHeader must be at least 4-byte aligned");
```

Alignment used with `alignas`:

```cpp
struct alignas(64) CacheLine {
    char data[64];
};

static_assert(sizeof(CacheLine) == 64, "must fit one cache line");
static_assert(alignof(CacheLine) == 64, "must be cache-line aligned");
```

## Enum Value Validation

Compile-time verification of enum value ranges and properties:

```cpp
enum class Priority : int {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

// Ensure underlying type is int
static_assert(sizeof(Priority) == sizeof(int),
              "Priority must have int underlying type");

// Validate enum value range (using C++17's std::to_underlying or direct cast)
static_assert(static_cast<int>(Priority::Critical) == 3,
              "Priority::Critical must equal 3");

// Ensure enum values cover a contiguous 0..N-1 range (for array indexing)
static_assert(static_cast<int>(Priority::Low) == 0, "Low must be 0");
static_assert(static_cast<int>(Priority::Normal) == 1, "Normal must be 1");
static_assert(static_cast<int>(Priority::High) == 2, "High must be 2");
static_assert(static_cast<int>(Priority::Critical) == 3, "Critical must be 3");
```

## Helper Techniques

### Splitting Multi-Condition Assertions

Assert each independent condition separately, ensuring each has its own error message:

```cpp
template <typename T>
class Container {
    static_assert(std::is_object<T>::value,
                  "Container requires object types (not references/void)");
    static_assert(!std::is_abstract<T>::value,
                  "Container does not support abstract types");
    static_assert(std::is_destructible<T>::value,
                  "T must be destructible");
};
```

### Compile-Time Computation Verification

```cpp
constexpr size_t fibonacci(size_t n) {
    return (n <= 1) ? n : fibonacci(n - 1) + fibonacci(n - 2);
}

static_assert(fibonacci(0) == 0, "fib(0) == 0");
static_assert(fibonacci(1) == 1, "fib(1) == 1");
static_assert(fibonacci(10) == 55, "fib(10) == 55");

// Verify compile-time mathematical constants
constexpr double PI_APPROX = 3.14159265358979;
// Note: floating-point static_assert needs to tolerate precision differences
// static_assert(PI_APPROX == 3.14159265358979, "");  // OK, same literal
```

### Conditional Messages (C++11 Compatible)

```cpp
// C++11 doesn't support message-less version; use a macro for convenience
#define STATIC_CHECK(cond) static_assert(cond, #cond)

STATIC_CHECK(sizeof(int) == 4);
// Compilation error message: "sizeof(int) == 4"
```

## Best Practices

1. **Attach a diagnostic message to every assertion**: The message should explain the **reason** for the constraint, not merely restate the condition.
   ```cpp
   // ❌ Message carries no information
   static_assert(sizeof(T) > 0, "sizeof(T) > 0");

   // ✅ Explains why
   static_assert(sizeof(T) > 0,
                 "T must be a complete type for pointer arithmetic");
   ```

2. **Place at the top of class/function scope**: Makes constraints visible before any logic, aiding quick identification of compilation error sources.

3. **Prefer `static_assert` over `#if` + `#error`**: `static_assert` works on conditions invisible at runtime like template parameters and type traits, and does not depend on the preprocessor.

4. **Combine with `constexpr` functions**: Encapsulate complex compile-time logic into `constexpr` functions, then verify results with `static_assert`.

5. **Do not replace runtime checks**: `static_assert` is only for conditions known at compile time. User input, dynamic configuration, etc. still require runtime assertions.

## Common Pitfalls

### Pitfall 1: Non-Compile-Time Constant Expressions

```cpp
int x = 42;
// ❌ Compilation error: x is not a constant expression
// static_assert(x == 42, "x must be 42");

const int y = 42;
static_assert(y == 42, "y must be 42");  // ✅
```

The condition of `static_assert` must be a constant expression. A `const` but non-`constexpr` variable is not a constant expression.

### Pitfall 2: Incomplete Types

```cpp
class Forward;

// ❌ Compilation error: Forward is an incomplete type, sizeof unavailable
// static_assert(sizeof(Forward) > 0, "");

// Must be after Forward's complete definition:
class Forward { int x; };
static_assert(sizeof(Forward) >= sizeof(int), "");
```

### Pitfall 3: `static_assert` in Function Scope for Non-Instantiated Templates

```cpp
template <typename T>
void foo() {
    static_assert(sizeof(T) > 4, "T must be larger than 4 bytes");
}

// If foo<char>() is never called, this assertion never triggers
// This is not necessarily a problem, but understand its "lazy" nature
```

### Pitfall 4: Floating-Point Comparison Precision

```cpp
// ⚠️ Floating-point constant expression comparison may fail due to precision issues
constexpr double a = 1.0 / 3.0;
constexpr double b = 0.3333333333333333;

// This depends on the compiler's constexpr floating-point precision handling
// static_assert(a == b, "");  // may succeed or fail
```

Floating-point `static_assert` should avoid direct equality comparison; use integer comparison or tolerance instead.

### Pitfall 5: Macro Expansion in Messages

```cpp
#define MSG "check failed"

// ✅ Macro expands to string literal
static_assert(sizeof(int) == 4, MSG);

// ❌ Cannot use non-literal as message
// const char* msg = "oops";
// static_assert(sizeof(int) == 4, msg);
```

Messages must be string literals, not variables or runtime strings.
