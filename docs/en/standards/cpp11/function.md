---
title: "std::function"
topic: unknown
feature: function
standard: N/A
status_checked_at: 2026-06-20
---
# std::function

## Overview

`std::function<R(Args...)>` is a type-erasing wrapper introduced in C++11 that stores any callable object — function pointers, lambdas, `std::bind` expressions, and function objects. It is a core component for callback mechanisms and functional programming patterns.

## Header

```cpp
#include <functional>
```

## Syntax

```cpp
std::function<R(Args...)> f;

// Examples
std::function<int(int, int)> add = [](int a, int b) { return a + b; };
std::function<void()> callback = []() { std::cout << "done\n"; };
```

## What It Can Wrap

| Type | Example |
|------|---------|
| Function pointer | `int(*)(int, int)` |
| Lambda | `[](int x) { return x * 2; }` |
| `std::bind` expression | `std::bind(add, _1, 10)` |
| Function object | Struct with `operator()` |
| Member function pointer | `&Class::method` (requires binding `this`) |

## Code Examples

### Basic Usage

```cpp
#include <functional>
#include <iostream>

void execute(std::function<int(int, int)> op, int a, int b) {
    std::cout << "Result: " << op(a, b) << "\n";
}

int main() {
    execute([](int a, int b) { return a + b; }, 3, 4);  // 7
    execute([](int a, int b) { return a * b; }, 3, 4);  // 12
}
```

### Sorting with Custom Comparator

```cpp
#include <algorithm>
#include <functional>
#include <vector>

int main() {
    std::vector<int> nums = {5, 2, 8, 1, 9, 3};

    std::function<bool(int, int)> desc = [](int a, int b) {
        return a > b;
    };

    std::sort(nums.begin(), nums.end(), desc);
    // nums: {9, 8, 5, 3, 2, 1}
}
```

### Callback Pattern

```cpp
#include <functional>
#include <string>
#include <iostream>

class Button {
    std::function<void()> onClick_;
public:
    void setOnClick(std::function<void()> callback) {
        onClick_ = callback;
    }
    void click() {
        if (onClick_) onClick_();
    }
};

int main() {
    Button btn;
    int count = 0;
    btn.setOnClick([&count]() {
        count++;
        std::cout << "Clicked " << count << " times\n";
    });
    btn.click();  // Clicked 1 times
    btn.click();  // Clicked 2 times
}
```

### Storing Different Types

```cpp
#include <functional>
#include <iostream>

int add(int a, int b) { return a + b; }

struct Multiplier {
    int factor;
    int operator()(int x) const { return x * factor; }
};

int main() {
    std::function<int(int, int)> f1 = add;
    std::function<int(int)> f2 = Multiplier{5};

    std::cout << f1(3, 4) << "\n";  // 7
    std::cout << f2(6) << "\n";     // 30
}
```

## Performance Considerations

| Aspect | Description |
|--------|-------------|
| Heap allocation | Small captures may be inlined; large captures trigger heap allocation |
| Virtual dispatch | Calls go through an internal virtual function — indirect call overhead |
| vs function pointer | Function pointer has zero overhead; `std::function` has wrapping cost |
| vs template | Templates are fully inlined; `std::function` has type-erasure cost |

**Rule of thumb**: Use templates when performance is critical. Use `std::function` when runtime polymorphism or type erasure is needed.

## std::function vs Function Pointer vs Template

| Feature | Function pointer | `std::function` | Template |
|---------|-----------------|-----------------|----------|
| Capture state | No | Yes | Yes |
| Type erasure | No | Yes | No |
| Overhead | Zero | Medium | Zero (inlined) |
| Runtime polymorphism | No | Yes | No |
| Use case | Pure functions | Callbacks/strategies | High-performance generic code |

## Pitfalls

**Empty std::function**

```cpp
std::function<int()> f;
// f();  // undefined behavior, throws std::bad_function_call

if (f) {
    f();  // safe
}
```

**Move semantics**

`std::function` supports move semantics. Moving a large-capture lambda is more efficient than copying:

```cpp
std::function<void()> f = [big_data = std::move(data)]() {
    // use big_data
};
```

**Hidden cost of type erasure**

`std::function` internally stores a pointer to captured data. Even empty-capture lambdas incur indirect call overhead. For simple function pointer scenarios, a raw function pointer is more efficient.

## Compiler Support

| Compiler | Minimum Version |
|----------|----------------|
| GCC | 4.4+ |
| Clang | 3.1+ |
| MSVC | 2012+ (VS 11.0) |
