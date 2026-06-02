---
title: "Exception Safety"
topic: unknown
feature: exception-safety
standard: N/A
status_checked_at: 2026-06-02
---
# Exception Safety

## Three Levels of Guarantee

### Nothrow Guarantee

The operation guarantees it will not throw exceptions. `noexcept` functions, destructors, and swap typically provide this guarantee.

### Basic Guarantee

If the operation throws an exception, the program is in a **valid but unspecified state** — no resource leaks, but the object's value may have changed.

### Strong Guarantee

If the operation throws an exception, the program state **rolls back to before the operation** — as if the operation never happened (transactional semantics).

```cpp
// typical implementation of the strong guarantee: operate on a copy, then swap
void Widget::update(const Data& d) {
  Widget copy(*this);       // copy first
  copy.data_ = d;           // operate on the copy (if exception is thrown, original is unchanged)
  swap(copy);               // noexcept swap
}
```

## RAII (Resource Acquisition Is Initialization)

The cornerstone of C++ — resources are acquired in the constructor and released in the destructor:

```cpp
{
  std::lock_guard<std::mutex> lk(mtx);    // lock acquired at construction
  // ... operate on shared data ...
}  // automatically unlocked at destruction — even if an exception occurs

{
  auto file = std::unique_ptr<File>(open("data.bin"));  // file opened at construction
  // ... use the file ...
}  // automatically closed at destruction
```

RAII makes exception safety natural — as long as resources are managed by RAII objects, the destructor is guaranteed to be called during stack unwinding.

## Stack Unwinding

When an exception is thrown, the runtime walks up the call stack looking for a matching `catch` block. As it passes through each function, local variables in that function are destroyed in reverse order of construction:

```
main()
  → foo()
    → bar()
      → throw std::runtime_error("oops");
    ← bar()'s local variables destroyed
  ← foo()'s local variables destroyed
  → catch (const std::exception& e) { ... }
```

## Scope Guard

An idiom that ensures code executes when a scope exits (whether or not an exception occurs):

```cpp
// simple C++11 implementation
auto guard = finally([]{ cleanup(); });

// C++20 can achieve a similar effect with jthread's destructor
```

## History of Exception Specifications

```
C++98:  void f() throw(std::runtime_error);  // dynamic exception specification (deprecated)
C++11:  void f() noexcept;                    // noexcept specifier
C++17:  dynamic exception specifications removed
```

**The duality of noexcept**: It is both part of the function signature (participates in overload resolution) and an optimization hint for the compiler. Not marking move constructors with `noexcept` causes the standard library to fall back to copying.
