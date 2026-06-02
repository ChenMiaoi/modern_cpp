---
title: "Object Model & Memory Terminology"
topic: unknown
feature: object-model
standard: N/A
status_checked_at: 2026-06-02
---
# Object Model & Memory Terminology

## Lifetime

An object's lifetime begins when construction completes and ends when destruction begins. Accessing an object outside its lifetime is **UB**.

```cpp
{
  int* p;
  {
    int x = 42;
    p = &x;
  }  // x's lifetime ends
  // *p is UB! Even though the memory may still exist, the object no longer does
}
```

## Storage Duration

| Storage Duration | Created When | Destroyed When | Example |
|-----------------|-------------|----------------|---------|
| automatic | Entering scope | Leaving scope | Local variables |
| static | Program startup | Program exit | Global variables, `static` local variables |
| dynamic | `new` | `delete` | Heap-allocated objects |
| thread | Thread creation | Thread exit | `thread_local` |

## Dangling Reference

A reference whose referent has been destroyed, but the reference still exists:

```cpp
const std::string& bad() {
  std::string s = "hello";
  return s;  // dangling reference! s is destroyed after the function returns
}

std::string_view bad_view() {
  std::string s = "hello";
  return s;  // dangling string_view! after s is destroyed, the view points to invalid memory
}
```

## Strict Aliasing Rule

Accessing the same memory through pointers of different types is UB (unless specific exceptions apply):

```cpp
float f = 3.14f;
int* p = reinterpret_cast<int*>(&f);
int i = *p;  // UB! Violates strict aliasing

// Correct approach:
std::memcpy(&i, &f, sizeof(i));  // OK, memcpy is allowed
// or use std::bit_cast (C++20)
int i2 = std::bit_cast<int>(f);  // OK
```

**Exception**: `char*`, `unsigned char*`, and `std::byte*` can access the underlying representation of any object.

## Placement New

Constructing an object in already-allocated memory:

```cpp
alignas(int) char buf[sizeof(int)];  // manually allocated memory
int* p = new (buf) int(42);          // construct int in buf
p->~int();                            // must manually destroy
// no need to delete — buf is a stack array
```

## std::launder (C++17)

In some cases, an object's address may become "invalidated" due to a type change. `std::launder` tells the compiler "this pointer does indeed point to a live object":

```cpp
struct X { const int n; };
X* p = new X{42};

// reconstruct X in p's memory (legal because the const member is the same)
p->~X();
X* p2 = new (p) X{43};

// p2 and p point to the same address, but the compiler may have cached the value of p->n
// std::launder tells the compiler not to use the cache
int val = std::launder(p2)->n;  // OK: 43
```

## Object Representation

The actual byte sequence of an object in memory. `sizeof(T)` returns the number of bytes in the object representation. But the object representation may contain **padding** bytes:

```cpp
struct S {
  char a;    // 1 byte
  // 3 bytes padding
  int b;     // 4 bytes
  char c;    // 1 byte
  // 3 bytes padding
};
// sizeof(S) == 12 (not 6! due to alignment requirements)
```
