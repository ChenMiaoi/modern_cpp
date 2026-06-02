---
title: libc++ function and shared_ptr
topic: libraries
feature: function-shared-ptr
standard: C++11
status_checked_at: 2026-06-02
implementation:
  libcxx:
    paths:
      - references/impl/llvm-project/libcxx/include/__functional/function.h
      - references/impl/llvm-project/libcxx/include/__memory/shared_ptr.h
    symbols:
      - std::function
      - __value_func
      - __policy_func
      - std::shared_ptr
      - __shared_ptr_emplace
exercises: []
solutions: []
---
# libc++ function and shared_ptr

## std::function: 24-Byte SBO

### Data Structure

```
__value_func:
  __buf_ (24-byte stack buffer)     __f_ (pointer)
  ┌─────────────────────────┐  ┌──────┐
  │ callable object or unused│──→│ points to│
  └─────────────────────────┘  └──────┘
                               ↑ may point to __buf_ (stack) or heap
```

### SBO Determination

```
  sizeof(_Fun) ≤ 24 AND is_nothrow_copy_constructible ?
    YES → ::new(&__buf_) _Fun(move(lambda))  // construct on stack
    NO  → new _Fun(lambda)                    // heap allocate
```

### Move Construction Special Case

```
  src on stack? → cannot steal pointer, must clone (same as copy)
  src on heap?  → steal pointer directly, src.__f_ = nullptr

  Key insight: even for std::function's move construction, stack-based callables require a copy!
```

## std::shared_ptr: Control Block Layout

### make_shared Single Allocation

```
Memory layout of make_shared<int>(42):

  ┌──────────────────────────────────────┐
  │     __shared_ptr_emplace control block│
  │  vtable ptr (8B)                      │
  │  use_count  (atomic, 4B) = 2          │
  │  weak_count (atomic, 4B) = 1          │
  │  int __elem_ = 42 (4B)               │  ← immediately follows
  └──────────────────────────────────────┘
           ↑ one malloc, single contiguous memory block

  shared_ptr<int> sp:
  ┌──────────┬──────────┐
  │ _M_ptr ──┼──────────┼──→ __elem_ (42)
  │ _M_ctrl──┼──────────┼──→ control block start
  └──────────┴──────────┘
  sizeof(shared_ptr) = 16 bytes (two pointers)
```

### Two-Level Destruction

```
use_count → 0: __on_zero_shared()
  → destroys element only, does not release memory (weak_ptr still alive)

weak_count → 0: __on_zero_shared_weak()
  → releases entire block of memory for control block + element
```


## Standard Semantics

### `std::function`

- **Callable wrapper**: `std::function<Sig>` is a general-purpose polymorphic function wrapper that can store, copy, and invoke any callable target (lambda, function pointer, `std::bind` expression, function object) — as long as its signature is compatible with `Sig`
- **SBO (Small Buffer Optimization)**: The standard does not guarantee SBO, but all major implementations provide it. libc++'s threshold is 24 bytes (`3 × sizeof(void*)`), and the wrapped type must satisfy `is_nothrow_copy_constructible`
- **`operator()` semantics**: Calling `operator()` on an empty `function` throws `bad_function_call` (§[func.wrap.badcall]); argument validity is not checked
- **Target access**: `target<T*>()` returns a `T*` pointing to internal storage when RTTI is enabled; `target_type()` returns `type_info`
- **Allocator extension** (C++11–C++14, deprecated): `function(allocator_arg, alloc, f)` allows custom storage allocators; in libc++'s implementation this parameter is ignored, handled solely through `__value_func`'s SBO/heap paths

### `std::shared_ptr`

- **Shared ownership**: Multiple `shared_ptr` instances can share ownership of the same object; the last `shared_ptr` destroyed is responsible for deleting the object (§[util.smartptr.shared.general])
- **Control block**: Each `shared_ptr` internally holds two pointers `(element_ptr, control_block_ptr)` (16 bytes). The control block maintains `use_count` and `weak_count`, dispatching destruction logic through virtual functions
- **make_shared semantics**: `make_shared<T>(args)` guarantees a single memory allocation, merging the control block and element into contiguous memory; `shared_ptr<T>(new T(...))` requires at least two allocations (element + control block)
- **weak_ptr and locking**: `weak_ptr` does not affect `use_count`; `weak_ptr::lock()` atomically checks `use_count > 0` and creates a new `shared_ptr`, returning an empty `shared_ptr` on failure
- **enable_shared_from_this**: When a type inherits `enable_shared_from_this<T>` and is constructed via `shared_ptr(new T(...))` or `make_shared`, the internal `weak_ptr` is automatically set up so that `shared_from_this()` is available
- **Thread safety**: The standard guarantees that control block reference count operations (`__add_shared`, `__release_shared`, `__add_weak`, `__release_weak`) are atomic; copying/destroying `shared_ptr` objects themselves requires external synchronization (§[util.smartptr.shared.general])

## Core Source Paths

### std::function

| File | Content |
|------|---------|
| `__functional/function.h` | `__base` (virtual interface with `__clone`, `destroy`, `destroy_deallocate`, `operator()`, `target`, `target_type`); `__func` (template implementation storing `__func_` member); `__value_func` (3×sizeof(void*) SBO buffer + `__f_` pointer); `__policy_func` (`__policy_storage` union + function pointer table); `function` main class definition |
| `__type_traits/invoke.h` | `__invoke_r<_Rp>`: unified callable invocation entry point, handling function pointers, member pointers, function objects |

### std::shared_ptr

| File | Content |
|------|---------|
| `__memory/shared_count.h` | `__shared_count` (holds `__shared_owners_`, `__add_shared`, `__release_shared`, pure virtual `__on_zero_shared`); `__shared_weak_count` (adds `__shared_weak_owners_`, `__add_weak`, `__release_weak`, pure virtual `__on_zero_shared_weak`); `__libcpp_atomic_refcount_increment` (`__ATOMIC_RELAXED`); `__libcpp_atomic_refcount_decrement` (`__ATOMIC_ACQ_REL`) |
| `__memory/shared_ptr.h` | `__shared_ptr_pointer` (`shared_ptr(new T(...))` control block, uses `_LIBCPP_COMPRESSED_TRIPLE` to store `(ptr, deleter, alloc)`); `__shared_ptr_emplace` (`make_shared` control block, `_Storage` inline `(alloc, elem)` compressed via `_LIBCPP_COMPRESSED_PAIR`); `shared_ptr` class definition (two pointers: `__ptr_`, `__cntrl_`) |
| `__memory/compressed_pair.h` | `_LIBCPP_COMPRESSED_PAIR`, `_LIBCPP_COMPRESSED_TRIPLE` macros: empty class EBO + `[[no_unique_address]]` to eliminate tail padding |

## Core Classes / Functions

### function Side

| Type / Function | Source Location | Description |
|-----------------|-----------------|-------------|
| `__base<Sig>` | `function.h:134` | Abstract interface, 7 virtual functions: `__clone()`, `__clone(__base*)`, `destroy()`, `destroy_deallocate()`, `operator()`, `target()`, `target_type()` |
| `__func<_Fp, Sig>` | `function.h:158` | Inherits `__base<Sig>`, holds `_Fp __func_`; `__clone()` → `new __func(__func_)` (heap clone); `__clone(__base* p)` → `::new (p) __func(__func_)` (in-place clone); `destroy()` → `__func_.~_Fp()`; `destroy_deallocate()` → `delete this` |
| `__value_func<Sig>` | `function.h:192` | Default path: `__buf_` (24-byte `aligned_storage<3*sizeof(void*)>`) + `__f_` (`__base*`); SBO determination: `sizeof(_Fun) <= sizeof(__buf_) && is_nothrow_copy_constructible<_Fp>`; move: heap path steals pointer, stack path must `__clone` |
| `__policy_func<Sig>` | `function.h:424` | `_LIBCPP_ABI_OPTIMIZED_FUNCTION` path: `__policy_storage` (`char[16]` union `void* __large`) + `__policy*` (function pointer table: `__clone`, `__destroy`, `__is_null`, `__type_info`); stricter SBO conditions: `sizeof(_Fun) <= 16 && alignof(_Fun) <= 8 && is_trivially_copyable && is_trivially_destructible` |
| `function<Sig>` | `function.h:607` | Thin wrapper: default `__value_func<Sig> __f_` (32 bytes) or `__policy_func<Sig> __f_` (24 bytes); all operations delegate to `__f_`; copy assignment uses copy-and-swap idiom |

### shared_ptr Side

| Type / Function | Source Location | Description |
|-----------------|-----------------|-------------|
| `__shared_count` | `shared_count.h:45` | `__shared_owners_` (initial 0); `__add_shared()` → `__atomic_add_fetch(..., 1, __ATOMIC_RELAXED)`; `__release_shared()` → `__atomic_add_fetch(..., -1, __ATOMIC_ACQ_REL)`, returns `true` when reaching zero; `use_count()` → `__atomic_load_n(&__shared_owners_, __ATOMIC_RELAXED) + 1` |
| `__shared_weak_count` | `shared_count.h:81` | Inherits `__shared_count` (private); adds `__shared_weak_owners_`; `__release_shared()` → on reaching zero calls `__release_weak()`; `__release_weak()` → when weak count reaches zero calls `__on_zero_shared_weak()` (releases control block memory); `lock()` → atomically checks `use_count > 0` |
| `__shared_ptr_pointer<T,D,A>` | `shared_ptr.h:97` | `shared_ptr(new T(...))` control block: `_LIBCPP_COMPRESSED_TRIPLE(T, __ptr_, D, __deleter_, A, __alloc_)`; `__on_zero_shared()` → `__deleter_(__ptr_); __deleter_.~_Dp()`; `__on_zero_shared_weak()` → `__alloc_.deallocate(this)` |
| `__shared_ptr_emplace<T,A>` | `shared_ptr.h:133` | `make_shared` control block: internal `_Storage` contains `_Data` (`_LIBCPP_COMPRESSED_PAIR(A, __alloc_, T, __elem_)`), layout as `char[sizeof(_Data)]` buffer; `__on_zero_shared()` → `allocator_traits::destroy(__elem_)`; `__on_zero_shared_weak()` → `allocator_traits::deallocate(this)` |
| `shared_ptr<T>` | `shared_ptr.h:293` | Two pointers: `element_type* __ptr_` + `__shared_weak_count* __cntrl_` (16 bytes); optional `_LIBCPP_SHARED_PTR_TRIVIAL_ABI` attribute for register passing optimization |

## Key Algorithms

### function: SBO Decision and Cloning

| Trigger | Algorithm Path |
|---------|----------------|
| Construct `function` (`__value_func` path) | Computes `sizeof(__func<_Fp, Sig>)` (`__base` vtable ptr + `_Fp` object); if ≤ 24 and `_Fp` satisfies `is_nothrow_copy_constructible` → `::new (&__buf_) _Fun(move(f))` (in-place stack construction); otherwise `new _Fun(move(f))` (heap allocation) |
| Copy construction | Compares `src.__f_` with `src.__buf_` address: equal → stack path, calls `__clone(__as_base(&__buf_))` (in-place copy); not equal → heap path, calls `__clone()` returning a new `__base*` |
| Move construction | Stack path (`(void*)src.__f_ == &src.__buf_`) → must `__clone` (cannot steal pointer, source must remain valid); heap path → direct `this->__f_ = src.__f_; src.__f_ = nullptr` (pointer stealing, O(1)) |
| `operator=` | Assignment uses `function(move(f)).swap(*this)` or `*this = nullptr; ...` pattern: destroy old target first, then move in new target |
| `swap` | Both on stack → swap via temporary buffer with three `__clone` calls; one on stack, one on heap → stack-to-heap `__clone` + heap-to-stack pointer stealing; both on heap → swap `__f_` pointers |

### shared_ptr: Reference Counting and Two-Level Destruction

| Trigger | Algorithm Path |
|---------|----------------|
| `shared_ptr` copy | `__cntrl_->__add_shared()` → `__atomic_add_fetch(&__shared_owners_, 1, __ATOMIC_RELAXED)`; `use_count` incremented |
| `shared_ptr` destruction (use_count > 1) | `__cntrl_->__release_shared()` → `__atomic_add_fetch(&__shared_owners_, -1, __ATOMIC_ACQ_REL)` → not zero, returns `false`, no destruction triggered |
| `shared_ptr` destruction (use_count reaches zero) | `__release_shared()` returns `true` → calls `__on_zero_shared()` (virtual function): for `__shared_ptr_emplace` → `allocator_traits::destroy(elem)`, for `__shared_ptr_pointer` → `deleter(ptr)`; then `__release_weak()` |
| `weak_count` reaches zero (inside `__release_weak`) | `__shared_weak_owners_` atomically decremented to -1 → calls `__on_zero_shared_weak()` (virtual function): for `__shared_ptr_emplace` → `allocator_traits::deallocate(ctrl_blk)`, for `__shared_ptr_pointer` → `alloc.deallocate(this)` |
| `weak_ptr::lock()` | Atomic read of `use_count`: if > 0, atomic `__add_shared()` (CAS loop or `__atomic_add_fetch`), returns non-empty `shared_ptr`; if == 0, returns empty `shared_ptr` |
| `make_shared` construction | Single `allocator_traits::allocate(alloc, 1)` obtains memory for `__shared_ptr_emplace<T, Alloc>`; constructs placement-new alloc + `allocator_traits::construct(elem)` in `_Storage`'s `__buffer_`; destruction in reverse order |

Non-contention fast path optimization for `__release_weak` (`memory.cpp`):

```cpp
// Use acquire load instead of atomicrmw in non-contention case
if (__atomic_load_n(&__shared_weak_owners_, __ATOMIC_ACQUIRE) == 0) {
  __on_zero_shared_weak();  // no contention, release directly
  return;
}
// Contention: atomic decrement, release if reaches zero
```

## ABI Constraints

### function ABI

- **Object size**: `sizeof(function<Sig>)` = `sizeof(__value_func<Sig>)` = 24 + 8 = 32 bytes (`__buf_` 24 bytes + `__f_` pointer 8 bytes, after alignment); under the `__policy_func` path it is 24 bytes (`__policy_storage` 16 + `__policy*` 8)
- **SBO buffer fixed at `3 × sizeof(void*)`**: Changing this size breaks the memory layout of all compiled `function` objects, constituting an ABI break
- **`__base` vtable**: Each wrapped type instantiates a new `__func` subclass, each with its own vtable; `function` instances with the same signature but different wrapped types share the `__base` interface but have independent `__func` vtables
- **`_LIBCPP_ABI_OPTIMIZED_FUNCTION` macro**: When enabled, switches to the `__policy_func` path, reducing object size but changing SBO conditions (requires trivially copyable + trivially destructible), incompatible with the default ABI
- **`bad_function_call`**: The presence or absence of a key function determines whether the vtable is exported or weak; the `_LIBCPP_AVAILABILITY_HAS_BAD_FUNCTION_CALL_KEY_FUNCTION` macro controls this behavior

### shared_ptr ABI

- **Object size**: `sizeof(shared_ptr<T>)` = 16 bytes (`__ptr_` + `__cntrl_` each 8 bytes); `weak_ptr` has the same layout
- **`_LIBCPP_SHARED_PTR_TRIVIAL_ABI`**: Optional `__attribute__((__trivial_abi__))`, allowing `shared_ptr` to be passed in registers (instead of forced through the stack), significantly reducing call overhead; not enabled by default; enabling it creates ABI incompatibility with code compiled without this attribute
- **Control block layout**: `__shared_count` (`long __shared_owners_`, 8 bytes) + `__shared_weak_count` (adds `long __shared_weak_owners_`, 8 bytes) + vtable pointer (8 bytes) = 24 bytes base for control block; `__shared_ptr_emplace` additionally appends `sizeof(CompressedPair<Alloc, T>)`
- **Reference count type**: `long` (typically 8 bytes on 64-bit platforms); changing to another type constitutes an ABI break
- **`_LIBCPP_COMPRESSED_TRIPLE` / `_LIBCPP_COMPRESSED_PAIR`**: The compressed pair layout within control blocks is controlled by the compiler macro `_LIBCPP_ABI_NO_COMPRESSED_PAIR_PADDING`; GCC and Clang have different alignment strategies to maintain old ABI compatibility

## Exception Safety

### function: Basic Guarantee

`__value_func` constructor exception paths:

1. **SBO path** (`sizeof(_Fun) ≤ 24 && is_nothrow_copy_constructible`): `_Fun`'s move construction is guaranteed by `is_nothrow_copy_constructible`; if `_Fun`'s own construction throws, `__f_` remains `nullptr` (initial state), no leak → **basic guarantee**
2. **Heap path** (`new _Fun(move(f))`): If `operator new` throws `bad_alloc`, `__f_` remains `nullptr`; if `_Fun` construction throws, `operator new` automatically reclaims memory → **basic guarantee**
3. **`operator()` invocation**: Exceptions inside the user callable propagate directly, internal state pointed to by `__f_` is unaffected; if `__f_ == nullptr`, throws `bad_function_call`

**Note**: SBO requires `is_nothrow_copy_constructible` (not just move), because copy construction must also be safe — this guarantees that the `__clone(__base*)` path won't leave a half-constructed state on the stack.

### shared_ptr: Strong Guarantee

1. **`make_shared<T>(args...)` construction**: If `allocator_traits::construct` throws, `__shared_ptr_emplace`'s `_Storage` destructs the allocator via RAII, `allocator_traits::deallocate` reclaims control block memory → **strong guarantee** (single allocation path, exception means full rollback)
2. **`shared_ptr(new T(...), deleter)` construction**: If element `new T(...)` throws, the control block has been allocated but will be automatically released (the `__shared_ptr_pointer` constructor does not allocate the control block — it is allocated by the `shared_ptr` constructor then passed in, protected by a `_Guard` RAII object)
3. **Copy/assignment**: `shared_ptr`'s copy only modifies the atomic reference count (`noexcept`), will not throw
4. **`shared_ptr` move assignment**: `reset()` old control block first, then steal new pointer (`noexcept`)

**`shared_ptr` is one of the strongest exception-safe components in the standard library**: apart from the user-provided constructor/deleter that may throw during initial construction, all operations are `noexcept`.

## Performance Model

### function Performance Characteristics

| Operation | With Stack SBO | Without (Heap Allocation) |
|-----------|----------------|--------------------------|
| Construct (no-capture lambda / function pointer) | 0 `malloc`, only placement new + copy | 1 `malloc` (`sizeof(__func)` + `__base` vtable ptr) |
| Construct (large lambda / capture) | N/A | 1 `malloc` |
| Copy construction | 1 virtual call `__clone(__base*)` (in-place copy) | 1 `malloc` + virtual call `__clone()` |
| Move construction | 1 virtual call `__clone(__base*)` (must copy, cannot steal) | 0 `malloc`, pointer stealing |
| `operator()` | 1 virtual call (`(*__f_)(args...)`) | Same |
| Destroy (on stack) | 1 virtual call `destroy()` (in-place destroy, no free) | N/A |
| Destroy (on heap) | N/A | 1 virtual call `destroy_deallocate()` (`delete this`) |

**Key performance insights**:

- `sizeof(function)` = 32 bytes is the largest among all major implementations (GCC's `function` = 32, MSVC's = 64), but this buys a 24-byte SBO buffer, making `int(*)(int)`, no-capture lambdas, and small function objects (≤ 24 bytes) all allocation-free
- Moving a stack-based callable requires a virtual function call (same cost as copy), which is libc++ `function`'s core cost compared to `std::unique_ptr`
- `__policy_func` (`_LIBCPP_ABI_OPTIMIZED_FUNCTION`) has stricter SBO conditions (requires trivially copyable + trivially destructible), but reduces object size to 24 bytes and avoids virtual function overhead (via function pointer table)

### shared_ptr Performance Characteristics

| Operation | Cost |
|-----------|------|
| `make_shared<T>(args)` | 1 `malloc` + 1 element construction |
| `shared_ptr(new T(args))` | 2 `malloc` (element + control block) |
| `shared_ptr` copy | 1 `atomic_add_fetch(RELAXED)` |
| `shared_ptr` destruction (use_count > 1) | 1 `atomic_add_fetch(ACQ_REL)` |
| `shared_ptr` destruction (use_count reaches zero) | 1 `atomic_add_fetch(ACQ_REL)` + 1 virtual call `__on_zero_shared()` + 1 `atomic_add_fetch(ACQ_REL)` (`__release_weak`) |
| `weak_ptr::lock()` | 1 acquire load + 1 `atomic_add_fetch(RELAXED)` (on success) |

**Cache and memory considerations**:

- `make_shared`'s single allocation places the control block and element in the same cache line, providing clear cache-friendly advantages for weak reference scenarios
- Control block vtable pointer (8 bytes) causes control block access to touch at least one cache line (64 bytes); for high-frequency `shared_ptr` copy/destruction scenarios, contention on the control block's cache line is the main bottleneck
- `_LIBCPP_SHARED_PTR_TRIVIAL_ABI` allows register passing, avoiding stack spill for by-value `shared_ptr` parameters, providing ~10–15% improvement in IPC-intensive scenarios

## Compilation / Benchmark Evidence

### Compile-Time Constant Validation

```cpp
// function SBO buffer size (64-bit platform)
static_assert(sizeof(__function::__value_func<int(int)>) == 32, "sizeof(function) = 32");
static_assert(sizeof(std::function<int(int)>) == 32, "sizeof(function) = 32");

// shared_ptr / weak_ptr size
static_assert(sizeof(std::shared_ptr<int>) == 16, "shared_ptr = 2 pointers");
static_assert(sizeof(std::weak_ptr<int>) == 16, "weak_ptr = 2 pointers");

// SBO threshold verification
// int(*)(int) = 8 bytes → SBO; large capture lambda → heap
// __func<int(*)(int), int(int)> contains vtable(8) + ptr(8) = 16 ≤ 24 → SBO
```

### function SBO Coverage

| Type | sizeof(_Fun) (estimated) | Enters SBO? |
|------|-------------------------|-------------|
| `int(*)(int)` (function pointer) | 16 (vtable + pointer) | ✅ Yes |
| `[=](int x){ return x + cap; }` (captures 1 int) | 20 (vtable + int + padding) | ✅ Yes |
| `[=](int x){ return x + a + b; }` (captures 2 ints) | 24 (vtable + 2×int) | ✅ Yes (boundary) |
| `[=]() { return vec; }` (captures `std::vector`) | > 24 | ❌ Heap allocation |
| `std::bind(&obj, &Class::method, _1)` | > 24 | ❌ Heap allocation |

### shared_ptr Control Block Overhead

| Control Block Type | Applicable Scenario | Element Allocation | Total malloc Count |
|-------------------|---------------------|-------------------|-------------------|
| `__shared_ptr_emplace<T, Alloc>` | `make_shared<T>(args)` | Inline in control block | 1 |
| `__shared_ptr_pointer<T, D, A>` | `shared_ptr(new T, D)` | Independent `new` | 2 |
| `__shared_ptr_pointer<T, D, A>` | `shared_ptr(new T)` | Independent `new`, D = `default_delete` | 2 |

### Reference Count Memory Ordering

| Operation | Memory Order | Reason |
|-----------|-------------|--------|
| `__add_shared()` | `__ATOMIC_RELAXED` | Only incrementing, no subsequent dependency (`PR22803`) |
| `__release_shared()` | `__ATOMIC_ACQ_REL` | Acquire ensures seeing writes before modification; release ensures own writes are visible to the next zero-reacher |
| `__add_weak()` | `__ATOMIC_RELAXED` | Same as `__add_shared` |
| `__release_weak()` | `__ATOMIC_ACQ_REL` | At zero, needs to see complete object destruction results |
