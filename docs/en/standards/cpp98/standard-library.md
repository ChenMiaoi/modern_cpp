---
title: "C++98 Standard Library"
topic: unknown
feature: standard-library
standard: N/A
status_checked_at: 2026-06-02
---
# C++98 Standard Library

## STL Containers

| Container | Type | Underlying Implementation | Description |
|-----------|------|--------------------------|-------------|
| `vector` | Sequence | Dynamic array | Contiguous memory, O(1) random access |
| `deque` | Sequence | Segmented array | O(1) insertion at both ends |
| `list` | Sequence | Doubly linked list | O(1) insertion at any position |
| `set` / `multiset` | Associative | Red-black tree | Ordered collection |
| `map` / `multimap` | Associative | Red-black tree | Ordered key-value pairs |
| `stack` | Container adapter | Defaults to `deque` | LIFO |
| `queue` | Container adapter | Defaults to `deque` | FIFO |
| `priority_queue` | Container adapter | Defaults to `vector` | Priority queue |
| `bitset` | Special | Fixed-size bit set | Bit operations |

## Iterators

Five-category iterator hierarchy (increasing capability):

1. **Input Iterator** — Read-only, single-pass traversal
2. **Output Iterator** — Write-only, single-pass traversal
3. **Forward Iterator** — Read/write, multi-pass forward traversal
4. **Bidirectional Iterator** — Bidirectional traversal
5. **Random Access Iterator** — Random access

## Algorithms

Common algorithm examples:

```cpp
// Sort
std::sort(vec.begin(), vec.end());

// Find
auto it = std::find(vec.begin(), vec.end(), 42);

// Transform
std::transform(src.begin(), src.end(), dst.begin(), [](int x) { return x * 2; });
// Note: C++98 does not have lambdas; the line above is modern syntax

// Accumulate
int sum = std::accumulate(vec.begin(), vec.end(), 0);

// Erase-remove idiom
vec.erase(std::remove(vec.begin(), vec.end(), value), vec.end());
```

## Function Objects

C++98 uses function objects (functors) to implement transferable "callbacks":

```cpp
struct Greater {
    bool operator()(int a, int b) const { return a > b; }
};

std::sort(vec.begin(), vec.end(), Greater());
```

The standard library provides predefined function objects such as `std::less` and `std::greater`, as well as `std::bind1st`/`std::bind2nd` adapters (superseded by `std::bind` and lambdas in C++11).

## Strings and I/O

- **`std::string`**: Dynamic string supporting concatenation, search, substring operations, etc.
- **`<iostream>`**: `cin`/`cout`/`cerr`
- **`<fstream>`**: File I/O
- **`<sstream>`**: String streams

## `auto_ptr` (Deprecated)

C++98's smart pointer `auto_ptr` has a fatal design flaw — copying transfers ownership, which causes dangling pointers when used in containers. C++11 replaced it entirely with `unique_ptr`.
