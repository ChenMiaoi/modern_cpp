---
title: range-v3 Actions 与 Sentinels
topic: libraries
feature: actions-sentinels
standard: C++20
status_checked_at: 2026-06-02
exercises: []
solutions: []
---
# range-v3 Actions and Sentinels

## Actions: Eager Operations

Unlike the lazy evaluation of Views, Actions directly modify containers:

```cpp
using namespace ranges;

std::vector<int> v = {3, 1, 4, 1, 5, 9};

// Actions are eager — directly modify v
v |= actions::sort | actions::unique;
// v is now {1, 3, 4, 5, 9}

v |= actions::transform([](int x) { return x * 2; });
// v is now {2, 6, 8, 10, 18}
```

The Actions pipe operator `|=` differs from the Views `|`: `|=` modifies the left operand in-place and returns a reference.

## Sentinel: The Sentinel Concept

Sentinel is a key innovation of range-v3 — `end()` is not necessarily an iterator; it can be any type comparable with an iterator:

```cpp
// Find the end of a null-terminated string
auto sv = ranges::subrange(ptr, nullptr);  // Iterator + nullptr sentinel

// Truncate to maximum length
auto sv2 = ranges::subrange(ptr, ptr + max_len);
```

The value of Sentinel: some sequences have termination conditions based on "value equals sentinel" (such as null-terminated strings) rather than "reached a certain position." The Sentinel concept allows compilers to optimize termination checks — for null-terminated strings, sentinel comparison requires only one pointer dereference and comparison, which is more efficient than the traditional iterator's double-pointer comparison.

```cpp
template <class S, class I>
concept sentinel_for = semiregular<S> && input_or_output_iterator<I> &&
    weakly_equality_comparable_with<S, I>;
```
