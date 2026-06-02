---
title: C++20 Ranges
topic: cpp20
feature: ranges
standard: C++20
status_checked_at: 2026-06-01
exercises:
  - exercises/cpp20/ranges1.cpp
solutions:
  - exercises/solutions/ranges1.cpp
---
# C++20 Ranges

## Overview

C++20 Ranges is a fundamental upgrade to the STL algorithm and container abstractions. Traditional STL algorithms accept a pair of iterators (`begin`/`end`), which easily leads to type mismatches, out-of-bounds access, and redundant parameters. Ranges elevate the "traversable sequence" to a first-class citizen, introducing the **range** concept, **view** views, and **pipe operator `|`** chain composition, transforming data processing code from imperative to declarative.

Core goals: replace iterator pairs with a single range object; achieve lazy evaluation through views, avoiding intermediate container allocation; provide composable pipeline-style data transformation syntax. Header: `<ranges>`, ranges overloads of algorithms are in `<algorithm>`.

## Range Concept Hierarchy

| Concept | Requirement | Typical Types |
|---------|-------------|---------------|
| `input_range` | At least single-pass traversal | `std::istream_view` |
| `forward_range` | Supports multi-pass traversal | `std::forward_list` |
| `bidirectional_range` | Supports reverse iteration | `std::list` |
| `random_access_range` | O(1) subscript access | `std::deque` |
| `contiguous_range` | Contiguous memory elements | `std::vector`, `std::string` |
| `viewable_range` | Can be adapted as a view | Lvalue ranges or certain rvalues |

```cpp
#include <ranges>
#include <vector>
#include <list>
static_assert(std::ranges::contiguous_range<std::vector<int>>);
static_assert(std::ranges::bidirectional_range<std::list<int>>);
static_assert(std::ranges::input_range<std::list<int>>);
// std::list does not satisfy random_access_range
```

## Views and Lazy Evaluation

A **view** is a lightweight range adaptation result satisfying the `std::ranges::view` concept. Lazy evaluation: views do not compute immediately, only producing elements on demand during iteration. O(1) construction and copying; multiple views are chained through `|` to form a pipeline:

```cpp
#include <ranges>
#include <vector>
#include <iostream>
int main() {
    std::vector data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto result = data
        | std::views::filter([](int n) { return n % 2 == 0; })
        | std::views::transform([](int n) { return n * n; });
    for (int v : result)
        std::cout << v << ' ';  // 4 16 36 64 100
}
```

`result` itself is just a lightweight view object; the `for` loop triggers filter and transform element-by-element during traversal.

## Common View Adapters

All adapters are in the `std::views` (i.e., `std::ranges::views`) namespace:

```cpp
std::vector data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

// filter: keep elements satisfying the predicate
auto even = data | std::views::filter([](int n) { return n % 2 == 0; });
// transform: apply a transformation to each element
auto squared = data | std::views::transform([](int n) { return n * n; });
// take / drop: take first N / skip first N
auto first3 = std::views::iota(1) | std::views::take(3);  // 1, 2, 3
auto skip2  = data | std::views::drop(2);                  // from 3rd onward
// iota: generate integer sequence (potentially infinite)
auto naturals = std::views::iota(1);      // infinite: 1, 2, 3, ...
auto bounded  = std::views::iota(1, 10);  // bounded: 1..9
// join: flatten nested ranges
std::vector<std::vector<int>> nested = {{1, 2}, {3, 4}, {5}};
auto flat = nested | std::views::join;    // 1, 2, 3, 4, 5
// reverse / elements
auto rev = data | std::views::reverse;    // 10, 9, ..., 1
std::vector<std::pair<int, std::string>> items = {{1, "a"}, {2, "b"}};
auto keys = items | std::views::elements<0>;  // 1, 2
```

## Pipeline Composition and Range Adapters

The `|` operator connects the range on the left with the view adapter on the right, supporting arbitrary depth chain composition:

```cpp
#include <ranges>
#include <vector>
#include <iostream>
#include <string>
int main() {
    std::vector<std::string> words = {
        "the", "quick", "brown", "fox", "jumps", "over", "lazy", "dog"
    };
    auto result = words
        | std::views::filter([](const std::string& s) { return s.size() >= 4; })
        | std::views::transform([](std::string s) {
              for (auto& c : s) c = static_cast<char>(std::toupper(c));
              return s;
          })
        | std::views::take(3);
    for (const auto& w : result)
        std::cout << w << ' ';  // QUICK BROWN JUMPS
}
```

Adapters are function objects and can also be called directly: `std::views::filter(pred)(range)` is equivalent to `range | std::views::filter(pred)`.

## subrange and common_view

`std::ranges::subrange` wraps a pair of iterators (or iterator + sentinel) into a range object, solving the problem of "scattered iterator pairs":

```cpp
std::vector v = {10, 20, 30, 40, 50};
std::ranges::subrange sub(v.begin() + 1, v.begin() + 4);
for (int x : sub) std::cout << x << ' ';  // 20 30 40
```

`std::views::common` adapts a view with differing sentinel types into a common view where `begin`/`end` types match, for interoperating with legacy code:
```cpp
auto cv = std::views::iota(1, 10) | std::views::common;
```

## ranges Namespace Algorithms

C++20 provides `std::ranges::` versions of nearly all STL algorithms, accepting ranges instead of iterator pairs, and supporting projections:

```cpp
std::vector v = {5, 3, 1, 4, 2};
std::ranges::sort(v);
auto found = std::ranges::find(v, 4);
bool all_pos = std::ranges::all_of(v, [](int n) { return n > 0; });
// projection: sort by a specific field
std::vector<std::pair<int, std::string>> items = {{3, "c"}, {1, "a"}, {2, "b"}};
std::ranges::sort(items, {}, &std::pair<int,std::string>::first);
```

## Comparison with Traditional STL

| Dimension | Traditional STL | Ranges |
|-----------|-----------------|--------|
| Interface | Iterator pair `begin/end` | Single range object |
| Chaining | Requires intermediate containers or manual loops | `\|` pipeline, zero intermediate allocation |
| Laziness | None (algorithms execute immediately) | Views compute on demand |
| Error messages | Verbose template errors | Concept constraints, clearer errors |
| Customization | Overload algorithms or write function objects | Implement `view_interface` or adapters |

## C++23 New Views

```cpp
std::vector a = {1, 2, 3};
std::vector b = {'a', 'b', 'c'};
// views::zip: parallel traversal of multiple ranges
for (auto [x, y] : std::views::zip(a, b))
    std::cout << x << y << ' ';  // 1a 2b 3c
// views::enumerate: indexed traversal
for (auto [i, v] : std::views::enumerate(a))
    std::cout << i << ':' << v << ' ';  // 0:1 1:2 2:3
// views::chunk: split into fixed-size chunks
std::vector data = {1, 2, 3, 4, 5, 6, 7};
for (auto chunk : data | std::views::chunk(3)) {
    for (int v : chunk) std::cout << v << ' ';
    std::cout << '|';  // 1 2 3 | 4 5 6 | 7 |
}
// views::slide: sliding window
for (auto window : data | std::views::slide(3)) { /* each window is a 3-element view */ }
// ranges::to: view -> container
auto vec = (data | std::views::filter([](int n) { return n % 2; }))
           | std::ranges::to<std::vector>();
```

## Best Practices

1. **Prefer view pipelines over hand-written loops**: filter/transform/take combinations are clearer than nested loops, with zero allocation overhead.
2. **Use `ranges::to`** (C++23) or `std::vector(view.begin(), view.end())` when materialization is needed.
3. **Avoid repeated iteration over views**: some views (like `filter` results) do not satisfy `forward_range` and can only be traversed once.
4. **Ensure view lifetime safety**: views hold references or iterators internally; the underlying data must outlive the view.
5. **Use `std::ranges::` algorithm versions**: cleaner signatures, support for sentinels and projections.

## Common Pitfalls

1. **Dangling references**: views store references or iterators; using a view after the underlying data is destroyed is undefined behavior:
   ```cpp
   auto dangling() {
       std::vector v = {1, 2, 3};
       return v | std::views::filter([](int n) { return n > 1; });
       // UB: v is destroyed, view's internal iterator is invalid
   }
   ```

2. **Constructing views from temporary ranges**: `auto v = getTemporaryVector() | std::views::transform(...)` — the temporary object is destroyed after the statement ends, leaving the view with a dangling iterator.

3. **Not understanding lazy semantics**: view pipelines do not execute immediately; `auto lazy = data | std::views::transform(f) | std::views::filter(g);` — neither `f` nor `g` is called until `lazy` is traversed.

4. **Views may not satisfy certain range concepts**: e.g., `filter_view` does not satisfy `random_access_range`, even if the underlying range does.

5. **`views::split` returns sub-ranges that cannot directly construct `std::string` in C++20** (C++23 `ranges::to` solves this).

6. **`iota` infinite ranges without `take` cause infinite loops**:
   ```cpp
   // Wrong: infinite loop
   for (int n : std::views::iota(1)) { /* ... */ }
   // Correct: add an upper bound
   for (int n : std::views::iota(1) | std::views::take(100)) { /* ... */ }
   ```
