---
title: "std::generator"
topic: unknown
feature: generator
standard: N/A
status_checked_at: 2026-06-02
---
# std::generator

`std::generator` is the standard coroutine generator provided by C++23, used for lazily generating element sequences. It encapsulates C++20 coroutine primitives, offering an out-of-the-box generator type.

## Basic Usage

```cpp
#include <generator>
#include <iostream>
#include <ranges>

std::generator<int> fibonacci() {
    int a = 0, b = 1;
    while (true) {
        co_yield a;
        auto temp = a;
        a = b;
        b = temp + b;
    }
}

int main() {
    for (int fib : fibonacci() | std::views::take(10)) {
        std::cout << fib << " ";
    }
    // 0 1 1 2 3 5 8 13 21 34
}
```

## co_yield and co_return

```cpp
std::generator<int> range(int start, int end) {
    for (int i = start; i < end; ++i) {
        co_yield i;          // Yield a value
    }
    // Implicit co_return
}

std::generator<int> limited() {
    co_yield 1;
    co_yield 2;
    co_yield 3;
    co_return;               // Explicit termination
}
```

## Recursive Generators

Use `std::ranges::elements_of` to recursively yield all elements of another generator:

```cpp
#include <generator>
#include <iostream>

struct Tree {
    int value;
    Tree* left = nullptr;
    Tree* right = nullptr;
};

std::generator<int> inorder(Tree* node) {
    if (!node) co_return;
    co_yield std::ranges::elements_of(inorder(node->left));
    co_yield node->value;
    co_yield std::ranges::elements_of(inorder(node->right));
}

int main() {
    Tree d{4}, e{5}, f{6};
    Tree b{2, &d, &e}, c{3, &f, nullptr};
    Tree a{1, &b, &c};

    for (int v : inorder(&a)) {
        std::cout << v << " ";
    }
    // 4 2 5 1 6 3
}
```

## Comparison with Hand-Written Coroutines

C++20 requires manually implementing `promise_type`, iterators, and other boilerplate of roughly 50–100 lines:

```cpp
// C++20 hand-written (need to implement promise_type, iterator, destruction, move semantics…)
class manual_generator {
public:
    struct promise_type {
        int current_value;
        std::suspend_always yield_value(int v) { current_value = v; return {}; }
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        manual_generator get_return_object() { /* ... */ }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };
    // Iterator implementation, lifetime management...
};

// C++23 — one line
std::generator<int> gen() { co_yield 1; }
```

## Integration with Ranges

`std::generator` satisfies `std::ranges::input_range`, making it directly usable with all range operations:

```cpp
std::generator<int> naturals() {
    for (int i = 0; ; ++i) co_yield i;
}

auto evens = naturals()
    | std::views::filter([](int n) { return n % 2 == 0; })
    | std::views::take(5);
// 0, 2, 4, 6, 8

auto squares = naturals()
    | std::views::take(5)
    | std::views::transform([](int n) { return n * n; });
// 0, 1, 4, 9, 16
```

## Practical Patterns

```cpp
// Combining multiple generators
std::generator<int> concat(std::generator<int> a, std::generator<int> b) {
    co_yield std::ranges::elements_of(std::move(a));
    co_yield std::ranges::elements_of(std::move(b));
}

// Flattening nested structures
std::generator<int> flatten(const std::vector<std::vector<int>>& vecs) {
    for (const auto& vec : vecs)
        for (int v : vec) co_yield v;
}
```

## C++26: std::execution

C++26 plans to introduce `std::execution` (P2300), whose sender/receiver model supports asynchronous generation and consumption of sequences. `std::generator` is a synchronous lazy sequence, while `std::execution` extends to asynchronous concurrent scenarios.

## Caveats

- The iterator is an input iterator (single-pass only)
- Generator objects are not copyable (move-only)
- Coroutine frames are heap-allocated; high-frequency short generators should be mindful of allocation overhead
- Exceptions propagate to the consumer
