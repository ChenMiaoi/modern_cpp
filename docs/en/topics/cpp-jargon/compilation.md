---
title: "Compilation & Linking Terminology"
topic: unknown
feature: compilation
standard: N/A
status_checked_at: 2026-06-02
---
# Compilation & Linking Terminology

## Translation Unit

The contents of a `.cpp` file after preprocessing (including all expanded `#include` directives). Each translation unit is compiled independently into a `.o`/`.obj` file, then the linker merges them.

## ODR (One Definition Rule)

Throughout the entire program, each entity (function, class, variable) must have exactly one definition. Violating the ODR is **UB** — the compiler may not report an error, but program behavior is unpredictable.

```cpp
// a.cpp
int foo() { return 1; }

// b.cpp
int foo() { return 2; }  // ODR violation! May not produce an error at link time

// Correct approach: inline functions must be defined in a header file, and all definitions must be identical
```

## Linkage

| Linkage | Meaning | Example |
|---------|---------|---------|
| external | Visible throughout the program | Non-static global functions/variables |
| internal | Visible within the current translation unit | `static` global functions/variables, anonymous namespaces |
| no linkage | Local scope | Local variables |

## Static Initialization Order Fiasco

The initialization order of global variables across different translation units is undefined:

```cpp
// a.cpp
std::string global_a = "hello";

// b.cpp
extern std::string global_a;
std::string global_b = global_a;  // dangerous! global_a may not be initialized yet
```

Solution: Use a `static` variable inside a function (Meyers Singleton) — guaranteed to initialize on first call.

## ABI (Application Binary Interface)

The binary-level convention for functions — how arguments are passed, how return values are obtained, and how names are mangled. ABI may be incompatible across different compilers or versions.

libstdc++'s `_GLIBCXX_USE_CXX11_ABI` macro is an example of ABI versioning — it uses `__cxx11` inline namespaces and `abi_tag` attributes to distinguish symbols between old and new ABIs.

## Name Mangling

The compiler encodes C++ function names into unique symbol names:

```cpp
namespace MyLib {
  class Widget {
    void process(int, double);
  };
}
// GCC symbol name: _ZN6MyLib6Widget7processEid
//                    ─┬─  ─┬─  ─┬─  ─┬─ ─┬
//                namespace class method arg types
```

Different compilers have different mangling rules — this is one of the primary reasons for ABI incompatibility.

## PImpl (Pointer to Implementation)

A classic idiom for hiding implementation details and reducing compile dependencies:

```cpp
// widget.h
class Widget {
  struct Impl;              // forward declaration
  std::unique_ptr<Impl> pImpl_;  // pointer to implementation
public:
  Widget();
  ~Widget();
  void do_something();
};

// widget.cpp
struct Widget::Impl {
  // all private members live here
  std::vector<Data> cache_;
  DatabaseConnection db_;
};

Widget::Widget() : pImpl_(std::make_unique<Impl>()) {}
Widget::~Widget() = default;  // must be in .cpp because Impl is an incomplete type
```

**Advantage**: Modifying `Impl` does not require recompiling code that uses `Widget.h`.

## Forward Declaration

Tells the compiler "this type exists" without requiring a complete definition:

```cpp
class Widget;  // forward declaration — can only be used for pointers/references

void process(Widget* w);  // OK: only needs a pointer
void process(Widget w);   // error: requires complete definition
```

## Include Guard vs #pragma once

```cpp
// traditional include guard
#ifndef MY_HEADER_H
#define MY_HEADER_H
// ... header contents ...
#endif

// compiler extension (non-standard but supported by all major compilers)
#pragma once
```

## PCH (Precompiled Header)

Precompiled headers — headers that rarely change (such as `<iostream>`, `<vector>`) are pre-compiled into a binary format to speed up subsequent compilation.

## LTO (Link-Time Optimization)

Link-time optimization — the compiler performs cross-translation-unit optimizations during the linking phase (inlining, dead code elimination, etc.). Gold linker's ThinLTO and GCC's `-flto` are implementations.

## Translation Phases

C++ compilation is divided into 9 phases:

```
1. Character mapping (source characters → basic source character set)
2. Line splicing (\ continuation)
3. Preprocessing (#include, #define expansion)
4. Execution character set mapping
5. String concatenation
6. Tokenization
7. Syntax analysis + semantic analysis
8. Template instantiation
9. Linking
```
