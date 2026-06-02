---
title: "Boost 内存管理"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# Boost Memory Management

## SmartPtr

`boost::shared_ptr`, `boost::weak_ptr`, `boost::scoped_ptr`, `boost::intrusive_ptr` — these are the direct predecessors of the C++11 smart pointers.

### intrusive_ptr

Unlike `shared_ptr`, `intrusive_ptr` requires the managed object itself to maintain the reference count:

```cpp
struct MyObject {
    std::atomic<int> refcount{0};
    // ...
};

void intrusive_ptr_add_ref(MyObject* p) { p->refcount.fetch_add(1); }
void intrusive_ptr_release(MyObject* p) {
    if (p->refcount.fetch_sub(1) == 1) delete p;
}

boost::intrusive_ptr<MyObject> ptr(new MyObject);
```

**Advantage**: No separate control block allocation required (one fewer `malloc` compared to `shared_ptr`).

## Pool: Memory Pool

Boost.Pool provides two pool allocators:

- **`pool<>`**: A memory pool of fixed-size blocks. O(1) allocation/deallocation, no fragmentation.
- **`object_pool<T>`**: An object pool that bulk-constructs and bulk-destroys objects of the same type.

```cpp
boost::pool<> alloc(sizeof(int));  // pool of 4-byte blocks
int* p = static_cast<int*>(alloc.malloc());  // allocate from pool
alloc.free(p);  // return to pool
```

## Align: Aligned Memory

`boost::alignment::aligned_alloc` provides aligned memory allocation — a cross-platform solution that predates C++17 `std::aligned_alloc`.
