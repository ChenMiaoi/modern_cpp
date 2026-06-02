---
title: EASTL Intrusive Containers and Hashtable
topic: libraries
feature: eastl-intrusive
standard: N/A
status_checked_at: 2026-06-02
exercises: []
solutions: []
---
# EASTL Intrusive Containers and Hashtable

> Source path: `references/impl/EASTL/include/EASTL/intrusive_list.h`, `internal/hashtable.h`

## Intrusive Containers

Intrusive containers require elements to contain the linked list node themselves:

```cpp
struct Task : public eastl::intrusive_list_node {
    int id;
    std::string name;
};

eastl::intrusive_list<Task> task_list;
Task t1{1, "build"}, t2{2, "test"};
task_list.push_back(t1);  // zero allocation
task_list.push_back(t2);
```

Advantages: zero heap allocation, data and list pointers within the same object (cache-friendly), no additional node management needed.

## Hashtable

EASTL's hashtable uses open addressing + linear probing:

```cpp
// bucket structure in hashtable.h
struct node {
    node* next;      // chained overflow
    hash_node hash_data;  // key + value
};

// bucket array
node** mpBucketArray;  // pointer array
size_t mnBucketCount;
size_t mnElementCount;
```

EASTL's `hash_map` and `hash_set` support intrusive versions (`intrusive_hash_map`, `intrusive_hash_set`), where elements carry their own hash values and list pointers.

## Red-Black Tree

EASTL's ordered containers (`map`, `set`) are based on `red_black_tree` (`internal/red_black_tree.h`), with an implementation similar to the standard library but without exception handling.
