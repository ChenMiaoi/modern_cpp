---
title: "nullptr — Type-Safe Null Pointer Literal"
topic: unknown
feature: nullptr
standard: N/A
status_checked_at: 2026-06-02
---
# nullptr — Type-Safe Null Pointer Literal

## Overview

Before C++11, there were two ways to represent a null pointer: `0` and `NULL`. Both introduce ambiguity — the compiler cannot syntactically distinguish "integer zero" from "null pointer." C++11 introduced the `nullptr` keyword, of type `std::nullptr_t`, completely eliminating the semantic confusion between null pointers and integers.

## Why nullptr Is Needed

In traditional C++, `NULL` is typically defined as `0`, which serves both integer and pointer roles:

```cpp
void f(int);
void f(char*);

f(0);    // calls f(int) — 0 is an integer literal
f(NULL); // calls f(int) on most compilers — NULL expands to 0
```

The programmer intends to call `f(char*)`, but `NULL` expands to `0`, so the compiler matches `f(int)`. The problem is worse in template scenarios:

```cpp
template <typename T>
void process(T value) { /* ... */ }

process(0);       // T = int — probably not what you want
process(NULL);    // T = int — same problem
process(nullptr); // T = std::nullptr_t — correct
```

Once `T` is deduced as `int`, subsequent pointer-based operations will produce compilation errors or undefined behavior, with extremely cryptic error messages.

## `std::nullptr_t` Type

The type of `nullptr` is `std::nullptr_t` (defined in `<cstddef>`), with these key properties:

1. Implicitly convertible to any pointer type (including function pointers and member pointers)
2. **Not** implicitly convertible to integer types
3. **Not** usable in arithmetic operations

```cpp
#include <cstddef>
#include <type_traits>

static_assert(sizeof(nullptr) == sizeof(void*), "pointer-sized");
static_assert(std::is_same<decltype(nullptr), std::nullptr_t>::value, "");

int*    p1 = nullptr;   // OK
void (*fp)() = nullptr; // OK — function pointer
int S::* mp = nullptr;  // OK — member pointer
// int x = nullptr;     // error: conversion from nullptr_t to non-scalar type
```

## Behavior in Overload Resolution

```cpp
void handle(int value) { /* int overload */ }
void handle(int* ptr)  { /* pointer overload */ }

handle(0);       // calls handle(int)
handle(NULL);    // calls handle(int) on most platforms
handle(nullptr); // calls handle(int*) — unambiguously
```

`nullptr` clearly expresses "null pointer" semantics; the compiler will not mistake it for an integer.

## Interaction with Templates

### Smart Pointer Scenario

```cpp
template <typename T>
class SmartPtr {
public:
    SmartPtr(T* p) : ptr_(p) {}
    bool operator==(std::nullptr_t) const { return ptr_ == nullptr; }
private:
    T* ptr_;
};

SmartPtr<int> sp(nullptr);
if (sp == nullptr) { /* compiles cleanly */ }
```

### Cannot Deduce Pointed-to Type

`nullptr` itself has no pointed-to type and cannot be used directly in scenarios requiring deduction of `T*`:

```cpp
template <typename T>
void takes_ptr(T* p) { /* ... */ }

takes_ptr(nullptr);                  // error: cannot deduce T from nullptr_t
takes_ptr(static_cast<int*>(nullptr)); // OK — explicit
```

## Common Pitfalls

### Pitfall 1: Ambiguity with bool Overloads

```cpp
void f(int*);
void f(bool);

f(nullptr);  // C++11/14: ambiguous — nullptr converts to both int* and bool
```

When this occurs, use an explicit cast or redesign the interface.

### Pitfall 2: Using nullptr in Boolean Contexts

```cpp
int* p = get_pointer();
if (p)           { /* OK — idiomatic */ }
if (p != nullptr) { /* OK — more explicit, preferred in modern C++ */ }
```

Both are correct, but `!= nullptr` makes intent clearer in code review.

### Pitfall 3: C-style API Boundaries

```cpp
extern "C" void c_function(void* p);
c_function(nullptr); // OK — nullptr converts to void*
```

On all major ABIs, `nullptr`'s representation is consistent with C's null pointer, but the standard does not guarantee this.

## Migration Guide from NULL / 0

**Step 1: Globally replace NULL** — Replace all `NULL` instances representing null pointers with `nullptr` (keep those in macro definitions).

**Step 2: Replace literal 0** — `0` is sometimes genuinely integer zero; requires manual judgment:

```cpp
int* p = 0;              // → int* p = nullptr;
if (p == 0) { }          // → if (p == nullptr) { }
return 0;                // in pointer-returning ctx → return nullptr;
```

**Step 3: Compiler warning-assisted migration**:

```bash
clang++ -Wzero-as-null-pointer-constant -std=c++11 source.cpp
g++     -Wzero-as-null-pointer-constant -std=c++11 source.cpp
```

This warning reports at every location where `0`/`NULL` is used as a null pointer, making it a powerful migration tool.

## Best Practices

1. Always use `nullptr` to represent null pointers, never `0` or `NULL`.
2. In template code, `nullptr` is the only correct null pointer representation — it does not pollute type deduction.
3. For function parameters with "optional pointer" semantics, use `nullptr` rather than a default argument of `= 0`.
4. Enable the `-Wzero-as-null-pointer-constant` compiler warning; enforce it in CI.
5. Do not mix `nullptr` with `bool` overloads — if an interface accepts both pointers and booleans, redesign the API.
6. `NULL` is still acceptable in C-style APIs, but the C++ side should immediately convert to `nullptr`.
