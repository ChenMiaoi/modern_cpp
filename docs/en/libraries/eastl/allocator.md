---
title: EASTL Allocator Model
topic: libraries
feature: eastl-allocator
standard: N/A
status_checked_at: 2026-06-02
exercises: []
solutions: []
---
# EASTL Allocator Model: Non-Template, Instantiation Design

> Source path: `references/impl/EASTL/include/EASTL/internal/config.h`, `allocator.h`

## Why a New Allocator Is Needed

The fundamental problem with the standard allocator `std::allocator<T>` is that it is **type-bound and stateless**:

```cpp
template <typename T>
class allocator {
public:
    T* allocate(size_t n);         // calls ::operator new
    void deallocate(T* p, size_t n);
};
```

Game engines need: per-subsystem allocation tagging (rendering, physics, AI), alignment control (SIMD), allocation tracking, runtime switching of allocation strategies. The standard allocator's type-bound design makes all of these difficult to implement.

## EASTL Allocator: Non-Template Instance

```cpp
class EASTL_API allocator
{
public:
    EASTL_ALLOCATOR_EXPLICIT allocator(
        const char* pName = EASTL_NAME_VAL(EASTL_ALLOCATOR_DEFAULT_NAME));
    allocator(const allocator& x);
    allocator(const allocator& x, const char* pName);
    allocator& operator=(const allocator& x);

    void* allocate(size_t n, int flags = 0);
    void* allocate(size_t n, size_t alignment, size_t offset, int flags = 0);
    void  deallocate(void* p, size_t n);

    const char* get_name() const;
    void        set_name(const char* pName);

protected:
#if EASTL_NAME_ENABLED
    const char* mpName;   // debug name
#endif
};
```

### Key Design Differences

**1. Not type-bound**: Returns `void*`; containers cast it themselves.

**2. Dual allocate overloads**:
- `allocate(n, flags)` — basic allocation, flags convey semantics (`MEM_TEMP = 0`, `MEM_PERM = 1`)
- `allocate(n, alignment, offset, flags)` — aligned allocation

**3. All instances compare equal**:

```cpp
inline bool operator==(const allocator&, const allocator&)
{
    return true;  // all allocators are treated as equal
}
```

This means container swaps don't need allocator compatibility checks — just swap the three pointers directly.

**4. dummy_allocator**: A no-op allocator where all `allocate()` calls return NULL. Used for fixed containers with `bEnableOverflow = false`.

## Container Usage Pattern

```cpp
eastl::allocator gameAlloc("GameHeap");
eastl::vector<RenderCommand> commands(gameAlloc);

eastl::allocator physAlloc("PhysicsHeap");
eastl::vector<RigidBody> bodies(physAlloc);
```

`VectorBase` uses `compressed_pair<T*, allocator_type>` to store the capacity pointer and allocator instance, saving one pointer's worth of space when the allocator is empty.
