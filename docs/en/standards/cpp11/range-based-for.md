---
title: "Range-based for"
topic: unknown
feature: range-based-for
standard: N/A
status_checked_at: 2026-06-20
---
# Range-based for

## Overview

Range-based for is a loop syntax introduced in C++11 that simplifies traversal of arrays, containers, and initializer lists. It eliminates explicit iterator and index management, making code shorter and safer.

## Syntax

```cpp
for (auto& x : container) { /* ... */ }
for (const auto& x : container) { /* ... */ }
for (auto x : container) { /* ... */ }
```

## Expansion Mechanism

Range-based for expands at compile time into equivalent iterator code:

```cpp
// What you write
for (auto& x : container) { use(x); }

// What the compiler generates
auto&& __range = container;
auto __begin = std::begin(__range);
auto __end = std::end(__range);
for (; __begin != __end; ++__begin) {
    auto& x = *__begin;
    use(x);
}
```

## Copy vs Reference Semantics

```cpp
std::vector<int> v = {1, 2, 3};

// auto copies each element — wasteful and incorrect
for (auto x : v) { x = 0; }  // modifies the copy, not the original

// auto& references the original — correct and efficient
for (auto& x : v) { x = 0; }  // modifies the original container

// const auto& is a read-only reference — safest for iteration
for (const auto& x : v) { std::cout << x; }
```

## Supported Container Types

Range-based for works with:

| Type | Description |
|------|-------------|
| C-style arrays | `int arr[] = {1, 2, 3};` |
| `std::vector`, `std::list`, etc. | Standard containers |
| `std::map`, `std::set` | Associative containers |
| `std::initializer_list` | Initializer lists |
| Custom types | Must provide `begin()` and `end()` |

## Code Examples

### Iterating Over Associative Containers

```cpp
#include <map>
#include <string>
#include <iostream>

int main() {
    std::map<std::string, int> ages = {
        {"Alice", 30}, {"Bob", 25}, {"Charlie", 35}
    };

    for (const auto& [name, age] : ages) {  // C++17 structured bindings
        std::cout << name << ": " << age << "\n";
    }

    // C++11 compatible
    for (const auto& pair : ages) {
        std::cout << pair.first << ": " << pair.second << "\n";
    }
}
```

### Iterating Over C-style Arrays

```cpp
int arr[] = {10, 20, 30, 40, 50};

for (const auto& x : arr) {
    std::cout << x << " ";
}
// Output: 10 20 30 40 50
```

### Modifying Container Elements

```cpp
#include <vector>
#include <algorithm>

int main() {
    std::vector<int> nums = {1, 2, 3, 4, 5};

    for (auto& x : nums) {
        x *= 2;  // modify in place
    }
    // nums is now {2, 4, 6, 8, 10}
}
```

## Pitfalls

**Modifying the container during iteration**

```cpp
std::vector<int> v = {1, 2, 3, 4, 5};
for (auto& x : v) {
    if (x % 2 == 0) v.push_back(x * 10);  // dangerous: invalidates iterators
}
```

Adding or removing elements inside a range-based for loop typically causes undefined behavior. Use index-based loops or explicit iterators when modifying container size during traversal.

**Custom types must provide `begin()` and `end()`**

```cpp
class Range {
    int start_, end_;
public:
    Range(int s, int e) : start_(s), end_(e) {}
    int* begin() { return &start_; }
    int* end() { return &end_; }
};

for (auto x : Range(1, 10)) {
    // Note: this won't work as expected because raw pointers are stored
    // The iterator type needs operator* and operator!=
}
```

**`auto` copy trap**

For large objects, using `auto` triggers the copy constructor, causing unexpected overhead. Always use `const auto&` for read-only iteration.

## Comparison with Traditional Loops

| Feature | Range-based for | Index loop | Iterator loop |
|---------|----------------|------------|---------------|
| Conciseness | Highest | Medium | Lower |
| Safety | High | Medium (out-of-bounds) | Medium |
| Modify elements | Requires `auto&` | Direct | Via iterator |
| Remove elements | Not supported | Not recommended | Supported |
| Get index | Not direct | Direct | Not direct |

## Compiler Support

| Compiler | Minimum Version |
|----------|----------------|
| GCC | 4.6+ |
| Clang | 3.0+ |
| MSVC | 2012+ (VS 11.0) |
