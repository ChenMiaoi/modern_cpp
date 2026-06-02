---
title: "C++11 Unordered Containers"
topic: unknown
feature: unordered-containers
standard: N/A
status_checked_at: 2026-06-02
---
# C++11 Unordered Containers

## Overview

C++11 introduced four unordered associative containers: `unordered_map`, `unordered_set`, `unordered_multimap`, `unordered_multiset`, defined in `<unordered_map>` and `<unordered_set>`. Based on hash table implementations, they provide average O(1) lookup, insertion, and deletion, replacing the `std::map`/`std::set` (red-black trees, O(log n)) that required manual maintenance in C++98.

The underlying structure is a bucket array. Each element is mapped to a bucket via a hash function, with colliding elements stored as a linked list within the bucket. When the load factor exceeds a threshold, automatic rehashing occurs — increasing the number of buckets and redistributing elements.

## Core API

### Four Unordered Containers

| Container | Allows Duplicate Keys | Mapping Relationship |
|-----------|----------------------|---------------------|
| `unordered_map<K,V>` | No | Key→Value |
| `unordered_set<K>` | No | Keys only |
| `unordered_multimap<K,V>` | Yes | Key→Value |
| `unordered_multiset<K>` | Yes | Keys only |

### Basic Usage

```cpp
#include <unordered_map>
#include <unordered_set>
#include <string>

// unordered_map — key-value pairs, unique keys
std::unordered_map<std::string, int> scores;
scores["Alice"] = 95;
scores.insert({"Charlie", 92});
scores.emplace("Diana", 88);      // in-place construction

auto it = scores.find("Alice");
if (it != scores.end()) {
    std::cout << it->first << ": " << it->second << "\n";  // Alice: 95
}

// C++20 has contains(); C++11 uses count()
if (scores.count("Bob") > 0) { /* ... */ }

scores.erase("Bob");

// unordered_set — stores keys only
std::unordered_set<std::string> names;
names.insert("Alice");
names.insert("Alice");  // duplicate insert, no effect
std::cout << names.size() << "\n";  // 1

// unordered_multimap — allows duplicate keys
std::unordered_multimap<std::string, int> course_grades;
course_grades.emplace("Alice", 95);
course_grades.emplace("Alice", 88);
// equal_range gets all values for the same key
auto range = course_grades.equal_range("Alice");
for (auto it = range.first; it != range.second; ++it) {
    std::cout << it->first << ": " << it->second << "\n";
}
```

### Performance Characteristics

| Operation | Average | Worst |
|-----------|---------|-------|
| Insert/Delete/Lookup | O(1) | O(n) |

Worst-case O(n) occurs when all elements hash to the same bucket. A good hash function effectively prevents this.

## Custom Hash Functions

### Specializing std::hash for Custom Types

```cpp
#include <functional>

struct Point {
    int x, y;
    bool operator==(const Point& o) const {
        return x == o.x && y == o.y;
    }
};

namespace std {
template<>
struct hash<Point> {
    size_t operator()(const Point& p) const {
        size_t h1 = std::hash<int>{}(p.x);
        size_t h2 = std::hash<int>{}(p.y);
        return h1 ^ (h2 * 2654435761u + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};
}

// Now usable directly
std::unordered_set<Point> points;
points.insert({1, 2});
```

### Using Custom Hash Function Objects

When you cannot modify the `std` namespace (e.g., third-party types), pass via template parameter:

```cpp
struct Record { std::string name; int id; };

struct RecordHash {
    size_t operator()(const Record& r) const {
        size_t h1 = std::hash<std::string>{}(r.name);
        size_t h2 = std::hash<int>{}(r.id);
        return h1 ^ (h2 * 2654435761u + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};

struct RecordEqual {
    bool operator()(const Record& a, const Record& b) const {
        return a.name == b.name && a.id == b.id;
    }
};

std::unordered_map<Record, double, RecordHash, RecordEqual> prices;
prices[{"apple", 1}] = 3.5;
```

## Bucket Interface and Load Management

### Bucket Interface

```cpp
std::unordered_map<std::string, int> map = {
    {"alpha", 1}, {"beta", 2}, {"gamma", 3}
};

std::cout << "Bucket count: " << map.bucket_count() << "\n";
std::cout << "Load factor: " << map.load_factor() << "\n";
std::cout << "alpha in bucket: " << map.bucket("alpha") << "\n";

// Iterate over elements in a single bucket
size_t idx = map.bucket("alpha");
for (auto it = map.begin(idx); it != map.end(idx); ++it) {
    std::cout << "  " << it->first << ": " << it->second << "\n";
}
```

### Load Factor and reserve

```cpp
std::unordered_map<std::string, int> map;

map.max_load_factor(0.75);  // lower = fewer collisions = more memory

// Pre-allocate space — avoids multiple rehashes (most important performance optimization)
map.reserve(1000);  // allocate enough buckets in one go

// Manually trigger rehash
map.rehash(1024);   // ensure bucket_count >= 1024

// Typical usage of reserve
std::unordered_map<int, std::string> lookup;
lookup.reserve(10000);
for (int i = 0; i < 10000; ++i) {
    lookup[i] = "item_" + std::to_string(i);
}
```

## Choosing Between Ordered and Unordered Containers

```cpp
// Need range queries → use ordered
std::map<int, std::string> ordered;
// Find elements in range [2, 6)
auto lo = ordered.lower_bound(2);
auto hi = ordered.upper_bound(6);
for (auto it = lo; it != hi; ++it) { /* ... */ }

// Pure lookup/existence check → use unordered
std::unordered_set<int> fast_lookup;
fast_lookup.insert(42);
bool exists = fast_lookup.count(42) > 0;  // O(1)
```

**Choose unordered**: O(1) lookup needed, no sorted traversal, keys have good hashes, larger data volumes.
**Choose ordered**: Ordered traversal needed, range queries, keys lack good hashes, memory-sensitive, need stable iterators.

## Custom Type as Key — Complete Example

```cpp
#include <unordered_map>
#include <string>

struct StudentId {
    int grade, class_num, number;
    bool operator==(const StudentId& o) const {
        return grade == o.grade && class_num == o.class_num && number == o.number;
    }
};

struct StudentIdHash {
    size_t operator()(const StudentId& id) const {
        size_t seed = std::hash<int>{}(id.grade);
        seed ^= std::hash<int>{}(id.class_num)
              + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<int>{}(id.number)
              + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};

int main() {
    std::unordered_map<StudentId, std::string, StudentIdHash> students;
    students[{3, 2, 15}] = "Alice";
    students[{3, 2, 8}]  = "Bob";

    auto it = students.find({3, 2, 15});
    if (it != students.end()) std::cout << it->second << "\n";  // Alice
}
```

## Best Practices

1. **`reserve` in advance**: When the approximate element count is known, call `reserve(n)` to avoid repeated rehashing. The simplest and most effective performance optimization.
2. **Hash function quality determines performance**: Simply XORing all fields is an anti-pattern — use different mixing offsets for different fields.
3. **`emplace` over `insert`**: In-place construction avoids temporary pair copy/move.
4. **`operator[]` inserts a default value**: For query-only access, use `find()` or `count()` to avoid polluting the container.
5. **Manually rehash after bulk inserts**: `map.rehash(0)` adjusts bucket count based on current size and max_load_factor.

## Common Pitfalls

- **`operator[]` implicit insertion**: `auto& v = map[key];` inserts a default value when the key doesn't exist. Use `find()` for read-only queries.
- **Rehash invalidates iterators**: When insert/erase causes rehash, all iterators and references are invalidated. Be extremely careful when modifying containers during iteration.
- **Hash collisions cause O(n)**: A poorly implemented custom hash function can be slower than `std::map`.
- **`unordered_multimap::erase` behavior**: `erase(it)` removes one element; `erase(key)` removes all elements matching the key.
- **Higher memory overhead than ordered containers**: Requires bucket array + linked list pointers. For small element counts, `std::map` may use less memory and be faster.
- **Floating-point numbers as keys**: `std::hash<double>` has undefined behavior for NaN. Avoid floating-point numbers as keys for unordered containers.
