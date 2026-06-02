---
title: "C++03"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# C++03

C++03 (ISO/IEC 14882:2003) is a bug-fix release of C++98. It introduced no new language features, but fixed several Defect Reports in the standard text.

## Positioning

C++03 is considered a "revision" of C++98; the differences between the two are virtually invisible in day-to-day programming. The next major change would not arrive until C++11 (originally planned as C++0x).

## Major Corrections

- **Value Initialization**: Clarified the behavior of the `T()` syntax, distinguishing default initialization, value initialization, and zero initialization
- **Corrections to `std::string` and `std::allocator`**
- **Refinement of two-phase name lookup** rules for templates
- **Clarification of the semantics of exception specifications**

## Why It Matters

Although C++03 itself introduced no new features, understanding its corrections ensures you have a correct understanding of the following:

- Why `new T()` and `new T` behave differently for built-in types
- The two-phase name lookup rules during template instantiation
- The precise behavior of `auto_ptr` under C++03 semantics

## Further Reading

- [Changes and Corrections](/standards/cpp03/changes)
