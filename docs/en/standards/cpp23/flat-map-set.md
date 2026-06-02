---
title: std::flat_map / std::flat_set
topic: cpp23
feature: flat_map
standard: C++23
status_checked_at: 2026-06-02
standard_refs:
  - draft: N4950
    clause: "[flat.map]"
proposals:
  - paper: P0429
    revision: R9
    status: accepted
exercises:
  - exercises/cpp23/flatmap1.cpp
solutions:
  - exercises/solutions/flatmap1.cpp
---
# std::flat_map / std::flat_set

C++23 introduces `std::flat_map` and `std::flat_set`, which implement associative containers using sorted `std::vector` under the hood. Compared to the red-black tree implementations of `std::map`/`std::set`, they offer better performance for small datasets and cache-sensitive scenarios.

## Basic Usage

```cpp
#include <flat_map>
#include <flat_set>
#include <iostream>

int main() {
    std::flat_map<std::string, int> scores;
    scores["Alice"] = 95;
    scores["Bob"] = 87;
    scores["Charlie"] = 92;

    for (const auto& [name, score] : scores) {
        std::cout << name << ": " << score << "\n";
    }
    // Sorted by key: Alice, Bob, Charlie

    std::flat_set<int> ids = {3, 1, 4, 1, 5, 9, 2, 6};
    // ids = {1, 2, 3, 4, 5, 6, 9}
    std::cout << ids.contains(4) << "\n";
}
```

## Internal Structure

```cpp
// flat_map is backed by two parallel sorted vectors
template <typename Key, typename T, typename Compare = std::less<Key>,
          typename KeyContainer = std::vector<Key>,
          typename MappedContainer = std::vector<T>>
class flat_map {
    KeyContainer keys_;
    MappedContainer values_;  // keys_[i] corresponds to values_[i]
};
```

## Construction

```cpp
std::flat_map<std::string, int> fm;  // Default construction

// Construct from already-sorted containers (O(n), skips sorting)
std::vector<std::pair<std::string, int>> sorted = {
    {"a", 1}, {"b", 2}, {"c", 3}
};
std::flat_map<std::string, int> fm_sorted(std::sorted_unique, sorted);

// Construct from unsorted data (O(n log n))
std::vector<std::pair<int, std::string>> unsorted = {
    {3, "c"}, {1, "a"}, {2, "b"}, {1, "x"}
};
std::flat_map<int, std::string> fm_unsorted(unsorted.begin(), unsorted.end());
// {1: "x"}, {2: "b"}, {3: "c"}
```

## Lookup Operations

```cpp
std::flat_map<int, std::string> fm = {{1, "a"}, {3, "c"}, {5, "e"}};

auto it = fm.find(3);              // O(log n)
bool has = fm.contains(4);         // false
auto lb = fm.lower_bound(2);      // Points to (3, "c")
size_t c = fm.count(3);            // 1
```

## Insertion and Deletion

```cpp
std::flat_map<int, std::string> fm;

// Insertion — O(n) worst case (element shifting required)
fm.insert({3, "c"});
fm.emplace(5, "e");

// Hint insertion (faster)
auto hint = fm.end();
fm.insert(hint, {4, "d"});

// Deletion — O(n)
fm.erase(3);

// Batch insertion
std::vector<std::pair<int, std::string>> batch = {{6, "f"}, {7, "g"}};
fm.insert_range(batch);
```

## Comparison with std::map / std::set

| Property | `std::map`/`set` | `std::flat_map`/`flat_set` |
|----------|------------------|---------------------------|
| Underlying structure | Red-black tree | Sorted vector |
| Lookup | O(log n) | O(log n), smaller constant factor |
| Insertion/deletion | O(log n) | O(n) (element shifting) |
| Cache locality | Poor (scattered nodes) | Good (contiguous memory) |
| Iterator stability | Stable | Unstable |
| Memory overhead | Per-node pointers | Compact, no extra pointers |

Performance guidelines:
- **< 100 elements**: flat is almost always faster
- **100–1000**: depends on read/write ratio
- **> 1000 with frequent writes**: map is usually better

## flat_multimap / flat_multiset

Versions that allow duplicate keys:

```cpp
std::flat_multimap<int, std::string> fmm;
fmm.insert({1, "a"});
fmm.insert({1, "b"});  // Duplicates allowed

std::flat_multiset<int> fms = {1, 1, 2, 2, 3};
```

## Suitable Scenarios

```
✓ Configuration tables (few keys, read-heavy)
✓ Lookup tables / LUTs (read-only after construction)
✓ Caching small maps (< 1000 entries)
✗ Frequent insertion/deletion (use map/set)
✗ Iterator/reference stability required (use map/set)
```

## Caveats

- Insertion/deletion invalidates all iterators, pointers, and references
- `keys()` and `values()` provide const access to the underlying containers
- Comparison operators compare in lexicographic order (consistent with `std::map`)
