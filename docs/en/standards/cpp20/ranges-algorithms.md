---
title: "C++20 Ranges Algorithms"
topic: unknown
feature: ranges-algorithms
standard: N/A
status_checked_at: 2026-06-20
---
# C++20 Ranges Algorithms

## Overview

C++20 provides `std::ranges::` namespace versions for nearly all STL algorithms in `<algorithm>`. Compared to traditional iterator-pair interfaces, ranges algorithms:

- Accept a single range object instead of `begin/end` iterator pairs.
- Support **projection**: compare/sort by a sub-field of elements.
- Support **sentinels**: `end` can be a different type from `begin`.
- Return iterators instead of void, enabling chaining.

## Common Algorithms

### `ranges::find`

```cpp
#include <algorithm>
#include <vector>
#include <iostream>

int main() {
    std::vector v = {1, 2, 3, 4, 5};
    auto it = std::ranges::find(v, 3);
    if (it != v.end()) {
        std::cout << "Found: " << *it << "\n";  // Found: 3
    }

    // Find with predicate
    auto it2 = std::ranges::find_if(v, [](int n) { return n > 3; });
    std::cout << "First > 3: " << *it2 << "\n";  // First > 3: 4
}
```

### `ranges::sort`

```cpp
#include <algorithm>
#include <vector>
#include <iostream>

int main() {
    std::vector v = {5, 3, 1, 4, 2};
    std::ranges::sort(v);
    // v == {1, 2, 3, 4, 5}

    // Custom comparator
    std::ranges::sort(v, std::ranges::greater{});
    // v == {5, 4, 3, 2, 1}

    // Projection: sort by absolute value
    std::vector nums = {-3, 1, -2, 4, -5};
    std::ranges::sort(nums, {}, [](int n) { return std::abs(n); });
    // nums == {1, -2, -3, 4, -5}
}
```

### `ranges::transform`

```cpp
#include <algorithm>
#include <vector>
#include <iostream>

int main() {
    std::vector v = {1, 2, 3, 4, 5};
    std::vector<int> result;
    std::ranges::transform(v, std::back_inserter(result),
                           [](int n) { return n * n; });
    // result == {1, 4, 9, 16, 25}

    // Binary transform
    std::vector a = {1, 2, 3};
    std::vector b = {10, 20, 30};
    std::vector<int> sum;
    std::ranges::transform(a, b, std::back_inserter(sum), std::plus{});
    // sum == {11, 22, 33}
}
```

### `ranges::filter` (via views)

```cpp
#include <ranges>
#include <vector>
#include <iostream>

int main() {
    std::vector v = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto evens = v | std::views::filter([](int n) { return n % 2 == 0; });
    for (int n : evens)
        std::cout << n << ' ';  // 2 4 6 8 10
}
```

### `ranges::for_each`

```cpp
#include <algorithm>
#include <vector>
#include <iostream>

int main() {
    std::vector v = {1, 2, 3, 4, 5};
    std::ranges::for_each(v, [](int& n) { n *= 2; });
    // v == {2, 4, 6, 8, 10}

    // With projection
    struct Person { std::string name; int age; };
    std::vector<Person> people = {{"Alice", 30}, {"Bob", 25}};
    std::ranges::for_each(people, [](const Person& p) {
        std::cout << p.name << "\n";
    }, &Person::name);  // project to name field
}
```

### `ranges::count_if`

```cpp
#include <algorithm>
#include <vector>
#include <iostream>

int main() {
    std::vector v = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto count = std::ranges::count_if(v, [](int n) { return n % 2 == 0; });
    std::cout << "Even count: " << count << "\n";  // Even count: 5
}
```

### `ranges::min` / `ranges::max`

```cpp
#include <algorithm>
#include <vector>
#include <iostream>

int main() {
    std::vector v = {3, 1, 4, 1, 5, 9, 2, 6};
    std::cout << "Min: " << std::ranges::min(v) << "\n";  // Min: 1
    std::cout << "Max: " << std::ranges::max(v) << "\n";  // Max: 9

    // Min by absolute value
    std::vector nums = {-5, 3, -2, 4};
    auto min_abs = std::ranges::min(nums, {}, [](int n) { return std::abs(n); });
    std::cout << "Min abs: " << min_abs << "\n";  // Min abs: -2
}
```

## Projected Iterators

Projection is a core feature of ranges algorithms: instead of comparing elements directly, they compare projected values of elements.

```cpp
#include <algorithm>
#include <vector>
#include <iostream>
#include <string>

struct Employee {
    std::string name;
    int salary;
};

int main() {
    std::vector<Employee> employees = {
        {"Alice", 70000}, {"Bob", 50000}, {"Charlie", 60000}
    };

    // Sort by salary
    std::ranges::sort(employees, std::ranges::less{}, &Employee::salary);
    for (const auto& e : employees)
        std::cout << e.name << ": " << e.salary << "\n";
    // Bob: 50000
    // Charlie: 60000
    // Alice: 70000

    // Find by salary
    auto it = std::ranges::find(employees, 60000, &Employee::salary);
    std::cout << "Found: " << it->name << "\n";  // Found: Charlie
}
```

## Comparison with Classic STL Algorithms

| Dimension | Classic STL | Ranges Algorithms |
|-----------|-------------|-------------------|
| Interface | `std::sort(begin, end)` | `std::ranges::sort(range)` |
| Comparator | `std::sort(begin, end, comp)` | `std::ranges::sort(range, comp, proj)` |
| Projection | Not supported | Natively supported |
| Return value | void (mostly) | Iterator (enables chaining) |
| Sentinels | Not supported | Different-type sentinels supported |
| Range checks | None | Type safety via concepts |

```cpp
// Classic approach
std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) {
    return a.salary < b.salary;
});

// Ranges approach
std::ranges::sort(v, {}, &Employee::salary);
```

## Pipeline Composition with Algorithms

Ranges algorithms integrate seamlessly with view pipelines:

```cpp
#include <ranges>
#include <algorithm>
#include <vector>
#include <iostream>

int main() {
    std::vector v = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // Filter, transform, then sort
    auto result = v
        | std::views::filter([](int n) { return n % 2 == 0; })
        | std::views::transform([](int n) { return n * n; })
        | std::views::common;

    std::vector<int> sorted(result.begin(), result.end());
    std::ranges::sort(sorted);
    for (int n : sorted)
        std::cout << n << ' ';  // 4 16 36 64 100
}
```

## All Algorithms Supporting Projection

The following ranges algorithms support a projection parameter:

| Algorithm | Description |
|-----------|-------------|
| `ranges::sort` | Sorting |
| `ranges::stable_sort` | Stable sorting |
| `ranges::partial_sort` | Partial sorting |
| `ranges::min` / `ranges::max` | Minimum/maximum value |
| `ranges::minmax` | Min-max pair |
| `ranges::find` / `ranges::find_if` | Find |
| `ranges::count` / `ranges::count_if` | Count |
| `ranges::all_of` / `ranges::any_of` / `ranges::none_of` | Condition checks |
| `ranges::remove_if` | Conditional removal |
| `ranges::unique` | Deduplicate |
| `ranges::adjacent_find` | Find adjacent duplicates |
| `ranges::lower_bound` / `ranges::upper_bound` | Binary search |

## Compiler Support

| Compiler | Version | Support Status |
|----------|---------|----------------|
| GCC | 10+ | Full support (`-std=c++20`) |
| Clang | 16+ | Full support (`-std=c++20`) |
| MSVC | 19.29+ (VS 2019 16.10+) | Full support (`/std:c++20`) |

```cpp
// Compile verification
// g++ -std=c++20 ranges_algo.cpp -o ranges_algo
// clang++ -std=c++20 ranges_algo.cpp -o ranges_algo
// cl.exe /std:c++20 ranges_algo.cpp
```

## Common Pitfalls

```cpp
// 1. Projection is not a lambda — it's a function pointer or member pointer
std::ranges::sort(v, {}, &Employee::salary);  // Correct
std::ranges::sort(v, {}, [](const auto& e) { return e.salary; });  // Also works

// 2. Return value is an iterator, not a range
auto it = std::ranges::find(v, 42);
// it is an iterator, not a range

// 3. Algorithms don't modify range size (mostly)
// ranges::sort sorts the original range
// ranges::transform needs an output iterator

// 4. Projection composition is Cartesian product
// sort(comp, proj) is equivalent to sort([comp, proj](a, b){ return comp(proj(a), proj(b)); })
```

## Summary

- Ranges algorithms accept range objects, simpler than traditional iterator-pair interfaces.
- Projection is a core feature enabling sorting/finding by sub-fields.
- Returning iterators enables chaining and composition with other algorithms.
- Seamless integration with view pipelines for declarative data processing.
- Supported by GCC 10+, Clang 16+, and MSVC 19.29+.
