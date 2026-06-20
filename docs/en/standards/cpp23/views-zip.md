---
title: std::views::zip
topic: cpp23
feature: views-zip
standard: C++23
status_checked_at: 2026-06-20
standard_refs:
  - draft: N4950
    clause: "[range.zip]"
proposals:
  - paper: P2328
    revision: R4
    status: accepted
exercises: []
solutions: []
---
# std::views::zip

C++23 introduces `std::views::zip` and `std::views::zip_transform`, which combine multiple ranges into a single range of tuple-like elements. This solves the long-standing problem of "parallel iteration over multiple containers" and eliminates the complexity of manual index management.

## Basic Usage

```cpp
#include <ranges>
#include <vector>
#include <string>
#include <iostream>
#include <tuple>

int main() {
    std::vector<int> ids = {1, 2, 3};
    std::vector<std::string> names = {"Alice", "Bob", "Charlie"};
    std::vector<double> scores = {90.5, 85.0, 92.3};

    // zip combines multiple ranges into tuple-like elements
    for (const auto& [id, name, score] : std::views::zip(ids, names, scores)) {
        std::cout << id << ": " << name << " = " << score << "\n";
    }
    // Output:
    // 1: Alice = 90.5
    // 2: Bob = 85
    // 3: Charlie = 92.3
}
```

## zip_transform

```cpp
#include <ranges>
#include <vector>
#include <iostream>
#include <algorithm>

int main() {
    std::vector<int> a = {1, 2, 3, 4};
    std::vector<int> b = {10, 20, 30, 40};

    // zip_transform applies a function to each pair of elements
    auto sums = std::views::zip_transform(std::plus{}, a, b);
    for (int s : sums) {
        std::cout << s << " ";
    }
    // Output: 11 22 33 44

    // Custom function
    auto products = std::views::zip_transform(
        [](int x, int y) { return x * y; }, a, b
    );
    for (int p : products) {
        std::cout << p << " ";
    }
    // Output: 10 40 90 160
}
```

## Comparison with Manual Indexing

```cpp
#include <ranges>
#include <vector>
#include <string>
#include <iostream>

int main() {
    std::vector<std::string> first_names = {"John", "Jane", "Bob"};
    std::vector<std::string> last_names = {"Doe", "Smith", "Johnson"};
    std::vector<int> ages = {30, 25, 35};

    // Old pattern: manual index management, error-prone
    for (size_t i = 0; i < first_names.size(); ++i) {
        std::cout << first_names[i] << " " << last_names[i]
                  << " (age " << ages[i] << ")\n";
    }

    // C++23 pattern: clear, safe, automatic length handling
    for (const auto& [first, last, age] : std::views::zip(first_names, last_names, ages)) {
        std::cout << first << " " << last << " (age " << age << ")\n";
    }
}
```

## Modifying Elements

```cpp
#include <ranges>
#include <vector>
#include <iostream>
#include <tuple>

int main() {
    std::vector<int> keys = {1, 2, 3};
    std::vector<std::string> values = {"a", "b", "c"};

    // zip returns references, allowing in-place modification
    for (auto&& [k, v] : std::views::zip(keys, values)) {
        k *= 10;
        v += "_modified";
    }

    // keys = {10, 20, 30}
    // values = {"a_modified", "b_modified", "c_modified"}
}
```

## Common Patterns

### Parallel Sorting

```cpp
#include <ranges>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>

int main() {
    std::vector<int> scores = {85, 92, 78, 95, 88};
    std::vector<std::string> names = {"Alice", "Bob", "Charlie", "Diana", "Eve"};

    // Sort by score while keeping names synchronized
    auto zipped = std::views::zip(scores, names);
    std::ranges::sort(zipped, std::ranges::greater{}, std::get<0>);

    for (const auto& [score, name] : std::views::zip(scores, names)) {
        std::cout << name << ": " << score << "\n";
    }
    // Diana: 95
    // Bob: 92
    // Alice: 85
    // Eve: 88
    // Charlie: 78
}
```

### Filtering Across Ranges

```cpp
#include <ranges>
#include <vector>
#include <iostream>
#include <algorithm>

int main() {
    std::vector<int> x = {1, 2, 3, 4, 5};
    std::vector<int> y = {2, 4, 6, 8, 10};

    // Find positions where both ranges have even values
    auto even_pairs = std::views::zip(x, y)
        | std::views::filter([](const auto& pair) {
            return std::get<0>(pair) % 2 == 0 && std::get<1>(pair) % 2 == 0;
        });

    for (const auto& [a, b] : even_pairs) {
        std::cout << "(" << a << ", " << b << ") ";
    }
    // Output: (2, 4) (4, 8)
}
```

### Building Structured Data

```cpp
#include <ranges>
#include <vector>
#include <string>
#include <iostream>

struct Record {
    int id;
    std::string name;
    double value;
};

int main() {
    std::vector<int> ids = {1, 2, 3};
    std::vector<std::string> names = {"sensor_A", "sensor_B", "sensor_C"};
    std::vector<double> values = {23.5, 19.8, 31.2};

    std::vector<Record> records;
    for (auto&& [id, name, val] : std::views::zip(ids, names, values)) {
        records.push_back({id, std::move(name), val});
    }
}
```

## The zip_view Type

```cpp
#include <ranges>
#include <vector>
#include <typeinfo>
#include <iostream>

int main() {
    std::vector<int> a = {1, 2};
    std::vector<double> b = {1.1, 2.2};

    auto z = std::views::zip(a, b);
    // Type: std::ranges::zip_view<std::vector<int>, std::vector<double>>
    // Each element: std::tuple<int&, double&>

    std::cout << typeid(z).name() << "\n";
}
```

## Empty Range Handling

```cpp
#include <ranges>
#include <vector>
#include <iostream>

int main() {
    std::vector<int> a = {1, 2, 3};
    std::vector<int> b;  // Empty range

    // zip result is bounded by the shortest range
    auto z = std::views::zip(a, b);
    // z is empty (length 0)

    for (const auto& [x, y] : z) {
        std::cout << x << " " << y << "\n";
    }
    // Will not execute
}
```

## Compiler Support

| Compiler | Version | Support Status |
|----------|---------|----------------|
| GCC | 14+ | Full support |
| Clang | 17+ | Full support |
| MSVC | 19.36 (VS 2022 17.6) | Full support |

> **Note**: MSVC requires `/std:c++latest` or `/std:c++23` flag.

## Notes

- `zip` is bounded by the shortest range; extra elements are ignored
- `zip` returns tuple-like references (`std::tuple<Ts&...>`)
- `zip_transform` accepts a callable as its first argument
- Cannot zip ranges of different lengths with automatic padding (use `std::views::counted` or `std::views::take` instead)
- Sorting a `zip` synchronously modifies all associated ranges
- `std::views::zip` only accepts input_range or higher-level ranges
