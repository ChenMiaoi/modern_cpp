---
title: C++26 constexpr Extensions
status_checked_at: 2026-06-01
topic: unknown
feature: constexpr-extensions
standard: N/A
---


# C++26 constexpr Extensions

## Overview

C++26 greatly expands `constexpr`, allowing compile-time use of `std::optional`, `std::vector`, `std::string`, `std::unique_ptr`, and relaxing multiple restrictions. It works deeply in concert with the reflection system.

**Related proposals:** P2738 (constexpr void* cast), P2747 (constexpr placement new), P3147 (constexpr unique_ptr), P3295 (constexpr vector/string).

## constexpr std::optional

```cpp
#include <optional>

consteval int find_sqrt(int key) {
    std::optional<int> result;
    for (int i = 0; i < 100; ++i)
        if (i * i == key) { result = i; break; }
    return result.value_or(-1);
}
static_assert(find_sqrt(64) == 8);
static_assert(find_sqrt(50) == -1);
```

## constexpr std::vector

```cpp
#include <vector>
#include <algorithm>

consteval std::vector<int> sieve(int limit) {
    std::vector<bool> is_prime(limit + 1, true);
    is_prime[0] = is_prime[1] = false;
    for (int i = 2; i * i <= limit; ++i)
        if (is_prime[i])
            for (int j = i * i; j <= limit; j += i)
                is_prime[j] = false;
    std::vector<int> primes;
    for (int i = 2; i <= limit; ++i)
        if (is_prime[i]) primes.push_back(i);
    return primes;
}
static_assert(sieve(100).size() == 25);

consteval std::vector<int> ct_sort() {
    std::vector<int> data{5, 3, 8, 1, 9, 2, 7, 4, 6};
    std::ranges::sort(data);
    auto [f, l] = std::ranges::remove(data, 5);
    data.erase(f, l);
    return data;
}
static_assert(ct_sort().front() == 1 && ct_sort().size() == 8);
```

## constexpr std::string

```cpp
#include <string>
#include <algorithm>

consteval bool is_palindrome(std::string_view s) {
    std::string str(s), rev = str;
    std::ranges::reverse(rev);
    return str == rev;
}
static_assert(is_palindrome("racecar"));
```

## constexpr unique_ptr (P3147)

```cpp
#include <memory>

struct Node {
    int value;
    std::unique_ptr<Node> next;
    constexpr Node(int v) : value(v), next(nullptr) {}
    constexpr ~Node() = default;
};

consteval int list_sum() {
    auto head = std::make_unique<Node>(1);
    head->next = std::make_unique<Node>(2);
    head->next->next = std::make_unique<Node>(3);
    int sum = 0;
    for (Node* p = head.get(); p; p = p->next.get())
        sum += p->value;
    return sum;
}
static_assert(list_sum() == 6);
```

**Limitation:** All allocation/deallocation must occur within the same constant evaluation context.

## Relaxed Restrictions

constexpr placement new and void* casts:

```cpp
#include <new>

struct Widget { int data[4]; };

consteval int placement_demo() {
    alignas(Widget) unsigned char buf[sizeof(Widget)];
    Widget* w = new (buf) Widget{10, 20, 30, 40};
    Widget* wp = static_cast<Widget*>(static_cast<void*>(w));
    int r = wp->data[0] + wp->data[3];
    w->~Widget();
    return r;
}
static_assert(placement_demo() == 50);
```

## Interaction with Reflection

constexpr containers eliminate the compile-time dynamic storage bottleneck for the reflection system:

```cpp
#include <meta>
#include <vector>
#include <string>

consteval std::vector<std::string> get_enum_names() {
    enum class Color { Red, Green, Blue, Yellow, Cyan, Magenta };
    std::vector<std::string> names;
    template for (constexpr auto e : std::meta::enumerators_of(^Color))
        names.emplace_back(std::meta::name_of(e));
    return names;
}
static_assert(get_enum_names().size() == 6);
```

## Implementation Status

| Feature | GCC | Clang | MSVC |
|---------|-----|-------|------|
| constexpr optional | 12+ | 16+ | 19.34+ |
| constexpr vector/string | 14 dev | dev | dev |
| constexpr unique_ptr | dev | dev | dev |
| constexpr placement new | 13+ | 17+ | In progress |

## Summary

C++26 constexpr extensions evolve compile-time programming from restricted arithmetic computation to full general-purpose programming. constexpr versions of vector/string/optional/unique_ptr, combined with relaxed restrictions, enable compile-time code to execute nearly the same logic as runtime, working deeply in concert with the reflection system.
