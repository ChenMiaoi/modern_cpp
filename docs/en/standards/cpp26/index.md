---
title: C++26
topic: cpp26
feature: overview
standard: C++26
status_checked_at: 2026-06-02
---

# C++26

> **Status note**: This article is compiled from publicly available materials as of 2026-06-02. The C++26 standardization process, proposal acceptance status, and compiler support change rapidly. Verify against WG21 drafts, cppreference compiler support, and compiler release notes before merging.

## Features Already in the Working Draft

### Contracts

Language-level support for preconditions, postconditions, and assertions:

```cpp
int sqrt(int n)
    pre (n >= 0)
    post (r: r * r <= n)
{
    // ...
}
```

### Reflection

Compile-time reflection capabilities — querying type information, member lists, enum values, etc., at compile time. A paradigm shift for C++ metaprogramming. (P2996R7 has been accepted)

### Pattern Matching

The `inspect` expression, similar to Rust's `match`, especially useful for types like variant and optional.

### `std::simd`

Standardized SIMD types and operations for writing portable vectorized code. (P1928)

### Senders/Receivers

A standardized asynchronous execution framework, more flexible and controllable than `std::async`. (P2300)

### `constexpr` Extensions

More standard library functions available at compile time.

## Feature Status Classification

| Status | Meaning | Example |
|--------|---------|---------|
| In working draft | Accepted for C++26 | Reflection (P2996), Contracts |
| Voted in | Passed WG21 vote, awaiting merge | Senders/Receivers (P2300) |
| Still evolving | Proposal still under revision | P3394 (annotations), P3436 |
| Deferred to C++29 | Did not meet C++26 timeline | Some pattern matching details |
| Experimental only | Compiler experimental branch support | Some constexpr extensions |

## Compiler Support Status

| Compiler | Reflection | Contracts | std::simd | Notes |
|----------|-----------|-----------|-----------|-------|
| Clang/LLVM | Experimental (`-freflection`) | In progress | N/A | Reference implementation |
| GCC | Experimental branch | In progress | `<experimental/simd>` | GCC 14+ |
| MSVC | In progress | In progress | N/A | — |

## Status Note

C++26 features are still evolving. This document tracks the latest developments, but content may lag behind the standard committee's latest decisions.

Reference sources:

- [C++ Reference](https://en.cppreference.com/)
- [ISO C++](https://isocpp.org/)
- [WG21 Papers](https://open-std.org/jtc1/sc22/wg21/docs/papers/)

## Further Reading

- [Contracts](/standards/cpp26/contracts)
- [Reflection](/standards/cpp26/reflection)
- [Pattern Matching](/standards/cpp26/pattern-matching)
