---
title: C++17 std::conjunction / std::disjunction / std::negation
topic: unknown
feature: type-traits-logic
standard: N/A
status_checked_at: 2026-06-20
---
# C++17 Logical Type Trait Composition

## Overview

`std::conjunction`, `std::disjunction`, and `std::negation` are templates introduced in C++17 within `<type_traits>` for **logical composition of type traits**. They are compile-time versions of logical AND (`&&`), OR (`||`), and NOT (`!`), supporting **short-circuit evaluation** — skipping subsequent trait instantiations when the result is already determined, avoiding compile errors and unnecessary template expansion.

## Basic Definitions

```cpp
#include <type_traits>

// conjunction: true when all traits are true (short-circuit AND)
template <class... Bs>
struct conjunction : std::true_type {};

template <class B>
struct conjunction<B> : B {};

template <class B, class... Bs>
struct conjunction<B, Bs...>
    : std::conditional_t<bool(B::value), conjunction<Bs...>, B> {};

// disjunction: true when any trait is true (short-circuit OR)
template <class... Bs>
struct disjunction : std::false_type {};

template <class B>
struct disjunction<B> : B {};

template <class B, class... Bs>
struct disjunction<B, Bs...>
    : std::conditional_t<bool(B::value), B, disjunction<Bs...>> {};

// negation: logical NOT
template <class B>
struct negation : std::bool_constant<!bool(B::value)> {};
```

## Basic Usage

```cpp
#include <type_traits>
#include <iostream>

int main() {
    // conjunction: all traits must be true
    static_assert(std::conjunction<
        std::is_integral<int>,
        std::is_signed<int>,
        std::is_convertible<int, long>
    >::value, "all must be true");

    // disjunction: any trait being true suffices
    static_assert(std::disjunction<
        std::is_integral<int>,
        std::is_floating_point<int>
    >::value, "at least one must be true");

    // negation: logical NOT
    static_assert(std::negation<
        std::is_pointer<int>
    >::value, "int must not be a pointer");

    std::cout << "all assertions passed\n";
}
```

## Short-Circuit Evaluation

This is the key difference from using `&&`/`||` directly:

```cpp
#include <type_traits>

// Short-circuit: when is_integral<int> is true, is_same<int, int*> is not instantiated
// Even if is_same<int, int*> had issues with incomplete types, it's skipped
static_assert(std::conjunction<
    std::is_integral<int>,             // true → continue
    std::is_same<int, int>             // true → result is true
>::value);

// Comparison with &&: both operands are instantiated
// conjunction stops at the first false
static_assert(std::is_integral<int>::value &&
              std::is_same<int, int>::value);  // both instantiated
```

Practical benefit — avoiding errors in SFINAE:

```cpp
#include <type_traits>
#include <iostream>

// Problem: if T is not a class type, &T::value causes a compile error
// conjunction short-circuits to avoid unnecessary instantiation

// Unsafe (no short-circuit guarantee)
// template <typename T>
// using safe_check = std::bool_constant<T::value && std::is_class<T>::value>;
// If T is not a class, T::value is instantiated first, causing an error

// Safe (conjunction short-circuits)
template <typename T>
using safe_check = std::conjunction<
    std::is_class<T>,           // check if class first
    std::bool_constant<T::value> // only check T::value if class
>;

struct Good { static constexpr bool value = true; };
struct Bad { };

int main() {
    static_assert(safe_check<Good>::value);   // OK
    static_assert(!safe_check<int>::value);   // OK, short-circuits past T::value
    static_assert(!safe_check<Bad>::value);   // OK

    std::cout << "all safe checks passed\n";
}
```

## Convenience Alias Templates

C++17 provides handy alias templates:

```cpp
#include <type_traits>
#include <iostream>

int main() {
    // conjunction_v: get bool value directly
    static_assert(std::conjunction_v<
        std::is_integral<int>,
        std::is_signed<int>
    >);

    // disjunction_v
    static_assert(std::disjunction_v<
        std::is_integral<double>,
        std::is_floating_point<double>
    >);

    // negation_v
    static_assert(std::negation_v<
        std::is_pointer<double>
    >);

    std::cout << "alias templates work\n";
}
```

## Practical Use Cases

### SFINAE Constraints

```cpp
#include <type_traits>
#include <iostream>
#include <string>

// Accept only arithmetic types except bool
template <typename T>
std::enable_if_t<std::conjunction_v<
    std::is_arithmetic<T>,
    std::negation<std::is_same<T, bool>>
>, T>
safe_multiply(T a, T b) {
    return a * b;
}

// Accept only move-constructible but not copy-constructible types
template <typename T>
std::enable_if_t<std::conjunction_v<
    std::is_move_constructible<T>,
    std::negation<std::is_copy_constructible<T>>
>, void>
process(T&& val) {
    T moved = std::move(val);
}

int main() {
    std::cout << safe_multiply(3, 4) << "\n";      // 12
    std::cout << safe_multiply(2.5, 4.0) << "\n";  // 10
    // safe_multiply(true, false);  // Compile error: bool excluded
}
```

### Template Specialization

```cpp
#include <type_traits>
#include <iostream>
#include <vector>
#include <list>

// Optimized at() for random-access iterator containers
template <typename Container>
typename std::enable_if_t<
    std::conjunction_v<
        std::is_same<typename Container::iterator_category,
                     std::random_access_iterator_tag>,
        std::negation<std::is_const<Container>>
    >,
    typename Container::reference
> unsafe_at(Container& c, size_t i) {
    // Random access: O(1)
    return c[i];
}

template <typename Container>
typename std::enable_if_t<
    std::negation_v<
        std::is_same<typename Container::iterator_category,
                     std::random_access_iterator_tag>
    >,
    typename Container::reference
> unsafe_at(Container& c, size_t i) {
    // Non-random access: O(n)
    auto it = c.begin();
    std::advance(it, i);
    return *it;
}

int main() {
    std::vector<int> v = {10, 20, 30};
    std::cout << unsafe_at(v, 1) << "\n";  // 20

    std::list<int> l = {100, 200, 300};
    std::cout << unsafe_at(l, 2) << "\n";  // 300
}
```

### Concept Simulation (Pre-C++20)

```cpp
#include <type_traits>
#include <iostream>
#include <string>
#include <sstream>

// Simulate C++20 concept: Printable
template <typename T>
using is_printable = std::conjunction<
    std::is_object<T>,
    std::negation<std::is_pointer<T>>,
    std::negation<std::is_array<T>>
>;

template <typename T>
std::enable_if_t<is_printable<T>::value>
smart_print(const T& val) {
    std::cout << val << "\n";
}

// Specialization for pointers
template <typename T>
std::enable_if_t<std::is_pointer_v<T>>
smart_print(T ptr) {
    if (ptr) {
        std::cout << *ptr << "\n";
    } else {
        std::cout << "(null)\n";
    }
}

int main() {
    smart_print(42);           // 42
    smart_print("hello");      // hello
    int x = 100;
    smart_print(&x);           // 100
}
```

## conjunction vs &&

| Feature | `std::conjunction` | `&&` |
|---------|-------------------|------|
| Evaluation time | Compile-time | Compile-time |
| Short-circuit | Supported (skips subsequent instantiation) | Not guaranteed (all may be instantiated) |
| Error handling | Short-circuit avoids SFINAE errors | May trigger unexpected compile errors |
| Readability | Clearer in template metaprogramming | More intuitive in simple cases |
| Return type | `std::bool_constant` | `bool` |

## Compiler Support

| Compiler | Minimum Version | Notes |
|----------|----------------|-------|
| GCC | 5.0 | Full support |
| Clang | 3.5 | Full support |
| MSVC | 19.0 (VS 2015) | Full support |

**Note**: `conjunction`/`disjunction`/`negation` were available as extensions in many compilers since C++11 and were standardized in C++17. All modern compilers fully support them.

## Best Practices

- **Prefer in SFINAE constraints**: Use instead of `&&`/`||`/`!` to avoid compile errors from inconsistent short-circuiting.
- **Use `_v` suffix**: `conjunction_v<Bs...>` is cleaner than `conjunction<Bs...>::value`.
- **Compose multiple type traits**: Create complex type constraints for precise template specialization control.
- **Pre-Concepts constraint tool**: Before C++20, this was the primary mechanism for simulating concepts.

## Common Pitfalls

```cpp
// Pitfall 1: conjunction short-circuits but && may not
// conjunction stops at the first false
// && may instantiate all operands (depends on compiler optimization)

// Pitfall 2: type trait's value member
// conjunction template parameters must be types inheriting from true_type/false_type
// Cannot use bool directly
// std::conjunction<std::true_type, true>  // Compile error!

// Pitfall 3: empty parameter list
static_assert(std::conjunction<>::value);   // true (default inherits true_type)
static_assert(!std::disjunction<>::value);  // false (default inherits false_type)

// Pitfall 4: comparison with fold expressions
// C++17 fold expressions can also do logical composition but don't guarantee short-circuit
// template <typename... Bs>
// using conjunction_fold = std::bool_constant<(Bs::value && ...)>;
// This does NOT guarantee short-circuit!
```
