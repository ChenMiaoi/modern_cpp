---
title: "C++20 std::erase / std::erase_if"
topic: unknown
feature: erase
standard: N/A
status_checked_at: 2026-06-20
---
# C++20 `std::erase` / `std::erase_if`

## Overview

C++20 introduces uniform container erasure functions `std::erase` and `std::erase_if`, replacing the verbose Erase-Remove idiom. These are non-member functions applicable to all standard containers.

Key advantages:
- **Concise**: One line replaces `container.erase(std::remove(...), container.end())`.
- **Uniform**: Same interface for all containers.
- **Safe**: Avoids common iterator invalidation bugs.

## `std::erase` — Erase by Value

```cpp
#include <vector>
#include <iostream>

int main() {
    std::vector v = {1, 2, 3, 2, 5, 2, 7};
    auto count = std::erase(v, 2);  // remove all elements equal to 2
    std::cout << "Removed " << count << " elements\n";  // Removed 3 elements
    for (int n : v)
        std::cout << n << ' ';  // 1 3 5 7
}
```

## `std::erase_if` — Erase by Predicate

```cpp
#include <vector>
#include <iostream>

int main() {
    std::vector v = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto count = std::erase_if(v, [](int n) { return n % 2 == 0; });
    std::cout << "Removed " << count << " elements\n";  // Removed 5 elements
    for (int n : v)
        std::cout << n << ' ';  // 1 3 5 7 9
}
```

## Supported Containers

### `std::vector`

```cpp
#include <vector>
#include <iostream>

int main() {
    std::vector v = {1, 2, 3, 2, 5};
    std::erase(v, 2);
    // v == {1, 3, 5}

    std::erase_if(v, [](int n) { return n > 3; });
    // v == {1, 3}
}
```

### `std::string`

```cpp
#include <string>
#include <iostream>

int main() {
    std::string s = "Hello, World!";
    std::erase(s, 'l');  // remove all 'l'
    std::cout << s << "\n";  // Heo, Word!

    std::erase_if(s, [](char c) { return std::isspace(c); });
    std::cout << s << "\n";  // Heo,Word!
}
```

### `std::list`

```cpp
#include <list>
#include <iostream>

int main() {
    std::list l = {1, 2, 3, 2, 5};
    std::erase(l, 2);
    // l == {1, 3, 5}

    std::erase_if(l, [](int n) { return n < 3; });
    // l == {3, 5}
}
```

### `std::forward_list`

```cpp
#include <forward_list>
#include <iostream>

int main() {
    std::forward_list fl = {1, 2, 3, 2, 5};
    std::erase(fl, 2);
    // fl == {1, 3, 5}

    std::erase_if(fl, [](int n) { return n > 3; });
    // fl == {1, 3}
}
```

### `std::deque`

```cpp
#include <deque>
#include <iostream>

int main() {
    std::deque d = {1, 2, 3, 2, 5};
    std::erase(d, 2);
    // d == {1, 3, 5}

    std::erase_if(d, [](int n) { return n < 4; });
    // d == {5}
}
```

### `std::map` / `std::set`

```cpp
#include <map>
#include <set>
#include <iostream>

int main() {
    // map
    std::map<std::string, int> m = {{"a", 1}, {"b", 2}, {"c", 3}};
    std::erase_if(m, [](const auto& pair) { return pair.second > 1; });
    // m == {{"a", 1}}

    // set
    std::set s = {1, 2, 3, 4, 5};
    std::erase_if(s, [](int n) { return n % 2 == 0; });
    // s == {1, 3, 5}
}
```

## Comparison with Erase-Remove Idiom

```cpp
#include <vector>
#include <algorithm>
#include <iostream>

int main() {
    // Old: Erase-Remove idiom
    {
        std::vector v = {1, 2, 3, 2, 5, 2, 7};
        v.erase(std::remove(v.begin(), v.end(), 2), v.end());
        // v == {1, 3, 5, 7}
    }

    // New: std::erase
    {
        std::vector v = {1, 2, 3, 2, 5, 2, 7};
        std::erase(v, 2);
        // v == {1, 3, 5, 7}
    }

    // Old: Conditional removal
    {
        std::vector v = {1, 2, 3, 4, 5, 6};
        v.erase(std::remove_if(v.begin(), v.end(),
                [](int n) { return n % 2 == 0; }), v.end());
    }

    // New: std::erase_if
    {
        std::vector v = {1, 2, 3, 4, 5, 6};
        std::erase_if(v, [](int n) { return n % 2 == 0; });
    }
}
```

### Comparison Table

| Dimension | Erase-Remove | `std::erase` |
|-----------|-------------|--------------|
| Code volume | 2 lines | 1 line |
| Readability | Low (requires knowing the idiom) | High (self-descriptive) |
| Error risk | Iterator invalidation, boundary errors | None |
| Return value | void | Number of erased elements |
| Container support | Only `random_access` + `erase` | All containers |

## Real-World Applications

### Log Filtering

```cpp
#include <vector>
#include <string>
#include <iostream>

void filter_logs(std::vector<std::string>& logs) {
    // Remove empty logs
    std::erase_if(logs, [](const std::string& s) { return s.empty(); });
    // Remove debug logs
    std::erase_if(logs, [](const std::string& s) {
        return s.find("[DEBUG]") != std::string::npos;
    });
}
```

### Data Cleaning

```cpp
#include <vector>
#include <algorithm>
#include <iostream>

struct SensorReading {
    double value;
    bool valid;
};

void clean_data(std::vector<SensorReading>& readings) {
    // Remove invalid readings
    std::erase_if(readings, [](const SensorReading& r) { return !r.valid; });
    // Remove outlier values
    std::erase_if(readings, [](const SensorReading& r) {
        return std::abs(r.value) > 1000.0;
    });
}
```

### Configuration Cleanup

```cpp
#include <map>
#include <string>
#include <iostream>

void clean_config(std::map<std::string, std::string>& config) {
    // Remove empty-value configs
    std::erase_if(config, [](const auto& pair) {
        return pair.second.empty();
    });
    // Remove commented configs (keys starting with #)
    std::erase_if(config, [](const auto& pair) {
        return !pair.first.empty() && pair.first[0] == '#';
    });
}
```

## Compiler Support

| Compiler | Version | Support Status |
|----------|---------|----------------|
| GCC | 10+ | Full support (`-std=c++20`) |
| Clang | 16+ | Full support (`-std=c++20`) |
| MSVC | 19.29+ (VS 2019 16.10+) | Full support (`/std:c++20`) |

```cpp
// Compile verification
// g++ -std=c++20 erase.cpp -o erase
// clang++ -std=c++20 erase.cpp -o erase
// cl.exe /std:c++20 erase.cpp
```

## Common Pitfalls

```cpp
// 1. std::erase is not a container member function — it's a free function
v.erase(2);           // Wrong: vector::erase requires an iterator
std::erase(v, 2);     // Correct

// 2. erase_if predicate receives const reference (associative containers)
std::map m = {{"a", 1}};
std::erase_if(m, [](const auto& pair) { return pair.second == 1; });

// 3. Return value is size_type (number of erased elements)
auto count = std::erase(v, 2);
// count type depends on container (vector::size_type etc.)

// 4. Not applicable to C-style arrays
int arr[] = {1, 2, 3};
// std::erase(arr, 2);  // Compilation error
```

## Summary

- `std::erase` and `std::erase_if` are modern replacements for the Erase-Remove idiom.
- One line replaces two, making code more concise, safer, and readable.
- Works with all standard containers: vector, string, list, deque, map, set, etc.
- Returns the number of erased elements, useful for debugging and logging.
- Supported by GCC 10+, Clang 16+, and MSVC 19.29+.
