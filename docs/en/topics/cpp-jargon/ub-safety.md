---
title: "Undefined Behavior & Safety Terminology"
topic: unknown
feature: ub-safety
standard: N/A
status_checked_at: 2026-06-02
---
# Undefined Behavior & Safety Terminology

## Undefined Behavior (UB)

The standard does not define program behavior — the compiler can do anything. UB is not "a crash"; it is "anything can happen."

```cpp
int a[3] = {1, 2, 3};
int x = a[5];  // UB: out-of-bounds access
// might return garbage, might crash, might "work fine" then blow up at the worst moment
// the compiler can even assume UB won't happen and optimize accordingly — eliminating relevant code paths
```

### Why Does UB Exist?

1. **Performance**: No bounds checking, no overflow checking → faster code
2. **Portability**: Different platforms behave differently; the standard does not mandate uniformity
3. **Optimization opportunities**: The compiler can assume UB won't occur → aggressive optimizations

## Implementation-Defined Behavior

The standard requires the behavior to be defined, but the specific behavior is determined by the compiler. The compiler **must document** its behavior.

```cpp
sizeof(int);           // implementation-defined (usually 4)
int i = -1;
unsigned u = i;        // implementation-defined (usually 2^32 - 1)
```

## Unspecified Behavior

The standard specifies the range of allowed behaviors, but does not specify which one occurs. The compiler does not need to document it.

```cpp
int f() { return 1; }
int g() { return 2; }
int x = f() + g();  // the calling order of f() and g() is unspecified
// f() might be called first, or g() might be called first
```

## Common UB List

### Signed Overflow

```cpp
int x = INT_MAX;
x++;  // UB! Signed integer overflow is UB

unsigned y = UINT_MAX;
y++;  // OK! Unsigned integer overflow is well-defined (modular 2^N)
```

### Null Dereference

```cpp
int* p = nullptr;
*p = 42;  // UB
```

### Use-After-Free

```cpp
int* p = new int(42);
delete p;
*p = 0;  // UB: the memory pointed to by p has been freed
```

### Buffer Overflow

```cpp
int arr[10];
arr[10] = 42;  // UB: index out of bounds (valid indices are 0–9)
```

### Dangling Reference

```cpp
int& bad() {
  int x = 42;
  return x;  // returns a reference to a local variable — UB
}
```

### Strict Aliasing Violation

```cpp
float f = 3.14f;
int i = *reinterpret_cast<int*>(&f);  // UB! Violates strict aliasing rule
```

### Use of Uninitialized Variable

```cpp
int x;
int y = x + 1;  // UB: x is uninitialized
```

## Nasal Demons

A classic metaphor for UB — "anything can happen, including demons flying out of your nose." This is a joke from the comp.lang.c era, but it precisely describes the danger of UB.

## Sanitizers

Compiler tools that detect UB at runtime:

```bash
# AddressSanitizer (ASan): detects memory errors
g++ -fsanitize=address main.cpp && ./a.out

# ThreadSanitizer (TSan): detects data races
g++ -fsanitize=thread main.cpp && ./a.out

# UndefinedBehaviorSanitizer (UBSan): detects UB
g++ -fsanitize=undefined main.cpp && ./a.out

# MemorySanitizer (MSan): detects use of uninitialized memory
clang++ -fsanitize=memory main.cpp && ./a.out
```

## Contracts (C++26 Proposal)

Explicit checks in function preconditions/postconditions — turning implicit UB into explicit runtime errors:

```cpp
int divide(int a, int b)
  pre (b != 0)              // precondition: b is not zero
  post (r: r * b == a)      // postcondition: result is correct
{
  return a / b;
}
```

If contracts are violated, the default behavior is `std::terminate` — much safer than UB.
