---
title: libstdc++ shared_ptr / RB-tree / function
topic: libraries
feature: shared-ptr-tree-function
standard: C++11
status_checked_at: 2026-06-02
implementation:
  libstdcxx:
    paths:
      - references/impl/gcc/libstdc++-v3/include/bits/shared_ptr_base.h
      - references/impl/gcc/libstdc++-v3/include/bits/stl_tree.h
      - references/impl/gcc/libstdc++-v3/include/bits/std_function.h
    symbols:
      - _Sp_counted_base
      - _Sp_counted_ptr_inplace
      - _Rb_tree
      - std::function
      - _Function_base
exercises:
  - exercises/cpp11-classes/smartptr2.cpp
  - exercises/cpp11-classes/smartptr1.cpp
solutions:
  - exercises/solutions/smartptr2.cpp
  - exercises/solutions/smartptr1.cpp
---
# libstdc++ shared_ptr / RB-tree / function

## std::shared_ptr Control Block Design

```cpp
template<typename _Lp>
class _Sp_counted_base {
  _Atomic_word  _M_use_count;    // strong reference count
  _Atomic_word  _M_weak_count;   // weak reference count
  virtual void _M_dispose() = 0;      // use_count → 0
  virtual void _M_destroy() = 0;      // weak_count → 0
};
```

### make_shared Single Allocation

```
  _Sp_counted_ptr_inplace<T, Alloc>
  ┌────────────────────────────────────────────────┐
  │ +0:  vptr                  (8B)                │
  │ +8:  _M_use_count  = 2    (4B)                │
  │ +12: _M_weak_count = 1    (4B)                │
  │ +16: _M_impl._M_alloc     (0B, empty allocator) │
  │ +16: _M_storage           (sizeof(T))          │
  └────────────────────────────────────────────────┘
  Single malloc, control block + object stored contiguously
```

Destruction sequence: when use_count → 0, only the element is destroyed (no deallocation); when weak_count → 0, the entire memory block is freed.

## _Rb_tree Red-Black Tree

### Sentinel Node _M_header

```
  _M_header (always marked red)
  ┌────────────┐
  │ _M_left ───┼──→ leftmost node (begin)
  │ _M_right ──┼──→ rightmost node (--end, O(1))
  │ _M_parent──┼──→ root node
  └────────────┘

  Empty tree: _M_left = _M_right = &header
  root._M_parent = header → used for end() determination
```

### Hint Insertion Optimization

```cpp
iterator insert_unique(iterator __position, const value_type& __v) {
  if (__position._M_node == _M_impl._M_header._M_left) {
    // hint = begin(): if new element < minimum, O(1) insert on the left
  } else if (__position._M_node == &_M_impl._M_header) {
    // hint = end(): if new element > maximum, O(1) insert on the right
  }
  return insert_unique(__v).first;  // fallback: O(log n)
}
```

## std::function: Function Pointer SBO

```cpp
class function<_Res(_ArgTypes...)> {
  typedef typename aligned_storage<3 * sizeof(void*)>::type _Any_data;
  _Any_data      _M_functor;   // callable storage (24B stack or heap)
  _Invoker_type  _M_invoker;   // dispatch function pointer
  void (*_M_manager)(_Any_data&, const _Any_data&, _Manager_operation);
};
```

**Differences from libc++**: libstdc++ uses function pointers instead of virtual functions, avoiding vtable indirect call overhead. On hot paths that call `std::function` frequently, there may be a slight performance difference.

## User API

The user-side entry points covered in this article are `std::shared_ptr`, the tree structure behind associative containers, and the callable wrapper API of `std::function`; the existing text has already directly laid out the control block, tree sentinel, and SBO skeleton.

## Standard Semantics

**`std::shared_ptr`**: Shared ownership model, where multiple `shared_ptr` instances can point to the same object. Internally maintains a two-phase reference count — `_M_use_count` (strong references) and `_M_weak_count` (weak references + 1 baseline), both implemented with thread safety via `__exchange_and_add_dispatch` atomic operations (`_S_atomic` policy). Supports custom deleters (deleter and object pointer stored separately, requiring two allocations). The aliasing constructor allows deriving a new `shared_ptr` pointing to a sub-object from an existing `shared_ptr`, sharing the same control block but holding a different raw pointer.

**`std::set` / `std::map`**: Ordered associative containers whose underlying data structure is an `_Rb_tree` red-black tree. The `value_type` of `set` is the `key_type` (unique keys); `multiset` allows duplicate keys. Elements are stored sorted by `_Compare` (default `std::less<Key>`). Provides bidirectional iterators (`bidirectional_iterator_tag`); `begin()` points to `_M_header._M_left` (leftmost node); `end()` points to `_M_header` itself. Insertion operations do not invalidate existing iterators or references (guaranteed by standard §[associative.reqmts] node stability).

**`std::function`**: Type-erased callable object wrapper that can store function pointers, lambdas, `std::bind` expressions, and other arbitrary `Callable` types. Supports `bool` emptiness check (`operator bool`); `operator()` throws `std::bad_function_call` when empty. Unlike C++23 `move_only_function`, `std::function` requires the target to be copyable, so `sizeof(std::function)` is typically 3 pointer sizes (`_Any_data` + `_M_invoker` + `_M_manager`). libstdc++ uses function pointers rather than virtual functions for dispatch, avoiding vtable indirect overhead.

## Object Layout

The above has already covered the `shared_ptr` control block, `_Rb_tree` header node, and `std::function` SBO storage; an "object body / control block / indirection layer" overview diagram will be added later.

## Core Source Paths

Source code tracing paths from user API to concrete implementation:

| User API | Public Header | Internal Implementation Header | Key Entry Point |
|----------|-----------|-----------|---------|
| `std::shared_ptr<T>` | `<memory>` → `shared_ptr.h` | `bits/shared_ptr_base.h` | `_Sp_counted_base` → `_M_release()` / `_M_dispose()` |
| `std::make_shared<T>` | `<memory>` → `shared_ptr.h` | `bits/shared_ptr_base.h` | `_Sp_counted_ptr_inplace::_M_get_deleter()` |
| `std::set<K>` / `std::map<K,V>` | `<set>` / `<map>` | `bits/stl_set.h` / `bits/stl_map.h` → `bits/stl_tree.h` | `_Rb_tree::_M_insert_unique()` / `_M_insert_()` |
| `std::function<Sig>` | `<functional>` | `bits/std_function.h` | `_Function_base::_Base_manager` → `_M_manager()` / `_M_invoke()` |

`shared_ptr_base.h` (approximately 2400 lines) contains the complete control block hierarchy: `_Sp_counted_base` → `_Sp_counted_ptr` (two-allocation path) and `_Sp_counted_ptr_inplace` (`make_shared` path). `stl_tree.h` (approximately 3500 lines) implements all `_Rb_tree` red-black tree logic; `stl_set.h` / `stl_map.h` are merely thin wrapper layers that forward container operations to `_Rb_tree` members. `std_function.h` (approximately 800 lines) contains `_Function_base` (SBO storage + manager), `_Function_handler` (type-erased invoker), and the `function` template class's `operator()` / assignment implementations.

## Core Classes / Functions

### shared_ptr Control Block Hierarchy

- **`_Sp_counted_base<_Lp>`** (`shared_ptr_base.h`): Abstract reference-counted base class, holding `_M_use_count` and `_M_weak_count` (both `_Atomic_word`). Declares three pure virtual / virtual functions: `_M_dispose()` (releases managed resource when use_count reaches zero), `_M_destroy()` (calls `delete this` to free the control block itself when weak_count reaches zero), `_M_get_deleter()` (returns a type-erased deleter pointer). The `_Lp` template parameter determines the locking policy: `_S_atomic` (default, uses `__exchange_and_add_dispatch`), `_S_mutex` (mutex fallback), `_S_single` (single-threaded, no atomic operations).
- **`_Sp_counted_ptr_inplace<_Ptr, _Alloc>`**: Control block used by `make_shared`, storing the object inside the control block's `_M_storage` member (single allocation). `_M_dispose()` calls the allocator's `destroy()` to destruct the object; `_M_destroy()` calls the allocator's `deallocate()` to free the entire memory block.
- **`_Sp_counted_ptr<_Ptr>`**: Control block used by `shared_ptr(new T(...))`, holding only a raw pointer; the object requires independent allocation (two allocations). `_M_dispose()` directly calls `delete _M_ptr` or invokes the custom deleter.

### RB-tree Core

- **`_Rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>`** (`stl_tree.h`): Complete red-black tree implementation; all `set`/`map`/`multiset`/`multimap` embed this class. The template parameter `_KeyOfValue` is a functor that extracts the key from `_Val` (identity for `set`, `select1st` for `map`).
- **`_Rb_tree_node<_Val>`**: Inherits from `_Rb_tree_node_base`, with an additional `_M_storage` (`__aligned_membuf<_Val>`) storing the actual value. Node layout: `_M_color`(1B + padding) + `_M_parent`(8B) + `_M_left`(8B) + `_M_right`(8B) + `_M_storage`.
- **`_Rb_tree::_M_header`** (`_Rb_tree_header`): Sentinel node; `_M_parent` points to the root node, `_M_left` points to the leftmost node (`begin()`), `_M_right` points to the rightmost node. In an empty tree, `_M_left = _M_right = &_M_header`. The `end()` iterator points to this sentinel.

### std::function Core

- **`_Function_base`** (`std_function.h`): Holds `_Any_data _M_functor` (SBO buffer, `sizeof(_Nocopy_types)` = 3 × `sizeof(void*)` = 24 bytes) and `_Manager_type _M_manager` (function pointer managing copy/destroy/type-query operations). The destructor releases the target via `_M_manager(..., __destroy_functor)`.
- **`_Function_handler<_Res(_ArgTypes...), _Functor>`**: Inherits from `_Function_base::_Base_manager<_Functor>`, providing a static `_M_invoke()` method — calls the stored callable via `std::__invoke_r<_Res>`. `_M_manager()` handles `__clone_functor` (copy) and `__destroy_functor` (destruct) operations.
- **`_M_invoker`**: Function pointer in the `function` class, pointing to a specialized instance of `_Function_handler::_M_invoke`. `operator()` directly calls `_M_invoker(_M_functor, args...)`, with no virtual function overhead.

## Key Algorithms

### shared_ptr Reference Count Lifecycle

1. **Construction**: `_M_use_count = 1`, `_M_weak_count = 1` (weak reference baseline, ensures the control block is not freed before use_count reaches zero).
2. **Copy `_M_add_ref_copy()`**: `__exchange_and_add_dispatch(&_M_use_count, 1)` — atomic increment, returns old value for overflow checking (`_S_chk`).
3. **Destruction `_M_release()`**: `__exchange_and_add_dispatch(&_M_use_count, -1)` — atomic decrement; if old value == 1 (i.e., drops to zero after decrement), calls `_M_release_last_use()`.
4. **`_M_release_last_use()`**: First calls `_M_dispose()` to destruct the managed object; then atomically decrements `_M_weak_count` by 1 (cancelling the baseline); if old value == 1 (i.e., weak references also reach zero), calls `_M_destroy()` to free the control block memory.
5. **`weak_ptr` lock `_M_add_ref_lock()`**: CAS loop checks that `_M_use_count > 0` then atomically increments, otherwise throws `bad_weak_ptr`.

### make_shared Single Allocation Path

`make_shared<T>(args...)` → allocates memory of size `sizeof(_Sp_counted_ptr_inplace<T>)` → placement-new constructs the control block + object. `_M_dispose()` destructs the object without freeing memory; `_M_destroy()` frees the entire block via the allocator. Compared to `shared_ptr(new T(...))`: first `new T` then `new _Sp_counted_ptr<T>`, two independent allocations that cannot exploit locality and incur extra malloc overhead.

### RB-tree Insertion and Rebalancing

`_M_insert_unique(__v)` flow: start from the root performing BST search to find the insertion position (O(log n)) → `_M_create_node()` allocates and constructs the node → sets parent/child pointers → if the tree is non-empty, attaches the node to the correct position → `_Rb_tree_insert_and_rebalance()` rebalances. Rebalancing rules: the new node is colored red; if the parent is also red, it violates the red-black property and must be fixed through rotations (left rotation / right rotation) and recoloring. At most 2 rotations are needed to restore balance.

Hint insertion (`insert_unique(iterator __position, __v)`): if the hint points to `begin()` and `__v < *begin()`, or the hint points to `end()` and `__v > *--end()`, an O(1) direct insertion is possible. Otherwise it falls back to the hint-free O(log n) path.

### std::function Dispatch Path

On assignment, `_M_init_functor()` determines the storage strategy: if `sizeof(_Functor) <= _M_max_size` and `alignof(_Functor) <= _M_max_align` and `is_trivially_copyable` is satisfied (`__is_location_invariant`), then `_M_create(__dest, __f, true_type)` — placement-new into the SBO buffer. Otherwise `_M_create(__dest, __f, false_type)` — heap-allocates via `new _Functor`, and only a pointer is stored in the SBO. On invocation, `_M_invoker` directly retrieves the object from the SBO (or dereferences the pointer) and calls `__invoke_r`.

## ABI Constraints

- **Control block virtual function layout**: `_Sp_counted_base` has four virtual functions: the virtual destructor, `_M_dispose()`, `_M_destroy()`, `_M_get_deleter()`; each control block instance carries a vtable pointer (8B on x86-64). The offset order of virtual functions in the vtable is ABI-fixed — adding, removing, or reordering any virtual function causes cross-DSO ODR violations and crashes. `_Sp_counted_ptr` and `_Sp_counted_ptr_inplace` as derived classes each have their own vtable.
- **`std::function` SBO size**: `_Any_data` is defined as `aligned_storage<3 * sizeof(void*)>`, which is 24 bytes on LP64. The actual SBO threshold `_M_max_size = sizeof(_Nocopy_types)` is also 24 bytes. This means `sizeof(std::function)` = 24 (SBO buffer) + 8 (`_M_invoker`) + 8 (`_M_manager`) = 40 bytes. Increasing the SBO buffer would change `sizeof(std::function)`, breaking ABI compatibility — all compiled `std::function` instance offsets would be misaligned.
- **RB-tree node layout**: The field order of `_Rb_tree_node_base` (`_M_color` / `_M_parent` / `_M_left` / `_M_right`) and alignment are ABI-fixed. `_Rb_tree_node<_Val>` inherits `_Rb_tree_node_base` and appends `_M_storage`. Any field reordering or type change breaks node offset compatibility with already-compiled code. The layout of `_Rb_tree_header` (`_M_header` + `_M_node_count`) is similarly fixed — iterators access node fields directly through `_M_node` pointers, with compile-time offsets hardcoded.
- **`_Lock_policy` template parameter**: The control block's `_Lp` parameter (`_S_atomic` / `_S_mutex` / `_S_single`) affects the choice of atomic operations. The default `__default_lock_policy = _S_atomic`, but on certain embedded platforms it may be `_S_single`. Control block instances with different policies have different vtables; mixing them causes UB.

## Exception Safety

- **`make_shared`**: Strong exception guarantee. Single memory allocation + placement-new constructs the object; if `T`'s constructor throws, `_Sp_counted_ptr_inplace`'s constructor frees the allocated memory via the allocator without leaking. Compared to the two-allocation path of `shared_ptr(new T(...))` — if the second allocation (control block) fails, the already-constructed `T` is properly destructed by the custom deleter (or `delete`), but overall only a basic guarantee is provided.
- **`shared_ptr` copy**: `noexcept`. `_M_add_ref_copy()` is only an atomic increment, allocates no memory, calls no user code, and throws no exceptions.
- **RB-tree insertion**: Strong exception guarantee. `_M_create_node()` first allocates node memory then constructs the value (`_M_construct_node()`); if value construction throws, the catch block frees the node memory via `_M_put_node()` and rethrows, leaving the tree state unchanged. The rebalancing phase (`_Rb_tree_insert_and_rebalance()`) only manipulates pointers and enum values and cannot throw.
- **`std::function` assignment**: Basic exception guarantee. If the copy constructor of `_Functor` throws during `_M_init_functor()`, the `function` object becomes empty (`_M_manager == nullptr`), and the previous callable object has already been destroyed via `__destroy_functor`. Does not satisfy the strong guarantee because assignment cannot roll back to the previous callable (the old object has been destroyed). C++23's `std::move_only_function` likewise only provides the basic guarantee.

## Iterator / Reference Invalidation

- **`shared_ptr`**: Does not involve the iterator concept. Copy, assignment, `reset()`, and other operations on a `shared_ptr` are completely safe with respect to other `shared_ptr` / `weak_ptr` instances sharing the same control block. The `shared_ptr` returned by `weak_ptr::lock()` is always valid (if the object is still alive). Note: `shared_ptr` itself is not thread-safe — concurrent read/write on the same `shared_ptr` instance requires external synchronization; but different `shared_ptr` instances pointing to the same control block are individually thread-safe (guaranteed by atomic reference counting).
- **RB-tree**: **Insertion (`insert` / `emplace`) does not invalidate any existing iterators or references** — new nodes are attached as leaf nodes without moving existing nodes. **Deletion (`erase`) only invalidates iterators pointing to the erased element** — in the libstdc++ implementation, deleting a node with two children uses successor relink (linking the successor node to the deleted position) rather than copy, so iterators to other nodes remain valid (required by standard §[associative.reqmts]/8). **Whole-container operations**: `clear()` invalidates all iterators; `swap()` does not invalidate iterators (iterators follow the nodes).
- **`std::function`**: `std::function` does not expose iterators. When replacing the target callable via `operator=` or `swap()`, external references that previously captured by reference or pointed to the old target's internal state will become dangling — the old object has been destructed by `_M_destroy(..., __destroy_functor)`. When `std::function` is copied, `__clone_functor` creates an independent copy of the target, so the internal states of two `function` instances do not affect each other.

## Performance Model

- **`shared_ptr` atomic reference counting**: Copy invokes `_M_add_ref_copy()` which executes `__exchange_and_add_dispatch` (compiles to `lock xadd` on x86), with memory order `memory_order_acq_rel` (implicit in `__exchange_and_add_dispatch`). Destruction invokes `_M_release()` with the same `lock xadd`; upon reaching zero it enters the cold path (`__attribute__((noinline))` marked `_M_release_last_use_cold()`), calling `_M_dispose()` + possibly `_M_destroy()`. Atomic operation overhead: approximately 5-20ns under uncontended scenarios (x86), potentially degrading to cache line bouncing (MESI protocol) under contention, becoming a bottleneck in multi-core high-concurrency scenarios. The CAS loop in `weak_ptr::lock()` adds additional overhead under heavy contention.
- **RB-tree pointer chasing**: Tree operations (`find` / `insert` / `erase`) have O(log n) time complexity, but each comparison requires following `_M_left` or `_M_right` pointers from parent to child node, causing cache misses. On modern CPUs, a single cache miss costs approximately 50-100ns, so for a tree with n = 1000 (depth ~10), a single lookup may trigger 10 cache misses, far exceeding flat array sequential access. This is also why `std::flat_map` (C++23) may outperform `std::map` on small datasets.
- **`std::function` SBO and indirect calls**: libstdc++'s SBO eligibility criteria are `sizeof(_Functor) <= 24 && alignof(_Functor) <= alignof(void*) && is_trivially_copyable`. When satisfied, heap allocation is avoided (saving `new` + `delete` overhead, approximately 50-200ns). Call path: `operator()` → `_M_invoker(_M_functor, args...)` → `__invoke_r`. Although it goes through a function pointer indirect call (one indirect jump, possibly triggering branch misprediction), it has one fewer pointer dereference than a virtual function call (vtable lookup). Typical function pointers / small lambdas have a near-100% SBO hit rate.

## libstdc++ vs libc++ vs MSVC

| Dimension | libstdc++ (GCC) | libc++ (Clang) | MSVC STL |
|------|----------------|---------------|----------|
| **Control block base class** | `_Sp_counted_base<_Lp>`, with `_M_dispose()` / `_M_destroy()` virtual functions | `__shared_count` / `__shared_weak_count`, similar virtual function interface but different class names | `_Ref_count_base`, virtual function interface similar to libstdc++ |
| **make_shared path** | `_Sp_counted_ptr_inplace`, single allocation | `__shared_ptr_emplace`, single allocation | `_Ref_count_obj`, single allocation |
| **Atomic operations** | `__exchange_and_add_dispatch` → `__atomic_fetch_add` | `__libcpp_atomic_refcount_*` → `__c11_atomic_*` | `_Atomic_*` or compiler builtins |
| **Lock policy template parameter** | `_Lock_policy` (`_S_atomic` / `_S_mutex` / `_S_single`) | No template parameter, always atomic | No template parameter, always atomic |
| **RB-tree node layout** | `_Rb_tree_node_base`: `color + parent + left + right`, node inherits then appends `_M_storage` | `__tree_node_base` / `__tree_end_node`: `__left_ + __right_ + __parent_ + __is_black_` (different field order) | `_Tree_node`: `_Color + _Parent + _Left + _Right + _Myval` |
| **`std::function` SBO size** | 24B (`3 * sizeof(void*)`), `sizeof(std::function)` = 40B | 24B (`3 * sizeof(void*)`), but `sizeof(std::function)` = 32B (different layout) | ≥ 6 * `sizeof(void*)` (implementation-dependent), `sizeof(std::function)` is larger |
| **`std::function` dispatch method** | Function pointer (`_M_invoker` + `_M_manager`) | Function pointer (`__f_` + `__buf_`) | Function pointer or internal vtable (implementation-dependent) |
| **Virtual functions vs function pointers** | shared_ptr uses virtual functions, function uses function pointers | shared_ptr uses virtual functions, function uses function pointers | shared_ptr uses virtual functions, function uses function pointers |

**Key differences summary**: All three implementations' `shared_ptr` control blocks use virtual functions for polymorphism, but class names and lock policy parameters differ. RB-tree node field orders vary (libstdc++ has color first, libc++ has `__is_black_` last), so cross-library node pointer casts cause UB. `std::function` SBO buffer sizes differ the most — libstdc++'s 24B can accommodate most lambdas, while MSVC's larger buffer accommodates more scenarios, at the cost of a larger `sizeof(std::function)`.

## Minimal Reproduction Code

```cpp
#include <functional>
#include <memory>
#include <set>

int main() {
  auto p = std::make_shared<int>(42);
  std::set<int> s{3, 1, 2};
  std::function<int(int)> f = [keep = p](int x) { return x + *keep; };
  return f(*s.begin());
}
```

## Compile / Disassemble / Benchmark Evidence

### shared_ptr Atomic Operation Assembly

```bash
g++ -std=c++20 -O2 -S shared_ptr_copy.cpp -o shared_ptr_copy.s
# observe _M_add_ref_copy compiles to lock xadd instruction
```

Core assembly for `shared_ptr` copy (x86-64):

```asm
; _M_add_ref_copy — atomic increment
lock add    DWORD PTR [rax], 1   ; or lock xadd
```

Destruction path `_M_release()`:

```asm
; _M_release — atomic decrement and check for zero
lock sub    DWORD PTR [rax], 1
jne         .Lskip_dispose       ; skip if not zero
call        _M_release_last_use_cold  ; cold path, noinline
```

### RB-tree Insertion Hot Path

```bash
g++ -std=c++20 -O2 -S rbtree_insert.cpp -o rbtree_insert.s
# observe BST search + _Rb_tree_insert_and_rebalance call
```

BST search portion of `_M_insert_unique`:

```asm
; starting from root, descend along left/right pointers
.Lsearch_loop:
    mov     rdx, [rax+16]       ; load _M_left
    mov     rcx, [rax+24]       ; load _M_right
    cmp     edi, [rax+32]       ; compare key
    jge     .Lgo_right
    mov     rax, rdx            ; go to left subtree
    jmp     .Lsearch_loop
```

### std::function SBO vs Heap Allocation

```bash
g++ -std=c++20 -O2 -S function_call.cpp -o function_call.s
# compare SBO path (direct call) vs heap path (one extra pointer dereference)
```

SBO path (target lambda ≤ 24B):

```asm
; _M_invoker directly calls the callable object in SBO
lea     rdi, [rbx+8]          ; &_M_functor._M_pod_data
call    [rbx+32]              ; call via _M_invoker function pointer
```

Heap path (target lambda > 24B):

```asm
; must first extract heap pointer from SBO then dereference
mov     rdi, [rbx+8]          ; load heap pointer from _M_functor
call    [rbx+32]              ; call via _M_invoker function pointer
```

### weak_ptr Lock Path

```bash
objdump -d a.out | grep -A20 '_M_add_ref_lock_nothrow'
```

Key: CAS loop checks `_M_use_count > 0` then atomically increments; on failure returns false (triggering `bad_weak_ptr`).

### Benchmark Tips

```bash
# shared_ptr copy throughput (single-threaded vs multi-threaded contention)
g++ -std=c++20 -O2 -pthread bench_shared_ptr.cpp -o bench
./bench --benchmark_filter="BM_SharedPtr"

# RB-tree vs flat_map lookup (cache effects)
g++ -std=c++20 -O2 bench_tree.cpp -o bench
./bench --benchmark_filter="BM_TreeLookup"

# std::function SBO hit rate
g++ -std=c++20 -O2 bench_function.cpp -o bench
./bench --benchmark_filter="BM_Function"
```

## cpplings Exercise Entry Points

- [`smartptr2` — shared_ptr and weak_ptr](../../../exercises/cpp11-classes/smartptr2.cpp)
- [`smartptr1` — unique_ptr](../../../exercises/cpp11-classes/smartptr1.cpp)
- [`movonlyfunc1` — move_only_function move-only callable wrapper](../../../exercises/cpp23/movonlyfunc1.cpp)
