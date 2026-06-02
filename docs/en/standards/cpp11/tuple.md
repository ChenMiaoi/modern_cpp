---
title: "C++11 std::tuple"
topic: unknown
feature: tuple
standard: N/A
status_checked_at: 2026-06-02
---
# C++11 std::tuple

## Overview

`std::tuple` is a fixed-size heterogeneous container introduced in C++11, defined in `<tuple>`. It aggregates an arbitrary number of values of arbitrary types into a single object, similar to a struct but without named fields. Tuples are especially important in generic programming — standard library interfaces in many places (`std::thread`, `std::promise`, unordered container return values) rely on them to pack multiple values.

Compared to `std::pair` (only two elements), tuples support an arbitrary number. Compared to hand-written structs, tuples require no additional type declaration and are suitable for temporary aggregation and generic contexts.

## Core API

### Creating Tuples

```cpp
#include <tuple>
#include <string>

// Direct construction
std::tuple<int, std::string, double> t1(42, "hello", 3.14);

// make_tuple — automatic type deduction (decays references and cv-qualifiers)
auto t2 = std::make_tuple(42, "hello", 3.14);
// type is tuple<int, const char*, double>

// C++14 and later supports get by type (type must be unique)
std::cout << std::get<std::string>(t1) << "\n";
```

### `std::get` — Accessing Elements

```cpp
auto t = std::make_tuple(1, std::string("hello"), 3.14);

int n = std::get<0>(t);           // 1 (by index, compile-time constant)
std::string s = std::get<1>(t);   // "hello"
double d = std::get<double>(t);   // 3.14 (C++14, by type)

// get returns a reference, usable for modification
std::get<0>(t) = 100;

// Move semantics version
auto moved = std::get<1>(std::move(t));
// the string in t is in a valid but unspecified state
```

### `tie` — Unpacking Tuples

```cpp
#include <tuple>

std::tuple<int, std::string, double> getData() {
    return {1, "hello", 3.14};
}

// C++11/14 unpacking method
int x;
std::string y;
double z;
std::tie(x, y, z) = getData();

// std::ignore ignores unwanted elements
int id;
std::tie(id, std::ignore, std::ignore) = getData();

// tie for lexicographic comparison — very practical
auto t1 = std::make_tuple(1, "alpha");
auto t2 = std::make_tuple(1, "beta");
if (t1 < t2) {  // compare first element, then second if equal
    std::cout << "t1 < t2\n";
}
```

### `forward_as_tuple` — Forwarding Tuple

```cpp
#include <tuple>

// forward_as_tuple preserves the reference category (lvalue/rvalue) of arguments
// Primarily used for perfect forwarding scenarios

template<typename... Args>
void wrapper(Args&&... args) {
    auto t = std::forward_as_tuple(std::forward<Args>(args)...);
    // Elements in t preserve original value category: lvalue reference or rvalue reference
}

// Practical use case: foundation for emplace series function implementations
std::vector<std::pair<int, std::string>> vec;
int key = 42;
std::string value = "hello";
// forward_as_tuple forwards value as an lvalue reference, avoiding copies
```

### `tuple_cat` — Concatenating Tuples

```cpp
#include <tuple>

auto t1 = std::make_tuple(1, 2);
auto t2 = std::make_tuple(3.0, "hello");
auto t3 = std::make_tuple(std::string("world"));

// Concatenate multiple tuples
auto combined = std::tuple_cat(t1, t2, t3);
// tuple<int, int, double, const char*, std::string>

static_assert(std::tuple_size<decltype(combined)>::value == 5, "");

// Practical: prepend an element to a tuple
auto with_prefix = std::tuple_cat(std::make_tuple(0), t1);
// (0, 1, 2)
```

### `tuple_size` and `tuple_element` — Type Queries

```cpp
#include <tuple>
#include <type_traits>

using MyTuple = std::tuple<int, std::string, double>;

static_assert(std::tuple_size<MyTuple>::value == 3, "");

// Get the type of the Nth element (compile-time)
using E0 = std::tuple_element<0, MyTuple>::type;  // int
using E1 = std::tuple_element<1, MyTuple>::type;  // std::string

// Preserves const qualifier for const tuple
using CE1 = std::tuple_element<1, const MyTuple>::type;  // const std::string

static_assert(std::is_same<E0, int>::value, "");
static_assert(std::is_same<CE1, const std::string>::value, "");
```

### Comparison Operators

Tuple comparison is **element-wise lexicographic**; all six operators (`==`, `!=`, `<`, `>`, `<=`, `>=`) are available:

```cpp
auto a = std::make_tuple(1, 2, 3);
auto b = std::make_tuple(1, 2, 4);
std::cout << (a < b) << "\n";  // 1 (third element 3 < 4)

// Naturally suited as composite sort keys
std::vector<std::tuple<int, std::string>> students = {
    {90, "Alice"}, {85, "Bob"}, {90, "Charlie"}, {85, "Aaron"}
};
std::sort(students.begin(), students.end(),
    [](const auto& a, const auto& b) {
        return std::make_tuple(-std::get<0>(a), std::get<1>(a))
             < std::make_tuple(-std::get<0>(b), std::get<1>(b));
    });
// Result: (90, Alice), (90, Charlie), (85, Aaron), (85, Bob)
```

## As a Function Return Type

The most common use of tuples is returning multiple values from a function:

```cpp
#include <tuple>
#include <string>

std::tuple<bool, double, std::string> divide(double a, double b) {
    if (b == 0.0) return {false, 0.0, "division by zero"};
    return {true, a / b, "ok"};
}

int main() {
    // C++11/14
    bool ok; double val; std::string error;
    std::tie(ok, val, error) = divide(10.0, 0.0);
    // ok=false, val=0.0, error="division by zero"
}
```

## C++17 Structured Bindings (Mentioned)

C++17 greatly simplifies tuple usage:

```cpp
// C++17 — direct unpacking
auto [id, name, value] = std::make_tuple(42, "hello", 3.14);

// Used in for loops
std::map<std::string, int> scores = {{"Alice", 95}, {"Bob", 87}};
for (const auto& [name, score] : scores) {
    std::cout << name << ": " << score << "\n";
}
```

## Practical Patterns

### Unpacking a Tuple with index_sequence

```cpp
#include <tuple>
#include <utility>
#include <iostream>

template<typename Tuple, typename Func, std::size_t... Is>
void for_each_impl(Tuple&& t, Func&& f, std::index_sequence<Is...>) {
    using swallow = int[];
    (void)swallow{0, (void(f(std::get<Is>(std::forward<Tuple>(t)))), 0)...};
}

template<typename Tuple, typename Func>
void for_each(Tuple&& t, Func&& f) {
    constexpr std::size_t N =
        std::tuple_size<typename std::decay<Tuple>::type>::value;
    for_each_impl(std::forward<Tuple>(t), std::forward<Func>(f),
                  std::make_index_sequence<N>{});
}

auto t = std::make_tuple(1, "hello", 3.14);
for_each(t, [](const auto& elem) {
    std::cout << elem << "\n";  // 1, hello, 3.14
});
```

### Implementing `operator<` with `tie`

```cpp
struct Record {
    std::string name;
    int age;
    int id;

    bool operator<(const Record& rhs) const {
        return std::tie(name, age, id)
             < std::tie(rhs.name, rhs.age, rhs.id);
    }
};
```

## Best Practices

1. **`auto` + `make_tuple` simplifies declarations**: Avoids verbose type signatures.
2. **`tie` + `ignore` for selective unpacking**: Use `std::ignore` when only some return values are needed.
3. **Use `tie` to implement `operator<`**: Avoids boilerplate code for hand-written nested comparisons.
4. **`forward_as_tuple` only for forwarding contexts**: It holds references; do not extend lifetime beyond the referenced objects.
5. **C++17 projects should prefer structured bindings**: Far more readable than `get<0>` calls, with no performance loss.

## Common Pitfalls

- **`make_tuple` decays types**: Removes references and cv-qualifiers. `std::string s; auto t = make_tuple(s);` holds a copy. Use `std::ref(s)` or `std::tie(s)` to preserve references.
- **`get` index out of range is a compilation error**: `N` must be less than the tuple size. Compile-time checked, but error messages can be hard to read.
- **`get<Type>` type must be unique** (C++14): Calling `std::get<int>` on `tuple<int, int>` fails to compile.
- **Move semantics with get**: `std::get<0>(std::move(t))` leaves that element in a valid but unspecified state after the move.
- **`forward_as_tuple` dangling reference**:
  ```cpp
  auto dangling() {
      std::string s = "temp";
      return std::forward_as_tuple(s);  // dangerous! returns reference to local variable
  }
  ```
  Use only with perfect forwarding, not for return values.
- **Compilation time and error messages**: Nested tuple types cause extremely long error messages; consider defining type aliases for tuple types.
