---
title: "Standard Library Idioms"
topic: unknown
feature: stdlib-idioms
standard: N/A
status_checked_at: 2026-06-02
---
# Standard Library Idioms

## CPO (Customization Point Object)

A function object in the standard library that discovers user-defined implementations via ADL:

```cpp
// std::ranges::begin is a CPO
// when calling ranges::begin(v):
// 1. if v has a member begin() → call v.begin()
// 2. otherwise find begin(v) via ADL
// 3. otherwise fall back to the default implementation

namespace MyLib {
  struct Container { ... };
  auto begin(Container& c) { return c.data(); }  // discovered via ADL
}

MyLib::Container c;
auto it = std::ranges::begin(c);  // found MyLib::begin via ADL
```

## Niebloid

A form of CPO implementation — a global function object that works through the ADL mechanism but is not itself a function:

```cpp
// ranges::begin is a niebloid (named after Eric Niebler)
namespace std::ranges {
  inline constexpr __begin_fn begin{};  // global constexpr function object
}
```

## Tag Invoke (C++20 Ranges Customization Mechanism)

```cpp
// recommended way for user types to customize ranges::begin
template<typename T>
auto tag_invoke(ranges::begin_t, MyContainer<T>& c) {
  return my_iterator<T>(c.data());
}
```

## Range / View / Pipe Operator

- **Range**: Anything that has `begin()` and `end()`
- **View**: A lightweight range adapter — O(1) copy, O(1) default construction, non-owning
- **Pipe Operator**: The `|` operator passes a range to a view adapter

See the [range-v3 pipe operator chapter](/libraries/range-v3/pipe-operator) for details.

## Sentinel

`end()` does not have to be an iterator — it can be a "sentinel," any type comparable with an iterator:

```cpp
// sentinel for null-terminated strings
struct null_sentinel {
  bool operator==(const char* p) const { return *p == '\0'; }
};

// usage
const char* s = "hello";
auto range = ranges::subrange(s, null_sentinel{});
```

## PMR (Polymorphic Memory Resource, C++17)

A runtime-polymorphic memory allocator — dispatched via virtual functions rather than template parameters:

```cpp
std::pmr::monotonic_buffer_resource pool(buf, sizeof(buf));
std::pmr::vector<int> v(&pool);  // uses pool for memory allocation
v.push_back(42);                  // allocates from pool, not from global new
```

## Allocator Model

The standard library allocator is type-bound — `vector<int, MyAlloc>` and `vector<double, MyAlloc>` are different types. This makes it difficult to switch allocation strategies at runtime. PMR and EASTL's non-template allocators are two solutions to this problem.

## Smart Pointer Idioms

```cpp
// unique_ptr: exclusive ownership, zero overhead (8 bytes)
auto p = std::make_unique<Widget>(args...);

// shared_ptr: shared ownership, reference counting (16 bytes)
auto p = std::make_shared<Widget>(args...);  // single allocation

// weak_ptr: does not own the object, can check if the object is still alive
std::weak_ptr<Widget> wp = sp;
if (auto locked = wp.lock()) { /* object is still alive */ }
```

## RAII Handle

The standard library makes extensive use of RAII to wrap operating system resources:

```cpp
std::lock_guard<std::mutex> lk(mtx);    // mutex lock
std::unique_ptr<File> fp(open(...));     // file handle
std::jthread worker(do_work);            // thread
```
