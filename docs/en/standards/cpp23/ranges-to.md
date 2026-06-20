---
title: std::ranges::to
topic: cpp23
feature: ranges-to
standard: C++23
status_checked_at: 2026-06-20
standard_refs:
  - draft: N4950
    clause: "[range.utility]"
proposals:
  - paper: P1206
    revision: R7
    status: accepted
exercises: []
solutions: []
---
# std::ranges::to

C++23 introduces `std::ranges::to`, a general-purpose mechanism for constructing containers from any range. It solves the long-standing problem of converting views into concrete containers in an elegant way, making it one of the most practical utilities in the Ranges library.

## Basic Usage

```cpp
#include <ranges>
#include <vector>
#include <list>
#include <set>
#include <iostream>
#include <string>

int main() {
    // Construct from initializer list
    auto v = std::ranges::to<std::vector>({1, 2, 3, 4, 5});

    // Construct from another container (type conversion)
    std::list<int> lst = {10, 20, 30};
    auto vec = std::ranges::to<std::vector>(lst);

    // Construct from a view (most common usage)
    std::vector<int> nums = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto evens = nums | std::views::filter([](int n) { return n % 2 == 0; });
    auto result = std::ranges::to<std::vector>(evens);
    // result = {2, 4, 6, 8, 10}
}
```

## Supported Container Types

```cpp
#include <ranges>
#include <vector>
#include <list>
#include <deque>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <string>

int main() {
    std::vector<int> src = {1, 2, 3};

    auto vec = std::ranges::to<std::vector>(src);
    auto lst = std::ranges::to<std::list>(src);
    auto deq = std::ranges::to<std::deque>(src);
    auto set = std::ranges::to<std::set>(src);
    auto uset = std::ranges::to<std::unordered_set>(src);
    auto str = std::ranges::to<std::string>(src | std::views::transform([](int i) {
        return static_cast<char>('0' + i);
    }));
}
```

## With std::from_ranges Constructors

`ranges::to` relies on the ranges constructors added in C++23. Every container can be directly constructed from an input_range:

```cpp
#include <ranges>
#include <vector>
#include <set>

int main() {
    auto odds = std::views::iota(1, 11) | std::views::filter([](int n) {
        return n % 2 == 1;
    });

    // ranges::to is syntactic sugar that calls the ranges constructor
    std::vector<int> v1(odds);  // C++23 ranges constructor
    auto v2 = std::ranges::to<std::vector>(odds);  // More explicit intent
}
```

## Practical Use Cases

### Collecting Filtered Results

```cpp
#include <ranges>
#include <vector>
#include <string>
#include <iostream>

struct Student {
    std::string name;
    int score;
};

int main() {
    std::vector<Student> students = {
        {"Alice", 90}, {"Bob", 65}, {"Charlie", 85},
        {"Diana", 72}, {"Eve", 95}
    };

    // Collect names of excellent students
    auto excellent_names = students
        | std::views::filter([](const Student& s) { return s.score >= 80; })
        | std::views::transform([](const Student& s) { return s.name; });

    auto names = std::ranges::to<std::vector<std::string>>(excellent_names);
    // names = {"Alice", "Charlie", "Eve"}
}
```

### Collecting After Chained Operations

```cpp
#include <ranges>
#include <vector>
#include <set>
#include <iostream>

int main() {
    std::vector<int> data = {5, 3, 8, 1, 9, 2, 7, 4, 6};

    // Sort + deduplicate + collect into a set (automatic dedup)
    auto result = data
        | std::views::transform([](int n) { return n * 2; })
        | std::ranges::to<std::set>();

    // result = {2, 4, 6, 8, 10, 12, 14, 16, 18}
}
```

### Building Associative Containers

```cpp
#include <ranges>
#include <vector>
#include <map>
#include <string>
#include <iostream>

int main() {
    std::vector<std::pair<std::string, int>> pairs = {
        {"apple", 3}, {"banana", 5}, {"cherry", 2}
    };

    auto fruit_map = std::ranges::to<std::map>(pairs);
    // fruit_map = {{"apple":3}, {"banana":5}, {"cherry":2}}

    // Build from a key-value view
    auto keys = std::views::iota(1, 4);
    auto mapping = keys | std::views::transform([](int i) {
        return std::pair{i, i * i};
    });
    auto sq_map = std::ranges::to<std::map>(mapping);
    // sq_map = {{1:1}, {2:4}, {3:9}}
}
```

## Custom Container Support

Any custom container with a ranges constructor works with `ranges::to`:

```cpp
#include <ranges>
#include <vector>

template <typename T>
class MyBuffer {
    std::vector<T> data_;
public:
    template <std::ranges::input_range R>
    explicit MyBuffer(R&& range) : data_(std::ranges::begin(range), std::ranges::end(range)) {}

    const auto& data() const { return data_; }
};

// Enable ranges::to support
template <typename T>
MyBuffer(std::ranges::input_range) -> MyBuffer<T>;

int main() {
    std::vector<int> src = {1, 2, 3, 4, 5};
    auto buf = std::ranges::to<MyBuffer>(src);
}
```

## Forwarding Ranges and Move Semantics

```cpp
#include <ranges>
#include <vector>
#include <string>
#include <iostream>

int main() {
    // Support move-only types
    auto uptrs = std::views::iota(0, 5)
        | std::views::transform([](int i) {
            return std::make_unique<int>(i);
        });

    auto vec = std::ranges::to<std::vector<std::unique_ptr<int>>>(std::move(uptrs));
}
```

## Comparison with Pre-C++23 Patterns

```cpp
#include <ranges>
#include <vector>
#include <algorithm>
#include <iterator>

int main() {
    std::vector<int> src = {1, 2, 3, 4, 5};
    auto view = src | std::views::filter([](int n) { return n > 2; });

    // C++20 verbose pattern
    std::vector<int> old_way;
    std::ranges::copy(view, std::back_inserter(old_way));

    // C++23 ranges::to (concise and clear intent)
    auto new_way = std::ranges::to<std::vector>(view);
}
```

## Compiler Support

| Compiler | Version | Support Status |
|----------|---------|----------------|
| GCC | 14+ | Full support |
| Clang | 17+ | Full support |
| MSVC | 19.38 (VS 2022 17.8) | Full support |

> **Note**: Some compilers may require the `-std=c++23` or `-std=c++2b` flag.

## Notes

- `ranges::to` requires the target container to support ranges constructors (added in C++23)
- For `std::string`, `ranges::to` converts a character range to a string
- When the container size is known (e.g., `ranges::size` is available), `ranges::to` pre-allocates memory
- `ranges::to` supports `std::initializer_list` as an argument
- Prefer `ranges::to` over the manual `std::ranges::copy` + `std::back_inserter` pattern
