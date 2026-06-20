---
title: "std::initializer_list"
topic: unknown
feature: initializer-list
standard: N/A
status_checked_at: 2026-06-20
---
# std::initializer_list

## Overview

`std::initializer_list<T>` is a lightweight container adapter introduced in C++11 that allows functions to accept brace-enclosed initialization lists as parameters. It is not an owning container itself — it provides a read-only view over a compiler-provided backing array.

Typical uses include: unifying container construction syntax, implementing variadic constructors, and replacing multiple overloaded functions.

## Syntax

```cpp
#include <initializer_list>

void func(std::initializer_list<int> list);

// In a constructor
class MyVector {
public:
    MyVector(std::initializer_list<int> init);
};
```

## Properties

| Property | Description |
|----------|-------------|
| Element type | `const T`, not modifiable |
| Size | Fixed at compile time, cannot grow or shrink |
| Memory layout | Compiler may place on stack or in read-only memory |
| Lifetime | Backing array lives only for the enclosing statement |
| Operations | `size()`, `begin()`, `end()`, range iteration |

## Code Examples

### Basic Usage

```cpp
#include <initializer_list>
#include <vector>
#include <iostream>

class IntContainer {
    std::vector<int> data_;
public:
    IntContainer(std::initializer_list<int> init) : data_(init) {}

    void print() const {
        for (auto v : data_) std::cout << v << " ";
        std::cout << "\n";
    }
};

int main() {
    IntContainer c = {1, 2, 3, 4, 5};
    c.print();  // 1 2 3 4 5
}
```

### As a Standalone Function Parameter

```cpp
double average(std::initializer_list<double> values) {
    double sum = 0;
    for (auto v : values) sum += v;
    return values.size() > 0 ? sum / values.size() : 0.0;
}

int main() {
    std::cout << average({1.0, 2.0, 3.0, 4.0});  // 2.5
}
```

### Nested initializer_lists

```cpp
#include <vector>
#include <initializer_list>

class Matrix {
    std::vector<std::vector<int>> data_;
public:
    Matrix(std::initializer_list<std::initializer_list<int>> rows) {
        for (auto& row : rows) {
            data_.emplace_back(row);
        }
    }
};

int main() {
    Matrix m = {
        {1, 2, 3},
        {4, 5, 6},
        {7, 8, 9}
    };
}
```

### Constructor Overload Resolution

```cpp
class Widget {
public:
    Widget(int a, int b) { /* two args */ }
    Widget(std::initializer_list<int> list) { /* list */ }
};

Widget w1(1, 2);     // calls Widget(int, int)
Widget w2{1, 2};     // calls initializer_list version
Widget w3 = {1, 2};  // calls initializer_list version (preferred over (int, int))
```

## Pitfalls

**Empty list ambiguity**

```cpp
Widget w1{};     // default constructor (C++11)
Widget w2({});   // initializer_list (empty list)
```

**Cannot resize**

`initializer_list` has no `push_back`, `insert`, or other mutating operations. It is a fixed-size read-only view.

**Lifetime trap**

```cpp
std::initializer_list<int> get_list() {
    return {1, 2, 3};  // dangerous: backing data outlives its scope
}
```

`initializer_list` does not own its data. Returning it may result in a dangling reference to the backing array. While some compilers may copy the data, this is undefined behavior.

**Performance note**

For small element counts, `initializer_list` is typically stack-allocated with no heap overhead. For large lists, implicit copies may occur. In performance-sensitive code, consider alternatives.

## Compiler Support

| Compiler | Minimum Version |
|----------|----------------|
| GCC | 4.4+ |
| Clang | 3.1+ |
| MSVC | 2013+ (VS 12.0) |
