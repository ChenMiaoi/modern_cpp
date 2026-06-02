---
title: "Smart Pointers"
topic: unknown
feature: smart-pointers
standard: N/A
status_checked_at: 2026-06-02
---
# Smart Pointers

## Overview

C++11 replaced `auto_ptr` with three smart pointers, establishing clear resource ownership semantics:

| Smart Pointer | Ownership | Use Case |
|---------------|-----------|----------|
| `unique_ptr` | Exclusive | Default choice, zero overhead |
| `shared_ptr` | Shared | Multiple owners managing the same resource |
| `weak_ptr` | Observing | Breaking `shared_ptr` circular references |

## `std::unique_ptr`

Exclusive ownership, not copyable, only movable. Overhead is identical to a raw pointer (zero extra memory, zero runtime overhead).

```cpp
#include <memory>

// Creation
auto p = std::make_unique<int>(42);             // recommended
auto arr = std::make_unique<int[]>(10);          // array version

// Usage
std::cout << *p << '\n';      // dereference: 42
std::cout << p->method();     // member access

// Move (ownership transfer)
auto p2 = std::move(p);       // p becomes nullptr
assert(p == nullptr);

// Custom deleter
auto file = std::unique_ptr<FILE, decltype(&fclose)>(
    fopen("data.txt", "r"), &fclose);
```

### When to Use

- Default choice, unless there is a clear sharing need
- Factory function return values
- Members in the Pimpl idiom
- Storing polymorphic objects in containers

## `std::shared_ptr`

Shared ownership, internally maintains a reference count. Resources are released when the last `shared_ptr` is destroyed.

```cpp
auto p1 = std::make_shared<int>(42);  // reference count = 1
auto p2 = p1;                          // reference count = 2
auto p3 = p1;                          // reference count = 3

p2.reset();  // reference count = 2
p3.reset();  // reference count = 1

std::cout << p1.use_count();  // 1
```

### Overhead

- **Memory**: Additional control block (reference count + weak reference count + deleter + allocator) ≈ 16–32 bytes
- **Operations**: Atomic increment/decrement of reference count (thread-safe but has overhead)

### `make_shared` vs Direct Construction

```cpp
// Recommended: single memory allocation (object and control block together)
auto p = std::make_shared<Widget>(args...);

// Two memory allocations (one for object, one for control block)
std::shared_ptr<Widget> p(new Widget(args...));
```

## `std::weak_ptr`

`weak_ptr` observes objects managed by `shared_ptr` without increasing the reference count; used to break circular references.

```cpp
struct Node {
    std::shared_ptr<Node> next;
    std::weak_ptr<Node> prev;  // use weak_ptr to break the cycle
};

auto n1 = std::make_shared<Node>();
auto n2 = std::make_shared<Node>();
n1->next = n2;
n2->prev = n1;  // does not increase n1's reference count

// Must check if still alive before use
if (auto sp = n2->prev.lock()) {
    // sp is a shared_ptr, n1 is guaranteed alive at this point
}
```

### `expired()` Check

```cpp
std::weak_ptr<int> wp;
{
    auto sp = std::make_shared<int>(42);
    wp = sp;
    assert(!wp.expired());  // still alive
}
assert(wp.expired());  // destroyed
```

## Circular Reference Problem

```cpp
// Classic mistake:
struct Parent {
    std::shared_ptr<Child> child;
};
struct Child {
    std::shared_ptr<Parent> parent;  // circular reference! Neither will be freed
};

// Correct:
struct Child {
    std::weak_ptr<Parent> parent;  // use weak_ptr
};
```

## Custom Deleters

```cpp
// unique_ptr's deleter is part of the type
std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(
    sqlite3_open("test.db"), &sqlite3_close);

// shared_ptr's deleter is not part of the type
std::shared_ptr<FILE> fp(fopen("test.txt", "r"), &fclose);
```

## Best Practices

| Scenario | Choice |
|----------|--------|
| Default | `unique_ptr` |
| Multiple owners | `shared_ptr` |
| Cache/observation | `weak_ptr` |
| Factory functions | Return `unique_ptr` (caller can implicitly convert to `shared_ptr`) |
| Existing raw pointer | Do not construct `shared_ptr` from a raw pointer; create with `make_shared` |
| Storing polymorphic objects in containers | `vector<unique_ptr<Base>>` |
