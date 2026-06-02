---
title: "Coroutine Compiler Lowering: Frame Layout, Allocation, Symmetric Transfer, and HALO Verification"
topic: cpp20
feature: coroutine-lowering
standard: C++20
status_checked_at: 2026-06-02
standard_refs:
  - draft: N4861
    clause: "[dcl.fct.def.coroutine]"
  - draft: N4861
    clause: "[coroutine.handle]"
  - draft: N4861
    clause: "[coroutine.traits]"
proposals:
  - P0057R9
  - P0620R0
  - P0913R0
  - P1477R4
  - P2008R0
exercises: []
solutions: []
---

# Coroutine Compiler Lowering (Internals)

## Overview

Coroutine functions (functions containing `co_await`, `co_yield`, or `co_return`) undergo a thorough **lowering transformation** during compilation: the compiler splits the original coroutine body into multiple code segments, allocates a coroutine frame on the heap (or on the caller's stack with HALO optimization), and drives execution with a state machine. Understanding this lowering process is a prerequisite for correctly implementing custom promise types, troubleshooting coroutine performance issues, and verifying HALO optimizations.

This article takes a compiler-centric perspective, covering: how coroutine functions are split, coroutine frame memory layout, `operator new` allocation paths and `get_return_object_on_allocation_failure`, promise type discovery via `coroutine_traits`, `await_transform` for customizing `co_await`, exception handling chains, `final_suspend` UB, destroy timing and frame ownership, symmetric transfer tail-call verification, controllable HALO optimization verification, and before/after IR comparison examples.

## Coroutine Function Lowering: How the Compiler Splits

A coroutine function body is transformed by the compiler into the following structure (pseudocode):

```
// Original coroutine
Task<int> foo(int x) {
    int local = x + 1;
    co_await some_op();
    co_return local * 2;
}

// Compiler's lowered logical structure
Task<int> foo(int x) {
    // 1. Allocate coroutine frame
    void* frame = operator new(coroutine_frame_size);

    // 2. Initialize frame fields
    auto& promise = construct_promise_in(frame);
    auto return_obj = promise.get_return_object();  // may suspend!
    frame->suspend_index = 0;        // initial state
    frame->param_x = x;              // copy parameter to frame
    // frame->local_local uninitialized

    // 3. initial_suspend
    auto init_await = promise.initial_suspend();
    if (!init_await.await_ready()) {
        init_await.await_suspend(handle);
        // suspend, waiting for resume()
    }
    // -- resume entry point --

    // 4. switch(fsm_index) drives state machine
    switch (frame->suspend_index) {
    case 0: goto resume_0;
    case 1: goto resume_1;
    }

resume_0:
    // Coroutine body segment 1: after initial_suspend, before first co_await
    frame->local_local = frame->param_x + 1;

    // 5. co_await some_op()
    {
        auto awaiter = get_awaiter(some_op());
        if (!awaiter.await_ready()) {
            frame->suspend_index = 1;       // save resumption point
            frame->current_await = &awaiter; // save current awaiter
            awaiter.await_suspend(handle);
            return;                          // return from function
        }
        awaiter.await_resume();
    }
    // fall through to resume_1 if not suspended

resume_1:
    // Coroutine body segment 2: after co_await
    // co_return local * 2
    promise.return_value(frame->local_local * 2);
    goto final_suspend;

final_suspend:
    auto final_await = promise.final_suspend();
    // final_await's await_ready() must return false
    final_await.await_suspend(handle);
    // does not return — handled by symmetric transfer or manual destroy
}
```

Key points:
- Parameters are **copied to the coroutine frame** at the coroutine entry (when not captured by reference), avoiding dangling after stack destruction
- `suspend_index` is a compiler-generated enum with one state per suspension point
- Compilers typically use **switch + computed goto** rather than actual `goto` for cross-platform compatibility

## Coroutine Frame Memory Layout

The coroutine frame is a compiler-managed runtime data structure. A typical layout:

```
Coroutine frame memory layout (Clang x86-64 example, simplified)
┌─────────────────────────────────┐  ← frame start address
│  vptr (if promise has virtual   │  8 bytes
│       destructor)               │
├─────────────────────────────────┤
│  coroutine_handle ptr to frame  │  8 bytes (compiler-managed)
├─────────────────────────────────┤
│  suspend_index (state machine)  │  4 bytes
├─────────────────────────────────┤
│  Promise object                │  sizeof(Promise)
│    ├─ get_return_object() result│
│    ├─ promise's own fields      │
│    └─ exception_ptr             │
├─────────────────────────────────┤
│  Function parameter copies      │  sum of parameter sizes (with alignment padding)
│    ├─ param_x: int             │
│    └─ param_y: string (if any) │
├─────────────────────────────────┤
│  Local variable storage         │  max active range of each local variable
│    ├─ local_local: int         │
│    └─ temp: ...                │
├─────────────────────────────────┤
│  Current awaiter storage        │  sizeof(largest awaiter)
│    └─ current_await: Awaiter   │
└─────────────────────────────────┘

Total size: computed at compile time, passed as parameter to operator new
```

How the compiler determines frame size:

```cpp
// Compiler-internal calculation (conceptual pseudocode)
size_t frame_size =
    sizeof(__coroutine_frame_header)   // suspend_index + handle pointer
  + sizeof(Promise)                    // promise_type instance
  + align_and_size(params...)          // parameter copies
  + max_active_locals_size(...)        // liveness analysis of local variables
  + max_awaiters_size(...)             // max awaiter across all co_await expressions
  + padding_for_alignment;
```

**Note**: Local variables only need to be stored in the frame if they cross a suspension point. Temporaries only active between two suspension points can remain in registers, determined by the compiler's liveness analysis.

## Allocation Paths: operator new and Custom Allocation

The compiler generates the following allocation code at the coroutine entry:

```cpp
// Compiler-generated allocation logic (conceptual)
void* __coroutine_frame = nullptr;
try {
    __coroutine_frame = ::operator new(frame_size);
} catch (...) {
    // If promise_type defines get_return_object_on_allocation_failure()
    // don't throw, directly call that function and return
    return Promise::get_return_object_on_allocation_failure();
}
```

### get_return_object_on_allocation_failure

This is an optional static member function. When the promise type defines it, the compiler uses the **nothrow version** of `operator new`, calling this function instead of throwing `std::bad_alloc` on allocation failure:

```cpp
struct LazyTask {
    struct promise_type {
        static LazyTask get_return_object_on_allocation_failure() noexcept {
            return LazyTask{nullptr};  // return invalid handle
        }
        // ... other members
    };
};
```

**When to use**: In embedded systems or real-time scenarios where exceptions are disabled, this path avoids linking `throw`.

### Custom operator new

Coroutine frames also support defining `operator new` on the promise or return type:

```cpp
struct PooledTask {
    struct promise_type {
        // Takes precedence over global operator new
        static void* operator new(std::size_t size) {
            return pool::allocate(size);  // custom memory pool
        }
        static void operator delete(void* ptr, std::size_t size) noexcept {
            pool::deallocate(ptr, size);
        }
        // ...
    };
};
```

The compiler looks up allocation functions in this order:
1. Return type's (`Task`) `operator new`
2. Promise type's `operator new`
3. Global `::operator new`

## Promise Type Discovery: coroutine_traits

The compiler finds the promise type through `std::coroutine_traits<ReturnType, Params...>::promise_type`. Default implementation:

```cpp
// Default trait in the std namespace
template <typename R, typename... Args>
struct coroutine_traits {
    using promise_type = typename R::promise_type;
};

// R is the coroutine's return type
// Args are parameter types (for specialization scenarios)
```

**Specialization scenario**: When the return type lacks a `promise_type` nested member, you can specialize `coroutine_traits`:

```cpp
// Provide coroutine support for std::future<int>
template <typename... Args>
struct std::coroutine_traits<std::future<int>, Args...> {
    struct promise_type {
        std::promise<int> p;
        std::future<int> get_return_object() { return p.get_future(); }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_value(int v) { p.set_value(v); }
        void unhandled_exception() { p.set_exception(std::current_exception()); }
    };
};
```

This makes `std::future<int> my_coro(...)` automatically become a coroutine without modifying `std::future` itself.

## await_transform: Customizing co_await Behavior

If the promise type defines `await_transform`, the compiler rewrites `co_await expr` to `co_await promise.await_transform(expr)`. This is a powerful hook for controlling `co_await` semantics:

```cpp
struct SwitchTask {
    struct promise_type {
        // All co_await expressions pass through this function
        auto await_transform(std::coroutine_handle<> h) {
            struct SwitchAwaiter {
                std::coroutine_handle<> target;
                bool await_ready() noexcept { return false; }
                // Symmetric transfer: directly jump to target
                std::coroutine_handle<> await_suspend(
                    std::coroutine_handle<>) noexcept {
                    return target;
                }
                void await_resume() noexcept {}
            };
            return SwitchAwaiter{h};
        }

        // Forbid directly co_await-ing other awaitables
        void await_transform(auto&) = delete;

        // ... other members
    }
};
```

**Typical applications**:
- **EagerlyEvaluated awaitable**: restrict `co_await` to specific awaitable types
- **Dispatcher**: ensure the coroutine resumes on a specific thread (e.g., UI thread)
- **Forbid co_await**: `await_transform(auto&) = delete` makes all `co_await` compile errors

```cpp
// Practical application: restrict co_await types
struct SafeTask {
    struct promise_type {
        // Only allow co_await of known-safe awaitables
        template <typename T>
        auto await_transform(SafeAwaitable<T> a) { return a; }

        // All other types produce compile errors
        template <typename T>
            requires (!is_safe_awaitable_v<T>)
        void await_transform(T&&) = delete;
        // ...
    };
};
```

## Exception Handling: unhandled_exception and await_resume Rethrow

Exception propagation in coroutines follows two paths:

### Path 1: Exceptions within the coroutine body

```cpp
void co_await_work() {
    auto awaiter = get_awaitable(expr);
    try {
        if (!awaiter.await_ready()) {
            suspend_index = N;
            current_await = &awaiter;
            awaiter.await_suspend(handle);
            return;
        }
    } catch (...) {
        promise.unhandled_exception();  // captured to promise
        goto final_suspend;
    }
    awaiter.await_resume();  // may throw → unhandled_exception
}
```

Typical implementation of `unhandled_exception()`:

```cpp
struct promise_type {
    std::exception_ptr ex_;

    void unhandled_exception() {
        ex_ = std::current_exception();  // store rather than terminate
    }

    // Rethrow in await_resume
    void rethrow_if_failed() {
        if (ex_) std::rethrow_exception(ex_);
    }
};
```

### Path 2: Rethrow at the caller side after resumption

```cpp
// Task's await_resume rethrows the exception stored in the coroutine's promise at the caller side
struct Task {
    struct Awaiter {
        std::coroutine_handle<promise_type> h;
        bool await_ready() noexcept { return false; }
        void await_suspend(std::coroutine_handle<> caller) { /* ... */ }
        int await_resume() {
            if (h.promise().ex_)
                std::rethrow_exception(h.promise().ex_);
            return h.promise().result;
        }
    };
};
```

**Chain summary**: Coroutine body throws → `unhandled_exception()` stores → `final_suspend()` suspends → caller resumes and rethrows via `await_resume()`.

## final_suspend and UB

The awaiter returned by `final_suspend()` **must have `await_ready()` return `false`** (i.e., must suspend). If it returns `true`:

```cpp
// Dangerous example — do not do this
std::suspend_never final_suspend() noexcept { return {}; }
// Equivalent to await_ready() == true → coroutine frame is immediately destroyed
// But coroutine_handle may still be held → dangling handle → UB
```

When `final_suspend` does not suspend, the compiler **immediately destroys the coroutine frame** after the coroutine body completes. Any subsequent operations on the `coroutine_handle` (`resume()`, `done()`, `promise()`) are undefined behavior.

**Correct approach**: `final_suspend` should always return `suspend_always` or a custom awaiter with `await_ready()` returning `false`. The coroutine's owner explicitly calls `handle.destroy()` at the appropriate time.

## Destroy Timing and Frame Ownership

Who manages the coroutine frame's lifetime:

```
Coroutine frame ownership model
─────────────────────────────────────────────────────
1. Compiler allocates frame at coroutine entry, returns return_object to caller
2. return_object typically holds coroutine_handle (non-owning raw pointer semantics)
3. Who is responsible for destroy?
   a. The return object's RAII destructor (most common)
   b. Caller manually calls after final_suspend
   c. End of symmetric transfer chain
─────────────────────────────────────────────────────
```

RAII wrapper example:

```cpp
template <typename T>
class OwnedTask {
public:
    using promise_type = /* ... */;
    ~OwnedTask() {
        if (handle_ && handle_.done())
            handle_.destroy();  // only destroy after final suspension
    }
    OwnedTask(OwnedTask&& o) noexcept : handle_(o.handle_) {
        o.handle_ = nullptr;
    }
    OwnedTask(const OwnedTask&) = delete;
private:
    std::coroutine_handle<promise_type> handle_{};
};
```

**Destroy constraints**:
- `destroy()` can only be called while the coroutine is in a **suspended state**
- Calling `destroy()` while the coroutine is running (not suspended) is UB
- After `destroy()`, the handle becomes dangling; any operation is UB

## Symmetric Transfer and Tail Call Verification

### Problem: Nested resume causes stack overflow

```
A.resume()        // stack frame +1
  └→ B.resume()   // stack frame +2
      └→ C.resume() // stack frame +3
          └→ ...    // N coroutines → stack depth O(N)
```

### Solution: Symmetric Transfer

When `await_suspend` returns `coroutine_handle<>`, the compiler optimizes it as a **tail call**, keeping stack depth constant:

```cpp
// await_suspend returns coroutine_handle → tail call
struct FinalAwaiter {
    std::coroutine_handle<> continuation;
    bool await_ready() noexcept { return false; }
    std::coroutine_handle<> await_suspend(
        std::coroutine_handle<>) noexcept {
        return continuation ? continuation : std::noop_coroutine();
    }
};
```

**Tail call verification**: Compilers (Clang/MSVC) compile symmetric transfer as `llvm.coro.resume` + `tail call` instructions. Verify with:

```bash
# Clang: check for tail markers in IR
clang++ -std=c++20 -O2 -S -emit-llvm -o - coroutine.cpp | grep "tail call"
# Should see:
#   tail call void @llvm.coro.resume(ptr %cont)
```

**`noop_coroutine()`**: Returns a special coroutine handle whose `resume()` is a no-op. Used as a termination sentinel for symmetric transfer chains, avoiding returning to a destroyed frame after `destroy()`.

### Stack Depth Proof

```
Symmetric transfer stack behavior:
A.resume()                    // stack frame +1
  └→ tail call B (symmetric)  // stack frame unchanged (reuses A's frame)
      └→ tail call C          // stack frame unchanged
          └→ tail call A.cont // stack frame unchanged
              └→ return       // stack frame -1

Result: regardless of how long the coroutine chain is, stack depth is always O(1)
```

## HALO (Heap Allocation eLision Optimization)

HALO allows the compiler to promote coroutine frame allocation from the heap to the caller's stack frame. **This is not a standard guarantee, but an optimization**.

### HALO Conditions

The compiler must prove:
1. The coroutine frame's lifetime is completely nested within the caller's lifetime
2. There is no path where the coroutine frame escapes the caller
3. `coroutine_handle` is not stored anywhere outside the caller's stack

```cpp
// HALO-friendly: nested lifetime
Generator<int> range(int lo, int hi) {
    for (int i = lo; i < hi; ++i) co_yield i;
}
void use() {
    for (int v : range(0, 10)) { /* frame can be promoted to stack here */ }
}

// HALO-unfriendly: frame escape
std::vector<Generator<int>> gens;
void spawn() {
    gens.push_back(range(0, 100));  // frame must be heap-allocated — lifetime exceeds function
}
```

### Controllable Verification: Allocation Counter

Use a custom `operator new` to track actual coroutine frame allocations:

```cpp
#include <atomic>
#include <cstdio>

inline std::atomic<int> g_coroutine_allocs{0};

struct TrackedAlloc {
    struct promise_type {
        static void* operator new(std::size_t size) {
            g_coroutine_allocs.fetch_add(1, std::memory_order_relaxed);
            return ::operator new(size);
        }
        static void operator delete(void* p, std::size_t sz) noexcept {
            g_coroutine_allocs.fetch_sub(1, std::memory_order_relaxed);
            ::operator delete(p, sz);
        }
        // ... promise members
    };
};

// Verification
void test_halo() {
    auto before = g_coroutine_allocs.load();
    {
        auto gen = range(0, 5);  // expect HALO → no operator new triggered
        for (int v : gen) { (void)v; }
    }
    auto after = g_coroutine_allocs.load();
    std::printf("coroutine allocs: %d (HALO %s)\n",
                after - before, (after == before ? "hit" : "miss"));
}
```

```bash
# Compile and verify
clang++ -std=c++20 -O2 -fcoroutines -o halo_test halo_test.cpp
./halo_test
# Expected output: coroutine allocs: 0 (HALO hit)
```

**Practical experience**:
- Clang supports HALO for simple generators at `-O1` and above
- MSVC requires `/O2` and a sufficiently simple coroutine body
- GCC's HALO support in 14.x is still incomplete
- Complex coroutines (capturing `shared_ptr`, nested `co_await`) typically do not trigger HALO

## Before/After IR: LLVM IR for a Simple Coroutine

Source code:

```cpp
#include <coroutine>

struct Task {
    struct promise_type {
        Task get_return_object() {
            return {std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() { throw; }
    };
    std::coroutine_handle<promise_type> handle;
};

Task simple_coro(int x) {
    int local = x * 2;
    co_await std::suspend_always{};
    co_return;
}
```

**Lowered LLVM IR (simplified core structure)**:

```llvm
; Coroutine entry function
define void @_Z11simple_coroi(i32 %x, ptr sret(%struct.Task) %result) {
entry:
  ; 1. Allocate coroutine frame
  %frame = call ptr @llvm.coro.begin(...)
  ; Equivalent to %frame = call noalias ptr @_Znwm(i64 <frame_size>)

  ; 2. Store parameters to frame
  %param_ptr = getelementptr inbounds %coroutine.Frame, ptr %frame, i32 0, i32 3
  store i32 %x, ptr %param_ptr

  ; 3. Construct promise
  ; ... promise initialization code ...

  ; 4. get_return_object
  call void @_ZN4TaskC1EPNSt16coroutine_handleINS_12promise_typeEEE(...)

  ; 5. initial_suspend
  %init = call i8 @llvm.coro.suspend(...)
  switch i8 %init, label %suspend [
    i8 0, label %body       ; resumed → enter coroutine body
    i8 1, label %cleanup    ; initial suspension
  ]

body:
  ; 6. Coroutine body segment 1: int local = x * 2
  %x_val = load i32, ptr %param_ptr
  %local_val = mul i32 %x_val, 2
  %local_ptr = getelementptr inbounds %coroutine.Frame, ptr %frame, i32 0, i32 4
  store i32 %local_val, ptr %local_ptr

  ; 7. co_await std::suspend_always{} → unconditional suspension
  %suspend1 = call i8 @llvm.coro.suspend(...)
  switch i8 %suspend1, label %suspend [
    i8 0, label %resume1
    i8 1, label %cleanup
  ]

resume1:
  ; 8. co_return → jump to final_suspend
  br label %final

final:
  ; 9. final_suspend
  %suspend_final = call i8 @llvm.coro.suspend(...)
  switch i8 %suspend_final, label %suspend [
    i8 0, label %unreachable
    i8 1, label %cleanup
  ]

cleanup:
  ; 10. Frame destruction (only executed on destroy())
  call i1 @llvm.coro.end(ptr %frame, i1 false, ...)
  br label %suspend

suspend:
  ret void

unreachable:
  unreachable
}
```

**Key LLVM intrinsics**:

| Intrinsic | Purpose |
|-----------|---------|
| `@llvm.coro.begin` | Begin coroutine, complete frame allocation |
| `@llvm.coro.suspend` | Mark suspension point, returns 0 (resume) / 1 (destroy) / 2 (suspend) |
| `@llvm.coro.end` | Mark coroutine end |
| `@llvm.coro.id` | Coroutine identity, includes promise and allocation metadata |
| `@llvm.coro.destroy` | Destroy coroutine frame |
| `@llvm.coro.resume` | Resume coroutine execution |
| `@llvm.coro.done` | Check if at final_suspend |

## Summary

```
Coroutine Lowering Full Pipeline
──────────────────────────────────────────
Source: co_await / co_yield / co_return
    ↓
Compiler: identify as coroutine → lookup coroutine_traits
    ↓
Extract: promise_type → frame size calculation → state machine generation
    ↓
Entry: operator new → initialize frame → get_return_object
    ↓
Initial: initial_suspend → await_ready? → suspend/continue
    ↓
Loop: switch(suspend_index) → coroutine body segment → co_await → suspend
    ↓
End: return_value/void → final_suspend → suspend
    ↓
Destroy: handle.destroy() → frame members destructed in reverse → operator delete
──────────────────────────────────────────
```

After mastering these internals, when implementing custom promise types you can:
- Precisely control frame allocation (custom `operator new`)
- Unify `co_await` semantics via `await_transform`
- Verify whether HALO optimization is effective
- Ensure symmetric transfer avoids stack overflow
- Correctly handle exception propagation chains
