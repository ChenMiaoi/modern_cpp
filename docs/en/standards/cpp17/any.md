---
title: C++17 std::any
topic: cpp17
feature: any
standard: C++17
status_checked_at: 2026-06-01
exercises:
  - exercises/cpp17/any1.cpp
solutions:
  - exercises/solutions/any1.cpp
---
# C++17 std::any

## Overview

`std::any` is a type-safe container introduced in C++17 within `<any>` that can store **a single value of any copy-constructible type**. It tracks the actual type at runtime and reports errors through `std::bad_any_cast` exceptions when types don't match. It is essentially a type-safe replacement for `void*`, suitable for scenarios where types are unpredictable, such as configuration parsing, script bindings, and plugin systems.

## Construction and Assignment

```cpp
#include <any>
#include <string>

std::any a1;                          // default construction: empty
std::any a2{42};                      // value construction
std::any a3{std::string{"hello"}};

a1 = std::string{"world"};            // assignment changes the type
a1 = 100;                              // changes from string to int

std::any a4 = a2;                      // deep copy

// in-place construction
std::any a5{std::in_place_type<std::string>, 5, 'X'}; // "XXXXX"
```

`std::any` requires the stored type to satisfy `CopyConstructible`. Non-copyable types (like `unique_ptr`) cannot be stored directly.

## std::any_cast: Value Access

```cpp
std::any a{42};

// value semantics—throws std::bad_any_cast on type mismatch
int val = std::any_cast<int>(a);       // 42
// double d = std::any_cast<double>(a); // throws exception

// pointer semantics—returns nullptr on mismatch, no exception thrown
int* p = std::any_cast<int>(&a);
if (p) std::cout << *p << "\n";       // 42

double* dp = std::any_cast<double>(&a);
// dp == nullptr
```

`any_cast<T>(&any)` takes `any*` and returns `T*` — this is the recommended way to avoid exceptions.

## State Querying

```cpp
std::any a;

a.has_value();             // false
a = 42;
a.has_value();             // true
a.type() == typeid(int);   // true
a.type().name();           // platform-dependent type name

a.reset();                 // clears the value
a.has_value();             // false
```

## Heterogeneous Containers

```cpp
#include <any>
#include <vector>
#include <iostream>

int main() {
    std::vector<std::any> bag;
    bag.push_back(42);
    bag.push_back(3.14);
    bag.push_back(std::string{"hello"});
    bag.push_back(true);

    for (const auto& item : bag) {
        if (item.type() == typeid(int))
            std::cout << "int: " << std::any_cast<int>(item) << "\n";
        else if (item.type() == typeid(std::string))
            std::cout << "string: " << std::any_cast<std::string>(item) << "\n";
    }
}
```

This is the most typical use case for `std::any`: configuration systems, message passing, JSON intermediate representations.

## Simple Property System Example

```cpp
#include <any>
#include <string>
#include <unordered_map>

class Properties {
    std::unordered_map<std::string, std::any> data_;
public:
    template<typename T>
    void set(const std::string& key, T&& value) {
        data_[key] = std::forward<T>(value);
    }

    template<typename T>
    T get(const std::string& key) const {
        auto it = data_.find(key);
        if (it == data_.end()) throw std::runtime_error("key not found");
        return std::any_cast<T>(it->second);
    }

    template<typename T>
    T get_or(const std::string& key, T default_val) const {
        auto it = data_.find(key);
        if (it == data_.end() || it->second.type() != typeid(T))
            return default_val;
        return std::any_cast<T>(it->second);
    }
};
```

## Comparison with std::variant and void*

| Feature | `std::any` | `std::variant<Ts...>` | `void*` |
|---------|-----------|----------------------|---------|
| Type safety | Runtime check | Compile-time guarantee | None |
| Storable types | Any copyable type | Fixed type list | Any (pointer only) |
| Error reporting | `bad_any_cast` | `bad_variant_access` | None (UB) |
| Size | Pointer + possible heap allocation | Value type + index | One pointer |
| Use case | Unpredictable types | Known type set | Low-level systems programming |

## Small Object Optimization (SBO)

Most implementations apply small object optimization to `std::any` — small objects (typically ≤16~24 bytes) are stored directly inside the `any` without heap allocation; large objects trigger heap allocation.

```cpp
std::any a{42};                          // typically no heap allocation
std::any b{std::vector<int>(10000, 1)};  // heap allocation
```

## Best Practices

- **Use `std::any` only when types are truly unpredictable**. When the type set is known, prefer `std::variant`.
- **Use the pointer form of `any_cast`** (`any_cast<T>(&a)`) to avoid exception overhead.
- **Consider `shared_ptr<T>` for large objects** to reduce copy overhead.
- **Document the expected types of property values** — the type contract lives in the caller's mind, not in the type system.

## Common Pitfalls

```cpp
// Pitfall 1: type mismatch
std::any a{42};
// std::any_cast<long>(a);  // throws bad_any_cast! int != long

// Pitfall 2: storing a pointer vs. storing a value
std::any a1{new int{42}};   // stores int*, use int* with any_cast
std::any a2{42};            // stores int, use int with any_cast
// the two are different—do not confuse them

// Pitfall 3: const qualification
const std::any ca{42};
// std::any_cast<int&>(ca);  // throws! cannot convert to non-const reference
int val = std::any_cast<int>(ca);           // OK: value copy
const int& cr = std::any_cast<const int&>(ca); // OK

// Pitfall 4: non-copyable types
// std::any a = std::make_unique<int>(42);  // compile error
std::any a = std::make_shared<int>(42);     // OK: shared_ptr is copyable

// Pitfall 5: performance-sensitive paths
// any_cast involves runtime typeid comparison
// prefer std::variant on hot paths
```
