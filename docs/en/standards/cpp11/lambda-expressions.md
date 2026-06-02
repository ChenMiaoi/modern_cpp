---
title: "Lambda Expressions"
topic: unknown
feature: lambda-expressions
standard: N/A
status_checked_at: 2026-06-02
---
# Lambda Expressions

## Overview

Lambda expressions are anonymous function objects that make closures first-class citizens of the language.

## Syntax

```cpp
[capture](parameters) mutable exception attribute -> return_type { body }
```

All parts are optional (simplest form: `[]() {}`).

## Capture List

| Capture Mode | Syntax | Semantics |
|-------------|--------|-----------|
| By value | `[x]` | Makes a copy |
| By reference | `[&x]` | References the external variable |
| Implicit by value | `[=]` | All used variables captured by value |
| Implicit by reference | `[&]` | All used variables captured by reference |
| Mixed | `[=, &x]` | Default by value, x by reference |
| Mixed | `[&, x]` | Default by reference, x by value |
| this pointer | `[this]` | Captures member variables (C++17 allows `[*this]` for by-value capture) |
| Init capture | `[x = expr]` | Move or arbitrary expression initialization (C++14) |

## Basic Examples

```cpp
// Simplest lambda
auto greet = []() { std::cout << "Hello\n"; };
greet();  // Output: Hello

// With parameters
auto add = [](int a, int b) { return a + b; };
std::cout << add(3, 4);  // Output: 7

// Capturing external variables
int factor = 10;
auto multiply = [factor](int x) { return x * factor; };
std::cout << multiply(5);  // Output: 50
```

## Working with STL Algorithms

The most common use of lambdas is as predicates for STL algorithms:

```cpp
std::vector<int> vec = {3, 1, 4, 1, 5, 9, 2, 6};

// Sorting
std::sort(vec.begin(), vec.end(), [](int a, int b) { return a > b; });

// Finding
auto it = std::find_if(vec.begin(), vec.end(), [](int x) { return x > 5; });

// Counting
auto count = std::count_if(vec.begin(), vec.end(), [](int x) { return x % 2 == 0; });

// Accumulating
int sum = std::accumulate(vec.begin(), vec.end(), 0,
    [](int acc, int x) { return acc + x * x; });

// Remove-erase
vec.erase(std::remove_if(vec.begin(), vec.end(),
    [](int x) { return x < 3; }), vec.end());
```

## Function Return Value

Lambda return types are usually automatically deduced. Use a trailing return type when explicit specification is needed:

```cpp
auto divide = [](double a, double b) -> double {
    if (b == 0.0) return 0.0;
    return a / b;
};
```

## Storing Lambdas

A lambda's type is anonymous; typically use `auto` or `std::function` to store it:

```cpp
// auto (recommended, zero overhead)
auto fn = [](int x) { return x * 2; };

// std::function (type erasure, has overhead)
std::function<int(int)> fn2 = [](int x) { return x * 2; };
```

## Mutable Lambda

Variables captured by value are `const` by default. `mutable` allows modification:

```cpp
int counter = 0;
auto increment = [counter]() mutable { return ++counter; };
// counter is unaffected; the lambda's internal copy changes
std::cout << increment();  // 1
std::cout << increment();  // 2
std::cout << counter;      // 0
```

## Recursive Lambda

Implementing a recursive lambda in C++11 requires `std::function`:

```cpp
std::function<int(int)> factorial = [&factorial](int n) {
    return n <= 1 ? 1 : n * factorial(n - 1);
};
```

In C++14, this can be done more elegantly using init captures.

## Lifetime Pitfall

```cpp
// Dangerous! Reference-captured variable goes out of scope
std::function<int()> make_counter() {
    int count = 0;
    return [&count]() { return ++count; };  // Dangling reference!
}

// Correct: capture by value
std::function<int()> make_counter() {
    int count = 0;
    return [count]() mutable { return ++count; };
}
```

## Best Practices

- Prefer `[=]` or `[&]` for short captures, then adjust as needed
- Prefer capture by value (avoids lifetime issues), use reference capture only when necessary
- Keep short lambdas on one line; extract long lambdas as named functions
- Use `auto` to store lambdas; use `std::function` only when type erasure is needed
