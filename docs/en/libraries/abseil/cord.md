---
title: Abseil Cord
topic: libraries
feature: cord
standard: N/A
status_checked_at: 2026-06-02
implementation:
  abseil:
    paths:
      - references/impl/abseil-cpp/absl/strings/cord.h
      - references/impl/abseil-cpp/absl/strings/cord_internal.h
    symbols:
      - Cord
      - CordRep
      - CordRepConcat
      - CordRepLeaf
exercises: []
solutions: []
---
# Abseil Cord: B-tree Buffer for Large Text

> Source path: `references/impl/abseil-cpp/absl/strings/cord.h`, `cord_internal.h`

`absl::Cord` is Google's string type designed for **large text concatenation** scenarios. Unlike `std::string`, `Cord` does not require character data to be stored contiguously—it uses a B-tree structure to split text into multiple chunks, where concatenation only modifies the tree structure without copying data.

## Core Problem: Large String Concatenation

`std::string`'s performance issues with repeated `append`:

```
string s;
s += chunk1;  // Allocates 2x capacity, copies
s += chunk2;  // May trigger expansion, copies all
s += chunk3;  // Same as above
...
s += chunkN;  // O(N²) total copies
```

For scenarios like log aggregation, protobuf serialization, HTTP body concatenation (frequent concatenation of large blocks), `std::string`'s O(n²) complexity is unacceptable.

## Cord's B-tree Structure

```
  Cord internal tree structure illustration:

       ┌────────────────────┐
       │  CordRep Concat    │    Root node
       │  (left, right)     │
       └─────────┬──────────┘
          ┌──────┴──────┐
          ▼             ▼
  ┌──────────────┐  ┌──────────────┐
  │ CordRep Leaf │  │ CordRep Concat│
  │ "Hello, "    │  │ (left, right) │
  └──────────────┘  └───────┬──────┘
                      ┌─────┴─────┐
                      ▼           ▼
              ┌──────────────┐ ┌──────────────┐
              │ CordRep Leaf │ │ CordRep External│
              │ "World"      │ │ (ref to mmap)   │
              └──────────────┘ └──────────────┘

  CordRep types:
  - Leaf:       Inline small data (up to 12 bytes inside the Rep)
  - External:   Points to external memory (mmap, protobuf buffer, etc.)
  - Concat:     Binary concatenation node (left + right)
  - Substring:  References a sub-range of another Rep (zero-copy slice)
  - Rope:       Balanced tree with multiple children (auto-converted from Concat)
```

## CordRep Core Design

```cpp
struct CordRep {
  // Reference count (atomic)
  std::atomic<size_t> refcount;
  
  // Data length
  size_t length;
  
  // Type tag
  enum Type { EXTERNAL, CONCAT, SUBSTRING, LEAF, ROPE };
  Type tag;
  
  union {
    struct { char data[12]; } leaf;          // Inline small data
    struct { void* data; FreeFunc free; } ext;  // External storage
    struct { CordRep* left; CordRep* right; } concat;  // Concatenation
    struct { CordRep* child; size_t offset; size_t length; } sub;  // Substring
  };
};
```

**Key design**: Cord's concatenation is O(1)—just create a Concat node pointing to the left and right subtrees. No data copying.

## Zero-Copy Concatenation

```cpp
absl::Cord a("Hello, ");
absl::Cord b("World!");
absl::Cord c = a + b;  // O(1): creates Concat node, a and b unchanged
```

Concatenation operation:
1. Create a new `CordRep Concat` node
2. `left = a.tree()`, `right = b.tree()`
3. `refcount = 1`, `length = a.length() + b.length()`
4. No character data is copied

## Flat Representation (Small String Optimization)

When Cord's data is very small, it degrades to a flat representation:

```cpp
struct CordRepInline {
  char data[kMaxInline];  // Typically 12-15 bytes
  size_t length;
};
```

Cords smaller than `kMaxInline` allocate no heap memory at all.

## External Storage References

Cord can reference external memory, zero-copy:

```cpp
// Reference mmap'd file contents — no copy
absl::Cord FromMmap(const void* data, size_t len) {
  return absl::MakeCordFromExternal(
      absl::string_view(static_cast<const char*>(data), len),
      [data, len](absl::string_view) { munmap(const_cast<void*>(data), len); });
}
```

## Comparison with std::string

| Dimension | `std::string` | `absl::Cord` |
|-----------|--------------|-------------|
| Storage | Contiguous memory | B-tree (multiple chunks) |
| Concatenation | O(n²) cumulative | **O(1) per operation** |
| Random access | O(1) | O(log n) |
| Substring | O(n) copy | **O(1) zero-copy** |
| External references | Not supported | **Supported** |
| SSO | 15-22 bytes | 12-15 bytes |
| Use case | General small strings | Large text, log aggregation, protobuf |
