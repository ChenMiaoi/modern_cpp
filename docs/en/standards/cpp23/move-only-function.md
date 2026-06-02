---
title: "std::move_only_function"
topic: unknown
feature: move-only-function
standard: N/A
status_checked_at: 2026-06-02
---
# std::move_only_function

C++23 introduces `std::move_only_function`, a move-only callable wrapper that resolves the limitation of `std::function` requiring wrapped callables to be copyable.

## Basic Usage

```cpp
#include <functional>
#include <memory>
#include <iostream>

int main() {
    auto ptr = std::make_unique<int>(42);
    std::move_only_function<int()> fn = [p = std::move(ptr)]() {
        return *p;
    };
    std::cout << fn() << "\n";  // 42

    auto fn2 = std::move(fn);  // Move construction
    // fn is now empty
}
```

## Comparison with std::function

```cpp
auto ptr = std::make_unique<int>(42);

// std::function — compile error! unique_ptr is not copyable
// std::function<int()> bad = [p = std::move(ptr)]() { return *p; };

// move_only_function — valid
std::move_only_function<int()> good = [p = std::move(ptr)]() { return *p; };
```

| Property | `std::function` | `std::move_only_function` |
|----------|-----------------|--------------------------|
| Copyable | Yes | No (move-only) |
| Stores move-only callable | No | Yes |
| Call on empty throws | `bad_function_call` | `bad_function_call` |
| Small object optimization | Yes | Yes |

## Function Signatures

```cpp
std::move_only_function<int(int, int)> add = [](int a, int b) { return a + b; };
std::move_only_function<void(const std::string&)> printer =
    [](const std::string& s) { std::cout << s << "\n"; };
std::move_only_function<double()> rand_gen =
    [engine = std::mt19937{}]() mutable {
        return std::uniform_real_distribution<>(0.0, 1.0)(engine);
    };

// noexcept version
std::move_only_function<int() noexcept> safe = []() noexcept { return 42; };
```

## const and Reference Qualifiers

```cpp
std::move_only_function<int() const> cfn = []() { return 1; };
std::move_only_function<int() &> lfn = []() { return 1; };
```

## Practical Application Scenarios

### Asynchronous Callbacks

```cpp
#include <functional>
#include <queue>
#include <memory>

class TaskQueue {
    std::queue<std::move_only_function<void()>> tasks_;
public:
    void push(std::move_only_function<void()> task) {
        tasks_.push(std::move(task));
    }
    void run_all() {
        while (!tasks_.empty()) { tasks_.front()(); tasks_.pop(); }
    }
};

TaskQueue queue;
auto resource = std::make_unique<Database>();
queue.push([r = std::move(resource)]() { r->query("SELECT 1"); });
queue.run_all();
```

### scope_exit Pattern

```cpp
class scope_exit {
    std::move_only_function<void()> action_;
public:
    explicit scope_exit(std::move_only_function<void()> a) : action_(std::move(a)) {}
    ~scope_exit() { if (action_) action_(); }
    scope_exit(const scope_exit&) = delete;
    scope_exit& operator=(const scope_exit&) = delete;
    scope_exit(scope_exit&&) = default;
    scope_exit& operator=(scope_exit&&) = default;
};

void process() {
    auto* fd = open_file("data.txt");
    scope_exit cleanup([fd]() { close_file(fd); });
    // ... use fd
}  // cleanup executes the action on destruction
```

## noexcept Semantics

```cpp
std::move_only_function<int() noexcept> safe;
std::move_only_function<int()> maybe_throws;

// Different types, cannot be assigned to each other
// safe = maybe_throws;  // Compile error
```

## Alternative Approaches Comparison

```cpp
// 1. move_only_function (recommended)
std::move_only_function<int()> fn = [p = std::move(ptr)]() { return *p; };

// 2. Template parameter (zero overhead but cannot store heterogeneous callables)
template <typename F> void call(F&& f) { f(); }

// 3. shared_ptr wrapping to bypass the limitation (extra overhead)
std::function<int()> fn2 = [p = std::shared_ptr<int>(std::move(ptr))]() {
    return *p;
};
```

## Caveats

- Calling an empty `move_only_function` throws `std::bad_function_call`; use `operator bool` to check
- A moved-from object is in a valid but unspecified state (typically empty)
- SBO (small object optimization) means small lambdas do not allocate heap memory
- Supports stateful `mutable` lambdas
