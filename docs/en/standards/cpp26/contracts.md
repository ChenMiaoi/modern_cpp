---
title: C++26 Contracts
status_checked_at: 2026-06-01
topic: unknown
feature: contracts
standard: N/A
---


# C++26 Contracts

## Overview

Contracts are C++26's language-level code correctness infrastructure, allowing preconditions, postconditions, and assertions to be declared in function interfaces and logic, with standardized violation handling and configurable build modes.

**Proposal status:** P2900R14 has been accepted for C++26.

## Syntax

### Preconditions (pre)

```cpp
int safe_divide(int a, int b)
    pre (b != 0)
    pre (a >= 0)
{ return a / b; }
```

### Postconditions (post)

```cpp
int absolute_value(int x)
    post (r: r >= 0)          // r binds to the return value
{ return x < 0 ? -x : x; }
```

### Assertions (contract_assert)

```cpp
void process(std::vector<int>& data) pre (!data.empty()) {
    std::ranges::sort(data);
    contract_assert(std::ranges::is_sorted(data));
}
```

## Violation Handling

```cpp
void my_handler(std::contract_violation const& v) {
    std::cerr << "Contract violation at " << v.source_location().line() << "\n";
    if (v.is_terminating()) std::abort();
}
std::set_contract_violation_handler(my_handler);
```

## Build Modes

| Mode | Behavior | Scenario |
|------|----------|----------|
| **default** | Implementation-defined | General development |
| **ignore** | Not evaluated, zero overhead | Release builds |
| **observe** | Evaluated and handler called, does not terminate | Logging/monitoring |

```bash
g++ -fcontract-mode=observe main.cpp
```

## Comparison with assert/static_assert

| Property | `assert` | `static_assert` | Contracts |
|----------|----------|-----------------|-----------|
| Evaluation time | Runtime | Compile time | Runtime (configurable) |
| Release builds | Removed | Always present | Optional |
| Location | Function body | Any declaration | Function interface + function body |
| Violation handling | `abort` | Compile error | Custom handler |

```cpp
template <typename T>
T clamp(T val, T lo, T hi) {
    static_assert(std::is_arithmetic_v<T>);   // Compile-time
    assert(lo <= hi);                          // Debug only
    contract_assert(lo <= hi);                // Configurable
    return val < lo ? lo : (val > hi ? hi : val);
}
```

## Virtual Function Contracts

```cpp
class Shape {
public:
    virtual double area() const post (r: r >= 0.0);
};

class Circle : public Shape {
    double radius_;
public:
    Circle(double r) : radius_(r) pre (r > 0.0) {}
    double area() const override post (r: r > 0.0)
    { return 3.14159265358979 * radius_ * radius_; }
};
```

Rule: derived class preconditions must not be stronger (only relaxed), postconditions must not be weaker (only strengthened), ensuring the Liskov Substitution Principle.

## Complete Example

```cpp
#include <vector>
#include <contract>

class Stack {
    std::vector<int> data_;
public:
    void push(int val) post: !data_.empty()
    { data_.push_back(val); }

    int pop() pre (!data_.empty()) {
        int val = data_.back();
        data_.pop_back();
        return val;
    }

    bool empty() const { return data_.empty(); }
};

int main() {
    Stack s;
    s.push(42);
    contract_assert(!s.empty());
    int val = s.pop();
    contract_assert(val == 42);
}
```

## Implementation Status

| Compiler | Status |
|----------|--------|
| GCC | In development, partial syntax support |
| Clang | Experimental branch |
| MSVC | No public implementation yet |

## Summary

C++26 Contracts elevate correctness checks from macros and comments into a language feature. Through standardized preconditions/postconditions, customizable violation handlers, and three build modes, contracts provide protection during development and operate at zero or controlled overhead in release builds.
