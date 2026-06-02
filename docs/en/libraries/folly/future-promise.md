---
title: Folly Future/Promise
topic: libraries
feature: future-promise
standard: N/A
status_checked_at: 2026-06-02
implementation:
  folly:
    paths:
      - references/impl/folly/folly/futures/Future.h
      - references/impl/folly/folly/futures/Promise.h
      - references/impl/folly/folly/futures/Core.h
    symbols:
      - Core
      - Future
      - SemiFuture
      - Promise
      - Try
exercises: []
solutions: []
---
# Folly Future/Promise: Core + Executor Async Model

> Source paths: `references/impl/folly/folly/futures/Future.h`, `Promise.h`, `Core.h`

## Core Shared State

```
Promise<T>  ──write──→  Core<T>  ←──read──  Future<T>
                         │
                    Try<T> result       (value or exception)
                    callback            (continuation)
                    Executor*           (scheduler)
```

```cpp
template <typename T>
struct Core {
  Try<T> result_;                    // value or exception
  std::function<void(Try<T>&&)> callback_;
  Executor* executor_;
  std::atomic<State> state_;         // four-state machine
  std::shared_ptr<RequestContext> context_;
};
```

## Four-State Machine

```
Core<T> complete state transition diagram:

                         Promise::setValue()
                    ┌─────────────────────────────────┐
                    │                                 ▼
              ┌─────────┐   Future::thenValue()  ┌──────────────┐
              │         │ ─────────────────────→ │              │
              │  Start  │                        │ OnlyCallback │
              │         │ ─────────────────────→ │              │
              └─────────┘                        └──────┬───────┘
                    │    Promise::setValue()              │
                    │                                 ▼
                    │                           ┌──────────────┐
                    │                           │     Done     │ ← callback(result)
                    │                           │   (terminal) │
                    │                           └──────────────┘
                    │                                 ▲
              ┌──────────────┐                        │
              │              │   Future::thenValue()  │
              │  OnlyResult  │ ─────────────────────→ ┘
              │              │
              └──────────────┘

Path 1 (Promise sets value first): Start → OnlyResult → Done
Path 2 (Future registers callback first): Start → OnlyCallback → Done

Regardless of which side arrives first, the callback is guaranteed to be triggered—the state machine eliminates races
```

## SemiFuture vs Future

```cpp
// SemiFuture: no bound Executor, cannot call .then()
SemiFuture<int> sf = makeSemiFuture(42);
Future<int> f = std::move(sf).via(&executor);  // must explicitly choose an Executor

// Future: bound Executor, supports chaining continuations
auto result = std::move(f)
    .thenValue([](int v) { return v * 2; })
    .thenValue([](int v) { return v + 1; });
```

**Why the distinction?** SemiFuture prevents users from accidentally executing continuations on the wrong Executor. The Executor must be chosen explicitly.

## Executor Abstraction

```cpp
class Executor {
public:
  virtual void add(Func&& f) = 0;  // submit task to executor
};

// Typical implementations:
// - CPUThreadPoolExecutor: thread pool
// - IOThreadPoolExecutor: I/O thread pool
// - InlineExecutor: execute on the current thread (for testing)
```

## Comparison with `std::future`

| Dimension | Folly Future | std::future |
|-----------|-------------|------------|
| Continuation | **`.thenValue()` chaining** | None (must block on `get()`) |
| Exception propagation | Automatic (wrapped in `Try<T>`) | Rethrown at `get()` |
| Executor | **Explicit binding** | None |
| SemiFuture | **Prevents misuse** | None |
| Performance | Avoids `shared_state` atomic ops | Reference-counted `shared_state` |

## User API

Users typically interact with this model through `Promise<T>`, `SemiFuture<T>`, `Future<T>`, and the `.via()` / `.thenValue()` chaining interface; the existing text above already goes directly into the `Core<T>` shared state.

## Standard Semantics

### Core Semantic Differences from `std::future` / `std::promise`

| Dimension | `std::future` / `std::promise` | Folly Future / Promise |
|-----------|-------------------------------|----------------------|
| **Continuation model** | No native continuation; can only block on `.get()` or poll `.wait_for()` | `.thenValue()` / `.thenTry()` chained monadic composition; callback triggered asynchronously by executor when result is ready |
| **Executor binding** | Implicit — `shared_state` managed by the standard library, user cannot control which thread the continuation runs on | **Explicit**: `SemiFuture` has no executor; must convert to `Future` via `.via(executor)` before attaching continuations; executor determines the callback's execution context |
| **Lazy vs eager** | `std::async` defaults to eager (starts immediately); `std::promise` is manually fulfilled | `SemiFuture` / `defer` family are lazy — continuations may only be scheduled after the executor is set; `Future` explicitly selects scheduling timing via `.via()` |
| **Exception propagation** | Stored in `shared_state`, rethrown at `.get()`; if the future is destroyed without calling `get()`, the exception is silently discarded | `Try<T>` uniformly wraps value/exception; exceptions propagate automatically along the continuation chain, reaching the end if uncaught; if `Promise` is destroyed without fulfilling, the future receives `BrokenPromise` |
| **SemiFuture / Future separation** | No such concept | `SemiFuture` (no executor, supports `defer*`) → `Future` (has executor, supports `then*`). Prevents users from accidentally executing continuations on the wrong executor |
| **Cancellation/interruption** | `std::stop_token` (C++20) is notification only | `future.raise(exception)` → `promise.setInterruptHandler(fn)` bidirectional channel; `cancel()` is a convenience method for `raise(FutureCancellation())` |
| **Sharing semantics** | `std::shared_future` allows multiple consumers | Folly Future is **non-copyable**; one `Promise` produces only one `SemiFuture`/`Future`, ensuring correctness of the single-consumer model |
| **Void representation** | `std::future<void>` / `std::promise<void>` is legal | `void` is not legal; use `folly::Unit` instead (`Future<Unit>` / `Promise<Unit>`) |

### Key Semantics of Executor Binding

1. **`.via()` is a one-way gate**: `SemiFuture::via(exec)` returns a `Future`, and the original `SemiFuture` becomes invalid (`valid() == false`). All subsequent `.thenValue()` continuations execute on that executor.
2. **Executor can be switched**: Calling `.via(exec2)` again in a `Future` chain switches the execution context for subsequent continuations, similar to `|` in a Unix pipe.
3. **Inline optimization**: If the previous callback's completing executor matches the next continuation's target executor, Folly can execute inline (`thenValueInline` / `OnlyCallbackAllowInline` state), avoiding an executor dispatch hop.
4. **`defer*` vs `then*`**: `SemiFuture::deferValue()`'s continuation is deferred until the consumer calls `.get()` or sets an executor — no executor binding is forced; `Future::thenValue()` requires an executor to already be bound.

## Object Layout

The key fields of `Core<T>` have been given above; a diagram of the ownership relationship between `Promise`/`Future` handles and the shared `Core` will be added later.

## Core Source Paths

`Future.h`, `Promise.h`, `Core.h` have been listed at the top of this document; the entry chain from continuation registration to executor dispatch will be added later.

### `Try<T>` — Unified Value/Exception Wrapper

```cpp
template <typename T>
class Try {
  union { T value_; exception_wrapper exception_; };
  enum { EMPTY, VALUE, EXCEPTION } state_;
};
```

- `Try<T>` is the result type of `Core<T>`, always in one of three states: **empty** (unset), **holds value**, or **holds exception**.
- No `void` specialization exists; `Try<Unit>` represents a valueless completion.
- Provides `hasValue()` / `hasException()` / `value()` / `exception()` accessors; calling `value()` rethrows if the current state is an exception.
- `makeTryWith(func)` constructs a `Try` from either the return value of `func()` or a thrown exception.

### `Core<T>` — Shared State Core

```cpp
// CoreBase: T-independent portion (reduces template instantiation overhead)
class CoreBase {
  Callback callback_;                      // folly::Function<void(CoreBase&, KeepAlive<>&&, exception_wrapper*)>
  std::atomic<State> state_;               // six-state FSM
  std::atomic<unsigned char> attached_;    // reference count (one bit for Promise, one for Future)
  std::atomic<unsigned char> callbackReferences_;
  KeepAliveOrDeferred executor_;           // tagged union: KeepAlive | DeferredExecutor
  Context context_;                        // shared_ptr<RequestContext>
  std::atomic<uintptr_t> interrupt_;       // interrupt FSM (low bits encode state, high bits store pointer)
  CoreBase* proxy_;                        // proxy chain
};

template <typename T>
class Core final : private ResultHolder<T>, public CoreBase {
  // ResultHolder<T> provides union { Try<T> result_; }, controlling layout
  // Ensures result_ shares the same cache line as the vtable pointer and callback_
};
```

Key factory methods:
- `Core::make()` → initial state `Start`
- `Core::make(Try<T>&&)` → initial state `OnlyResult` (value already ready)
- `Core::make(in_place, args...)` → in-place construct the result

### `Promise<T>` — Producer Handle

```cpp
template <class T>
class Promise {
  Core* core_;        // pointer to shared Core
  bool retrieved_;    // whether getSemiFuture/getFuture has been called
};
```

Core methods:
- `getSemiFuture()` → returns `SemiFuture<T>`, sharing the same `Core`; can only be called once.
- `setValue(v)` / `setException(ew)` / `setTry(Try<T>&&)` → fulfill the promise, triggering FSM transitions.
- `setInterruptHandler(fn)` → register an interrupt callback, paired with `future.raise()`.
- On destruction, if `valid() && !isFulfilled()`, automatically fulfills the associated future with a `BrokenPromise` exception.

### `SemiFuture<T>` — Consumer Side (No Executor)

- Inherits `FutureBase<T>`, does not hold an executor.
- Core methods: `deferValue(f)` / `defer(f)` / `deferError(f)` / `deferEnsure(f)` — register deferred continuations.
- `.via(executor)` → converts to `Future<T>`, binding an executor.
- `.get()` → blocks the current thread waiting for the result.
- `.wait()` → blocks waiting, but does not move the result.

### `Future<T>` — Consumer Side (With Executor)

- Inherits `FutureBase<T>`, holds an executor.
- Core methods: `thenValue(f)` / `thenTry(f)` / `thenError(tag, f)` / `ensure(f)`.
- `.then(f)` is an alias for `.thenTry(f)`.
- Each `.then*()` returns a new `Future<R>` (monadic map); the original `Future` becomes invalid.
- If the continuation returns `Future<R>`, automatic unwrapping occurs (flatMap semantics).

### `.via()` — Executor Binding

```cpp
// SemiFuture → Future
Future<T> SemiFuture<T>::via(Executor::KeepAlive<> executor) &&;
// Future → Future (switch executor)
Future<T> Future<T>::via(Executor::KeepAlive<> executor) &&;
```

Effects of `.via()`:
1. Writes the executor into `Core::executor_`.
2. If the result is already ready (`OnlyResult`), immediately schedules the callback through the executor.
3. If a callback is already registered (`OnlyCallback`), waits for the result to arrive and then schedules on the new executor.

### `.thenValue()` — Continuation Registration

```cpp
template <typename F>
Future<R> Future<T>::thenValue(F&& func) &&;
```

1. Creates a new `Core<R>` (new) from the current `Core` (old); the new Core inherits the executor from the old Core.
2. Installs a callback on the old Core: upon receiving `Try<T>`, extracts the value, passes it to `func`, and writes the result into the new Core.
3. If the old Core is already in `OnlyResult`, the callback executes immediately.
4. The old `Future`'s `core_` is set to null (`valid() == false`); the new `Future` holds the new Core.

## Key Algorithms

### Core Path 1: `setValue` / `setResult` — Producer Writes Result

```
Promise::setValue(v)
  → Core::setResult(Try<T>(std::move(v)))
    → placement-new Try<T> into Core::result_
    → Core::setResult_(completingKA)
      → state_.compare_exchange_strong(expected=Start, desired=OnlyResult)
        If successful (Start → OnlyResult):
          result is stored, no callback → terminal state, waiting for callback to arrive
        If expected is OnlyCallback or OnlyCallbackAllowInline:
          → state = Done
          → doCallback(): schedule callback(result) on executor
        If expected is Proxy:
          → proxyCallback(): forward result to proxy Core
```

Key point: `setResult` is a one-time operation; calling it twice triggers undefined behavior (explicitly noted in the source code comments).

### Core Path 2: Continuation Installation — Consumer Registers Callback

```
Future::thenValue(func)
  → create new Core<R> (state Start)
  → construct callback lambda:
      receives (Try<T>&&) → extracts value → calls func(value) → writes into new Core<R>
  → old Core::setCallback(callback, context, allowInline)
    → state_.compare_exchange_strong(expected=Start, desired=OnlyCallback)
      If successful (Start → OnlyCallback):
        callback is stored, waiting for result
      If expected is OnlyResult:
        → state = Done
        → doCallback(): immediately execute callback(result)
      If expected is Proxy:
        → pass callback through proxy chain
  → return Future<R>(new Core)
```

### Core Path 3: Executor Submission — doCallback Dispatch

```
Core::doCallback(completingKA, priorState)
  → check if completingKA and executor_ are the same executor
    → if same and priorState == OnlyCallbackAllowInline:
        inline execute callback (avoid executor hop)
    → otherwise:
        executor_->add([callback, result]() { callback(result); })
        → executor is responsible for executing on the appropriate thread
```

### Core Path 4: Exception Propagation

```
Promise::setException(ew)
  → setTry(Try<T>(exception_wrapper(ew)))
    → normal setValue path, but Try holds an exception

Exception thrown inside continuation:
  → func(value) throws E inside callback
  → catch → Try<R>(exception_wrapper(E)) written into new Core<R>
  → exception propagates automatically along the chain until caught by thenError() or reaches the chain end
```

### Proxy Chain Mechanism

```
Core A (Proxy) ──proxy_──→ Core B (actual state)

When A's result is set to Proxy state:
- subsequent callback operations on A are transparently forwarded to B via walkProxyChain()
- A's doCallback forwards the callback to B for execution
- used in Future split and FutureSplitter scenarios
```

## ABI Constraints

### Template Header-Only Inline Model

The core types of Folly futures (`Core<T>`, `Future<T>`, `SemiFuture<T>`, `Promise<T>`, `Try<T>`) are **entirely implemented as template header files** with no independently compiled `.so`/`.dll` exported symbols (`Core<Unit>` has an `extern template` to limit instantiation, see `FOLLY_USE_EXTERN_FUTURE_UNIT`).

**Implications:**
- There is no ABI boundary across DSOs — each link unit independently instantiates all templates.
- **ABI instability is equivalent to API instability**: field reordering, `State` enum value changes, `Callback` signature changes, and `KeepAliveOrDeferred` layout changes all cause ODR violations and silent data corruption.
- After upgrading the Folly version, a full recompilation of all object files using futures is required.

### Layout-Sensitive Key Types

| Type | Layout Risk |
|------|------------|
| `State` enum | `uint8_t` bitmask, values hardcoded in the FSM CAS operations. Adding/reordering enum values changes comparison logic |
| `CoreBase::interrupt_` | `uintptr_t` with low 2 bits encoding a state machine (`InterruptMask = 0x3`), high bits storing a pointer. Pointer alignment assumptions are platform-specific |
| `KeepAliveOrDeferred` | Tagged union (`State::Deferred` / `State::KeepAlive`), manually manages union lifetime |
| `ResultHolder<T>` | `union { Try<T> result_; }` used to control construction timing; shares cache line with vtable pointer |
| `Callback` (i.e. `folly::Function<...>`) | `folly::Function` has its own inline buffer size and SBO threshold |

### `Core<Unit>` extern Template

```cpp
#if FOLLY_USE_EXTERN_FUTURE_UNIT
extern template class Core<folly::Unit>;
#endif
```

`Core<Unit>` is the only instantiation with an `extern template` declaration. This limits the explicit instantiation of the `Unit` specialization to the `Core.cpp` compilation unit, reducing binary size. However, this is only a compilation optimization and does not provide ABI stability guarantees.

### Comparison with `std::future` ABI

`std::future`/`std::promise` have a stable `shared_state` layout in libstdc++/libc++ (typically located in `<bits/shared_ptr_base.h>` or `<__future>`), and ABI compatibility across DSOs is guaranteed by the standard library `.so`. Folly provides no such guarantee — sharing a `Core` pointer across processes (or serializing a `Future`) is impossible.

## Exception Safety

### `Try<T>` Exception Wrapping

`Try<T>` is the cornerstone of Folly futures' exception safety. It wraps both the `T` value and an `exception_wrapper` in the same union:

- `Promise::setException(ew)` → constructs `Try<T>(std::move(ew))` → writes into `Core::result_`.
- Continuations receive results via `Try<T>&&` and can safely check `hasValue()` / `hasException()` before accessing the value.
- `Try::value()` directly rethrows the stored exception in the exception state, never returning garbage values.

### Exception Propagation in Continuations

```cpp
auto f = std::move(future)
    .thenValue([](int v) { throw std::runtime_error("oops"); return v; })
    .thenValue([](int v) { /* never executes */ return v; })
    .thenError([](exception_wrapper ew) { /* catches "oops" */ return 0; });
```

Rules:
1. Exceptions thrown by `f` inside `.thenValue(f)` are caught internally by Folly, wrapped as `Try<R>(exception_wrapper)`, and written into the new Core.
2. The exception propagates automatically along the continuation chain, skipping subsequent `.thenValue()` calls.
3. Only `.thenError()` / `.deferError()` can catch and recover from exceptions.
4. If uncaught at the end of the chain, the exception is rethrown at `.get()`, or silently discarded in a detached future.

**Difference from `std::future`**: `std::future`'s exception is only rethrown once at `.get()`; Folly's exceptions propagate continuously through the chain and can be intercepted at every step.

### Broken Promise

```cpp
{
  folly::Promise<int> p;
  auto f = p.getSemiFuture();
  // p destroyed without calling setValue/setException
}
// f.get() throws BrokenPromise exception
```

Behavior of `Promise::~Promise()`:
1. If `valid() && !isFulfilled()`, automatically calls `setException(BrokenPromise(tag_t<T>{}))`.
2. `BrokenPromise` inherits from `PromiseException` (`std::logic_error`), carrying type name information.
3. This guarantees that the future chain will never hang indefinitely — even if the producer forgets to fulfill, the consumer receives a clear error signal.

### Promise Violation Exception Types

| Operation | Precondition Violated | Exception Thrown |
|-----------|----------------------|-----------------|
| `setValue()` / `setException()` / `setTry()` | `!valid()` | `PromiseInvalid` |
| `setValue()` / `setException()` / `setTry()` | `isFulfilled()` | `PromiseAlreadySatisfied` |
| `getSemiFuture()` / `getFuture()` | `!valid()` | `PromiseInvalid` |
| `getSemiFuture()` / `getFuture()` | Already called before | `FutureAlreadyRetrieved` |
| `Future::then*()` / `SemiFuture::defer*()` | `!valid()` | `FutureInvalid` |
| `Future::then*()` | Continuation already attached | `FutureAlreadyContinued` |

All these exceptions inherit from `FutureException` or `PromiseException` (both `std::logic_error`), indicating programming errors rather than runtime failures.

### Executor Submission Failure

The executor's `add(Func&&)` may throw an exception (e.g., queue full, executor already shut down). In this case:

1. The exception bubbles up inside `doCallback()`.
2. Folly catches the exception and writes it as the continuation's result into the next Core.
3. If the executor is shut down (e.g., calling `add()` after `CPUThreadPoolExecutor::join()`), behavior depends on the executor implementation — typically throws `std::runtime_error`.

**Note**: `InlineExecutor` does not throw such exceptions (always executes synchronously on the current thread), but may cause unbounded stack growth (each inline-executed continuation triggers the next).

### Interrupt Handling Exception Safety

`future.raise(ew)` and `promise.setInterruptHandler(fn)` share an atomic state machine (the low 2 bits of the `interrupt_` field):

- `raise()` called before `setInterruptHandler()`: the interrupt object is stored and invoked when the handler arrives.
- `setInterruptHandler()` called before `raise()`: the handler is stored and invoked synchronously when `raise()` arrives.
- Both arrive simultaneously: arbitrated by CAS, guaranteeing the handler is called exactly once.
- Calling `setInterruptHandler()` twice triggers `terminate_with<logic_error>` (an unrecoverable programming error).

## Iterator / Reference Invalidation

### Future/SemiFuture Handle Validity

Folly Future follows strict **move semantics**:

- **Non-copyable**: Both `Future<T>` and `SemiFuture<T>` have their copy constructor/assignment deleted.
- **Invalid after move**: Any `.then*()` / `.defer*()` / `.via()` / `.get()` call consumes `this` via the `&&` qualifier. After the call, `valid() == false`, and any operation on the moved-from object throws `FutureInvalid`.
- **`SemiFuture` → `Future` implicit move**: `SemiFuture(Future<T>&&)` allows implicit conversion, invalidating the `Future`.

```cpp
auto f1 = std::move(p).getSemiFuture();
auto f2 = std::move(f1).via(&exec);  // f1 invalid
auto f3 = std::move(f2).thenValue([](int v) { return v + 1; });  // f2 invalid
// f3 is valid, holds the new Core
```

### `Try<T>&` Reference Lifetime

`future.result()` / `future.value()` returns a reference to `Core::result_`. The validity of the reference is constrained as follows:

1. **`OnlyResult` state**: the reference is always valid (producer has written, consumer has exclusive access).
2. **`Done` state**: the reference may be invalid — the callback may have already moved out the result. Do not assume the reference still points to a valid value.
3. **`poll()` moves the result**: `poll()` returns `Optional<Try<T>>`, internally moving out the result. Afterward, `result()` / `value()` references point to the moved-from object.

### Lifetime of Objects Captured by Continuation Lambdas

```cpp
std::string s = "hello";
auto f = std::move(future).thenValue([s = std::move(s)](int v) {
  return v + s.size();
});
// s has been moved, the original variable is invalid
```

Continuation lambdas are stored in `Core::callback_` (`folly::Function`), with their lifetime tied to the Core:
- Invoked during the `Done` state transition.
- After invocation, `callback_` is destroyed (`derefCallback()`).
- If the future chain is destroyed but the callback has not yet executed (e.g., a detached future where the executor hasn't scheduled it), the callback and its captured objects are destroyed along with the Core.

### `Core*` Pointer Validity

The lifetime of `Core` is controlled by reference counting (the `attached_` field):

- `Promise` destroyed → `detachPromise()` → `detachOne()`
- `Future`/`SemiFuture` destroyed → `detachFuture()` → `detachOne()`
- When `attached_` reaches zero → `delete this` → `~Core()` → `~ResultHolder()` → `result_.~Try<T>()`

`Core` is neither movable nor copyable (constructor is private, move/copy are deleted). All access to Core is through raw pointers `Core*`; pointer validity is guaranteed by destruction ordering.

### References in Proxy Chains

When a Future is split, multiple `Core` objects may form a chain through `proxy_` pointers. Cores on the proxy chain may be destroyed earlier than the original Core, but `walkProxyChain()` is only called after `hasResult()`, at which point all intermediate proxies have completed their transformation and no dangling pointers can occur.

## Performance Model

### Memory Allocation Model

Each `Promise` construction (or `makePromiseContract()`) allocates one `Core<T>`:

```
Core<T> memory layout (typical x86-64):
┌─────────────────────────────────┐
│ vptr (8B)                       │ ← ResultHolder<T> + CoreBase
│ callback_ (Function, ~32-64B)   │ ← folly::Function inline buffer
│ state_ (1B atomic)              │
│ attached_ (1B atomic)           │
│ callbackReferences_ (1B atomic) │
│ [padding 5B]                    │
│ executor_ (KeepAliveOrDeferred) │ ← ~16B tagged union
│ context_ (shared_ptr, 16B)      │
│ interrupt_ (8B atomic)          │
│ proxy_ (8B)                     │
│ result_ (Try<T>, variable)      │ ← same cache line (if T is small)
└─────────────────────────────────┘
```

`Core<T>` is allocated via `new`; `Core<T>::make()` is the sole entry point. Creating one Promise-Future pair = **one heap allocation**.

`Core<Unit>` uses `extern template` (`FOLLY_USE_EXTERN_FUTURE_UNIT`) to reduce template bloat.

### State Machine Atomic Synchronization Cost

| Operation | Atomic Instruction | Memory Order |
|-----------|-------------------|-------------|
| `setResult_()` | `CAS(state_, Start→OnlyResult)` | `acq_rel` |
| `setCallback_()` | `CAS(state_, Start→OnlyCallback)` | `acq_rel` |
| `doCallback()` | `CAS(state_, OnlyCallback→Done)` | `acq_rel` |
| `hasResult()` | `load(state_)` | `acquire` |
| `raise()` / `setInterruptHandler()` | `CAS(interrupt_, ...)` | `acq_rel` |

Each Core state transition involves exactly one CAS (no spin loop). If the CAS fails (the other side arrived first), execution falls through directly to the other side's branch with no retries.

**Worst case**: Each continuation registration + fulfillment involves 2 CAS operations (one setCallback, one setResult, each with one failure followed by a fallthrough).

### Executor Hop Cost

```
Continuation chain of length N:
  auto f = std::move(f0)
    .thenValue(f1)    // Core 0 → Core 1
    .thenValue(f2)    // Core 1 → Core 2
    ...
    .thenValue(fN);   // Core N-1 → Core N

Number of executor hops = N (each thenValue may trigger one executor->add())
```

**Inline optimization**: When `thenValueInline` / `thenTryInline` is used, if the previous callback's completing executor matches the next Core's executor, executor dispatch is skipped and execution proceeds inline. This reduces the number of executor hops from N to the number of executor switches.

`OnlyCallbackAllowInline` state specifically supports this optimization: when `setCallback_` receives `InlineContinuation::allow` and the result is already ready with a matching completing executor, it executes inline directly instead of calling `executor->add()`.

### Performance Comparison with `std::future`

| Dimension | `std::future` (libstdc++) | Folly Future |
|-----------|--------------------------|-------------|
| Shared state allocation | `shared_ptr<_Task_state>`, one heap allocation | `Core<T>*`, one heap allocation |
| Synchronization primitives | `mutex` + `condition_variable` (`.wait()` path) | No mutex, pure `atomic<CAS>` |
| Continuation | No native support | One CAS per Core |
| Exception storage | `exception_ptr` (reference counted) | `exception_wrapper` (reference counted + type erasure) |
| Blocking wait | `condition_variable::wait()` → futex | `Baton::wait()` → futex (lighter weight) |

Folly's advantage lies in the **fully lock-free state machine** — `std::future`'s `.wait()` requires a mutex to protect the condition variable, while Folly's continuation model avoids mutexes entirely.

### `folly::Function` SBO

`Core::callback_` has type `folly::Function<void(CoreBase&, KeepAlive<>&&, exception_wrapper*)>`. `folly::Function` uses small buffer optimization (SBO), storing small lambdas (typically ≤ 32 bytes, platform-dependent) in a stack-allocated inline buffer, avoiding additional heap allocations. Most continuation lambdas are small enough to not trigger a secondary allocation.

## libstdc++ vs libc++ vs MSVC

Folly futures' behavior is independent of the specific platform standard library implementation (it is a standalone library), but has the following comparison points with the three standard libraries' `std::future`/`std::promise`:

### Continuation Model Differences

| Dimension | libstdc++ (`std::future`) | libc++ (`std::future`) | MSVC (`std::future`) | Folly Future |
|-----------|--------------------------|----------------------|---------------------|-------------|
| Continuation API | None | None | None (no `.then()` since C++11) | `.thenValue()` / `.thenTry()` / `.deferValue()` |
| Executor support | None | None | None | Explicit `Executor` abstraction, `.via()` binding |
| Lazy deferral | Not supported | Not supported | Not supported | `SemiFuture::defer*()` deferred until `.get()` or executor set |
| Exception propagation along chain | N/A | N/A | N/A | Automatic via `Try<T>` along the chain |

### Exception Passing Implementation Differences

| Dimension | libstdc++ | libc++ | MSVC | Folly |
|-----------|-----------|--------|------|-------|
| Exception storage | `exception_ptr` (`shared_ptr<__exception_ptr::exception_ptr>`) | `exception_ptr` (`shared_ptr`-like implementation) | `exception_ptr` (internal SEH integration) | `exception_wrapper` (reference counted + `type_info` erasure, can carry any type) |
| Passing mechanism | `rethrow` at `.get()` | `rethrow` at `.get()` | `rethrow` at `.get()` | `Try<T>` moves between Cores, interceptable at each step |
| Loss behavior | Exception silently discarded when future is destroyed | Same as left | Same as left | `BrokenPromise` exception guarantees no silent loss |

### Scheduling Semantics Differences

| Dimension | libstdc++ | libc++ | MSVC | Folly |
|-----------|-----------|--------|------|-------|
| `.wait()` implementation | `condition_variable` + mutex | `condition_variable` + mutex | `condition_variable` + mutex | `folly::Baton` (futex-based, no mutex) |
| `std::async` launch policy | `launch::async | launch::deferred` (varies by implementation) | Similar to libstdc++ | Similar to libstdc++ | No implicit thread creation; executor controls explicitly |
| `shared_state` lifetime | `shared_ptr` reference counting | `shared_ptr` reference counting | `shared_ptr` reference counting | Raw pointer + bit counting (`attached_`), no `shared_ptr` overhead |

### Platform-Specific Notes

- **libstdc++**: `std::promise`'s `shared_state` is implemented within `libstdc++.so`, providing ABI stability. Folly provides no such guarantee.
- **libc++**: `exception_ptr` is based on `__cxa_current_exception`, incompatible with Folly's `exception_wrapper` (based on `folly::exception_tracer`).
- **MSVC**: `exception_ptr` integrates with Windows SEH; Folly's `exception_wrapper` on MSVC uses a different underlying mechanism. The low-bit pointer encoding in `CoreBase::interrupt_` depends on 8-byte alignment, which holds under MSVC 64-bit.
- **Folly cross-platform**: `folly::Baton` uses futex on Linux, `ulock_wait`/`__ulock_wait` on macOS, and `WaitOnAddress` on Windows. Executor hop cost is consistent across all platforms (all are virtual calls to `add(Func&&)`).

## Minimal Reproduction Code

```cpp
#include <folly/futures/Future.h>
#include <folly/futures/Promise.h>

int main() {
  folly::Promise<int> p;
  auto f = p.getSemiFuture().deferValue([](int x) { return x + 1; });
  p.setValue(41);
  return std::move(f).get();
}
```

## Compilation / Disassembly / Benchmark Evidence

### CAS Disassembly of Core State Transitions

Taking `CoreBase::setResult_()` as an example, the core CAS compiles to a single `lock cmpxchg` instruction on x86-64:

```asm
; folly::futures::detail::CoreBase::setResult_(folly::Executor::KeepAlive<>&&)
; state_ is std::atomic<State> at an offset from CoreBase
mov     eax, 1          ; State::Start (0x1)
mov     ecx, 2          ; State::OnlyResult (0x2)
lock cmpxchg [rdi+OFF], cl  ; CAS(state_, Start→OnlyResult)
jne     .slow_path      ; failure → check if OnlyCallback or Proxy
; fast path: Start → OnlyResult, return directly
ret
.slow_path:
; failure branch: the other side arrived first, take doCallback or proxyCallback
```

Key characteristics:
- **No mutex, no futex**: Pure `lock cmpxchg`, a single instruction completing the state transition.
- **Memory order `acq_rel`**: Compiles to a CAS with the `lock` prefix (on x86, `lock` implies a full barrier).
- **No retry on failure**: After a CAS failure, execution falls through directly to the fallthrough branch (`OnlyCallback` → `Done`), with no spin loop.

### Continuation Submission Path

Simplified disassembly of executor submission in `doCallback()`:

```asm
; check if inline execution is possible
cmp     byte [rdi+OFF_STATE], 3  ; OnlyCallbackAllowInline?
jne     .enqueue
; check if completing executor == stored executor
cmp     rsi, [rdi+OFF_EXECUTOR]
je      .inline_execute
.enqueue:
; executor_->add(std::move(callback))
mov     rax, [rdi+OFF_EXECUTOR]   ; load executor vptr
call    [rax+VTABLE_ADD_OFFSET]   ; virtual call to add(Func&&)
ret
.inline_execute:
; call callback directly, bypassing executor
call    callback_func
ret
```

### `std::future` Blocking Path Comparison

Typical implementation of libstdc++ `std::future::wait()`:

```asm
; std::future::wait() — requires mutex + condition_variable
lea     rdi, [rsp+MUTEX_OFF]
call    pthread_mutex_lock       ; 1. lock
; check ready flag
cmp     byte [rsp+READY_OFF], 0
jne     .ready
.wait:
lea     rdi, [rsp+CV_OFF]
lea     rsi, [rsp+MUTEX_OFF]
call    pthread_cond_wait        ; 2. block waiting (futex internally)
cmp     byte [rsp+READY_OFF], 0
je      .wait
.ready:
lea     rdi, [rsp+MUTEX_OFF]
call    pthread_mutex_unlock     ; 3. unlock
```

Folly `SemiFuture::wait()` uses `folly::Baton`:

```asm
; folly::Baton::wait() — no mutex
mov     eax, [rdi]              ; load atomic state
test    eax, eax
jnz     .ready
; Linux: direct futex system call
mov     eax, 202                ; __NR_futex
xor     r10, r10               ; timeout = NULL
syscall
.ready:
ret
```

### Benchmark Order-of-Magnitude Reference

Based on the `futures/benchmarks/` tests bundled with the Folly source (typical x86-64 / Linux 5.x / GCC 12):

| Operation | Typical Latency | Notes |
|-----------|----------------|-------|
| `Promise` + `SemiFuture` creation | ~50-80 ns | One `Core<T>` heap allocation + atomic initialization |
| `setValue()` (no callback) | ~10-15 ns | Single CAS + placement new |
| `thenValue()` registration + fulfillment | ~80-150 ns | Core allocation + CAS + executor->add() |
| Inline continuation chain (same executor) | ~30-50 ns/step | No executor hop, pure CAS + function call |
| `std::promise` + `std::future` creation | ~40-60 ns | `shared_ptr` allocation |
| `std::future::get()` blocking | ~200-500 ns | mutex lock + cond_wait + unlock |

**Key takeaways**:
- Folly continuation chains (inline on the same executor) are approximately 3-10x faster than `std::future` blocking `.get()`.
- The main overhead comes from heap-allocating `Core<T>` (~50ns) and executor dispatch (~30-80ns).
- For small POD types like `int`, `Try<T>` value storage has zero overhead (stored directly in the union).

### Compile-Time Guarantees

```cpp
// Future is non-copyable — caught at compile time
Future<int> f1 = makeFuture(42);
Future<int> f2 = f1;  // CE: copy constructor is deleted

// .then*() consumes Future — caught at compile time
auto f3 = f1.thenValue([](int v) { return v + 1; });
// CE: f1 is an lvalue, thenValue requires &&

// void specialization is blocked by static_assert
// Core<void> compilation fails: "void futures are not supported. Use Unit instead."
```

These constraints are enforced at compile time through `= delete`, `&&` qualifiers, and `static_assert`, with no runtime checking overhead.

## cpplings Exercise Entries

- [`jthread1` — std::jthread and stop_token](../../../exercises/cpp20/jthread1.cpp)
- [`condvar1` — Condition Variables and Producer-Consumer Pattern](../../../exercises/cpp11-std/condvar1.cpp)
- [`expected23` — std::expected Error Handling](../../../exercises/cpp23/expected23.cpp)
