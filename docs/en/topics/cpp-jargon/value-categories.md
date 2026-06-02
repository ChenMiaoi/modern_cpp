---
title: "Value Categories"
topic: unknown
feature: value-categories
standard: N/A
status_checked_at: 2026-06-02
---
# Value Categories

> "Every expression in C++ has two independent properties: a **type** and a **value category**." — The Standard

## The Five Value Categories

Since C++17, value categories form a hierarchy:

```
            expression
           ┌─────┴─────┐
        glvalue      rvalue
       ┌───┴───┐    ┌───┴───┐
    lvalue   xvalue   prvalue
```

- **lvalue**: An expression that has an address and is addressable. In `int x = 42;`, `x` is an lvalue.
- **prvalue** (pure rvalue): A pure value with no address. The literal `42` and `std::string("hello")` are prvalues.
- **xvalue** (expiring value): A value that is "about to be moved." The result of `std::move(x)` is an xvalue.
- **glvalue** (generalized lvalue) = lvalue + xvalue: Any expression that has identity.
- **rvalue** = prvalue + xvalue: Any expression that can bind to an rvalue reference.

## Practical Impact

```cpp
void f(int&);       // accepts only lvalues
void f(int&&);      // accepts only rvalues (prvalue or xvalue)

int x = 42;
f(x);               // calls f(int&) — x is an lvalue
f(42);              // calls f(int&&) — 42 is a prvalue
f(std::move(x));    // calls f(int&&) — std::move(x) is an xvalue
```

## Materialization (C++17)

C++17 introduced a key concept: **temporary materialization**. When a prvalue needs to be bound to a reference or have its members accessed, it is "materialized" into a temporary object (xvalue).

```cpp
struct S { int x; };
S foo() { return S{42}; }  // foo() is a prvalue

int&& r = foo().x;  // foo() is materialized into a temporary S object, then its member is taken
```

Before C++17, `S s = foo();` could involve two copies (RVO was optional). Since C++17, prvalues do **not** create temporary objects — they construct directly "in place." This is **guaranteed copy elision**.

## Pitfall: `auto` Deduction Loses References

```cpp
std::string s = "hello";
auto&& r1 = s;           // r1 is std::string& (lvalue reference)
auto&& r2 = std::move(s); // r2 is std::string&& (rvalue reference)
auto&& r3 = "hello";     // r3 is const char(&)[6] (array reference)

auto&& r4 = std::string("hi"); // r4 is std::string&& (C++17: prvalue materialization)
```
