---
title: EASTL fixed_* Containers
topic: libraries
feature: eastl-fixed
standard: N/A
status_checked_at: 2026-06-02
exercises: []
solutions: []
---
# EASTL fixed_* Containers: Zero Heap Allocation Deterministic Containers

> Source path: `references/impl/EASTL/include/EASTL/fixed_vector.h`, `fixed_string.h`

## fixed_vector Memory Layout

```
fixed_vector<T, nodeCount, bEnableOverflow> object memory layout:

+----------------------------------------------------------+
| fixed_vector object (sizeof includes the entire inline buffer) |
|                                                          |
|  VectorBase<T, fixed_allocator>                          |
|    T* mpBegin       ─────────────────────────┐           |
|    T* mpEnd         ─────────────────────┐   │           |
|    T* capacityPtr   (= mpBegin + N)      │   │           |
|                                           |   |           |
|  aligned_buffer<N * sizeof(T), alignof(T)>|   |           |
|    +-------+-------+-------+-----+--------+              |
|    | T[0]  | T[1]  | T[2]  | ... | T[N-1] |             |
|    +-------+-------+-------+-----+--------+              |
|    ^mpBegin  ^mpEnd (equal initially)                     |
+----------------------------------------------------------+

push_back() path decision:

            size() < nodeCount ?
                  |
           +------+------+
           |             |
         [yes]         [no]
           |             |
           v             v
 Write mBuffer      bEnableOverflow == true ?
 (inline buffer)          |
     O(1)        +------+------+
                 |             |
               [true]       [false]
                 |             |
                 v             v
        OverflowAllocator   dummy_allocator
        (default EASTLAlloc) (returns NULL)
        heap overflow space  -> undefined behavior

Two typical usage patterns:
  fixed_vector<int, 64, false> v;   // pure stack mode, exceed 64 -> assert/UB
  fixed_vector<int, 64, true>  v;   // hybrid mode, first 64 on stack, then heap
```

## Template Parameters

| Parameter | Semantics |
|-----------|-----------|
| `T` | Element type |
| `nodeCount` | Inline storage capacity |
| `bEnableOverflow` | Whether to fall back to heap allocation when capacity is exhausted |
| `OverflowAllocator` | Allocator type used for overflow |

## Construction Process

```cpp
fixed_vector()
    : base_type(fixed_allocator_type(mBuffer.buffer))
{
    mpBegin = mpEnd = (value_type*)&mBuffer.buffer[0];
    internalCapacityPtr() = mpBegin + nodeCount;
}
```

`mpBegin` points to `mBuffer.buffer` — this is memory on the stack, not the heap. Capacity is fixed at construction time.

## Move Construction Special Case

Move construction **cannot perform pointer swapping** (the inline buffer is part of the object), so it must move elements one by one into its own `mBuffer`. This is slower than a normal vector move construction.
