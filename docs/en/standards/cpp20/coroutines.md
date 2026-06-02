---
title: "C++20 Coroutines"
topic: unknown
feature: coroutines
standard: N/A
status_checked_at: 2026-06-02
---
# C++20 Coroutines

## Overview

C++20 introduces language-level stackless coroutines, controlling coroutine **suspension** and **resumption** through three keywords: `co_await`, `co_yield`, and `co_return`. The compiler saves coroutine execution state into a heap-allocated coroutine frame, which can be resumed at any time after suspension. The standard library did not provide `std::generator` until C++23; C++20's coroutine framework is a customizable low-level primitive — production use requires custom return types or third-party libraries (e.g., `cppcoro`, Boost.Asio). Core value: writing asynchronous logic in synchronous style, lazy generators (on-demand computation, O(1) memory), and high-concurrency task scheduling (no thread-switching overhead).

## Coroutine Keywords and Suspension Points

Three keywords define coroutine behavior: `co_await expr` suspends waiting for an Awaiter to be ready; `co_yield expr` is equivalent to `co_await promise.yield_value(expr)` producing a value and suspending; `co_return expr` calls `return_value(expr)` (or `return_void()` when parameterless) and terminates the coroutine body.

The evaluation of `co_await expr` constitutes a **suspension point**. The compiler calls three methods following the Awaiter protocol:

```cpp
struct Awaiter {
    bool await_ready() const noexcept;     // true → skip suspension, continue immediately
    // await_suspend with three return types:
    //   void              → unconditional suspension
    //   bool              → resume immediately when false (conditional suspension)
    //   coroutine_handle  → symmetric transfer (see below)
    auto await_suspend(std::coroutine_handle<>) const noexcept;
    auto await_resume() const noexcept;    // value returned after resumption
};
```

The standard library provides `std::suspend_always` (always suspends) and `std::suspend_never` (never suspends).

```cpp
Generator<int> fibonacci() {
    int a = 0, b = 1;
    while (true) { co_yield a; auto tmp = a; a = b; b = tmp + b; }
}
Task<int> compute() {
    int x = co_await async_read();  // suspend waiting for async I/O
    co_return x * 2;
}
```

## promise_type

Every coroutine return type must have an embedded or trait-associated `promise_type` that defines the complete coroutine lifecycle protocol:

```cpp
struct Task {
    struct promise_type {
        int result{};
        Task get_return_object() {                    // construct the return object
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }  // lazy start
        std::suspend_always final_suspend() noexcept { return {}; }    // final suspension
        void return_value(int v) { result = v; }      // co_return expr
        // void return_void() noexcept {}              // co_return; or no co_return
        std::suspend_always yield_value(int v) {       // co_yield expr
            result = v; return {};
        }
        void unhandled_exception() { std::terminate(); }
    };
    std::coroutine_handle<promise_type> handle;
};
```

Key constraint: `return_value` and `return_void` **cannot coexist**; the presence of `co_yield` implicitly allows `return_void`. The Awaiter returned by `final_suspend()` must be suspendable (`await_ready()` returns `false`); otherwise the coroutine frame is automatically destroyed and the handle becomes invalid (UB).

## coroutine_handle

`std::coroutine_handle<Promise>` is a non-owning handle providing operations on the coroutine frame:

```cpp
auto h = task.handle;
h.resume();       // resume execution from suspension point
h.done();         // query whether at final suspension point
h.destroy();      // destroy coroutine frame (must be in suspended state)
h.promise();      // access the promise object (requires concrete Promise type)
// Type-erased version: can resume, cannot access promise
std::coroutine_handle<void> erased = h;
// No-op handle, used to terminate symmetric transfer chains
std::coroutine_handle<> noop = std::noop_coroutine();
```

`coroutine_handle` does not manage lifetime. The typical approach is an RAII wrapper that calls `destroy()` in its destructor.

## Generator Pattern

C++20 requires hand-written generators; C++23 provides `std::generator<T>`:

```cpp
template <typename T>
class Generator {
public:
    struct promise_type {
        T current{};
        Generator get_return_object() {
            return Generator{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() { std::terminate(); }
        std::suspend_always yield_value(T value) {
            current = std::move(value); return {};
        }
    };
    struct iterator {
        std::coroutine_handle<promise_type> h;
        iterator& operator++() { h.resume(); return *this; }
        const T& operator*() const { return h.promise().current; }
        bool operator==(std::default_sentinel_t) const { return !h || h.done(); }
    };
    iterator begin() { if (handle) handle.resume(); return {handle}; }
    std::default_sentinel_t end() { return {}; }
    ~Generator() { if (handle) handle.destroy(); }
    Generator(Generator&& o) noexcept : handle(o.handle) { o.handle = nullptr; }
    Generator(const Generator&) = delete;
private:
    explicit Generator(std::coroutine_handle<promise_type> h) : handle(h) {}
    std::coroutine_handle<promise_type> handle;
};
Generator<int> range(int lo, int hi) {
    for (int i = lo; i < hi; ++i) co_yield i;
}
// for (int v : range(0, 10)) { /* 0, 1, ..., 9 */ }
```

`begin()` performs the first `resume()` advancing to the first `co_yield`; each `++` advances to the next suspension point. In C++23, `std::generator<T>` can be used directly in place of the hand-written version.

## Async Pattern

Wrap low-level asynchronous operations as Awaiters to write asynchronous code in synchronous style:

```cpp
struct AsyncReadAwaiter {
    int fd;
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) {
        register_io_callback(fd, [h]() { h.resume(); });
    }
    ssize_t await_resume() { return get_read_result(fd); }
};
Task<ssize_t> handle_connection(int fd) {
    ssize_t n = co_await AsyncReadAwaiter{fd};  // suspend waiting for I/O ready
    co_return n;
}
```

## Symmetric Transfer

Nested `resume()` calls cause the call stack to grow linearly. Symmetric transfer lets `await_suspend` return a `coroutine_handle`, and the compiler optimizes it as a tail call, keeping stack depth constant:

```cpp
struct Task {
    struct promise_type {
        std::coroutine_handle<> continuation{};
        // ... get_return_object, initial_suspend, return_void, unhandled_exception ...
        auto final_suspend() noexcept {
            struct FinalAwaiter {
                bool await_ready() noexcept { return false; }
                std::coroutine_handle<> await_suspend(
                    std::coroutine_handle<promise_type> h) noexcept {
                    return h.promise().continuation
                        ? h.promise().continuation : std::noop_coroutine();
                }
            };
            return FinalAwaiter{};
        }
    };
    std::coroutine_handle<promise_type> handle;
    bool await_ready() const noexcept { return false; }
    std::coroutine_handle<promise_type> await_suspend(
        std::coroutine_handle<> caller) noexcept {
        handle.promise().continuation = caller;
        return handle;  // symmetric transfer: direct jump, no call stack growth
    }
};
```

`final_suspend` returning `continuation` hands control back to the caller, avoiding stack overflow from `A.resume() → B.resume() → C.resume()` chains.

## Best Practices

1. **Lazy start**: `initial_suspend()` returns `suspend_always` to avoid races caused by execution advancing to the suspension point immediately after construction.
2. **Final suspension**: `final_suspend()`'s `await_ready()` must return `false`; otherwise accessing the handle after automatic destruction is UB.
3. **Exception propagation**: Capture and store exceptions in `unhandled_exception()` using `std::current_exception()`, and rethrow in `await_resume()`.
4. **RAII manage handles**: Call `destroy()` in the return object's destructor; no bare handle leaks.
5. **Symmetric transfer**: Long coroutine chains must use symmetric transfer to avoid stack overflow, with `noop_coroutine()` as the termination sentinel.
6. **HALO verification**: The compiler may allocate the coroutine frame on the caller's stack (HALO optimization), but this is an optimization, not a guarantee; benchmark performance-critical paths.

## Common Pitfalls

1. **Dangling references**: `co_await` is a potential suspension point; after suspension, all externally referenced objects captured by reference must still be alive:
   ```cpp
   Task dangerous(const std::string& input) {
       co_await some_async_op();  // input may have been destroyed externally
       process(input);            // UB
   }
   // Fix: pass parameters by value, or copy to local variables before suspension
   ```
2. **Heap allocation and HALO**: Coroutine frames are typically heap-allocated by the compiler via `operator new`. HALO can eliminate heap allocation when the lifetime can be statically analyzed, but lazy coroutines are harder to optimize than eagerly executed ones.
3. **`initial_suspend` and races**: When using `suspend_never` for immediate execution, the caller may not be ready yet. Asynchronous tasks **strongly recommend lazy start**.
4. **Missing `promise_type` members**: Missing any required member (`get_return_object`, `initial_suspend`, `final_suspend`, `return_value` or `return_void`, `unhandled_exception`) will produce cryptic template errors. It is recommended to start from a minimal compilable template and expand incrementally.
5. **`done()` misinterpretation**: When a coroutine is suspended at `initial_suspend`, `done()` returns `false` but no values have been produced; when suspended at `final_suspend`, `done()` returns `true`. Iterators must check both `h.done()` and handle validity.
