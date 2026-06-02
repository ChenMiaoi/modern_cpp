---
title: "C++14 std::exchange"
topic: unknown
feature: exchange
standard: N/A
status_checked_at: 2026-06-02
---
# C++14 std::exchange

## Overview

`std::exchange` sets a variable to a new value while returning its old value. It is an atomic "replace-and-retrieve" operation, useful when implementing move semantics, state machine transitions, and container operations. Although it can be manually implemented in three lines, the standard library provides an efficient, general-purpose version.

## Syntax

```cpp
template <class T, class U = T>
constexpr T exchange(T& obj, U&& new_value);
```

- `obj`: the variable to be replaced
- `new_value`: the new value
- Returns the old value of `obj`

Header: `<utility>`

## Implementation Rationale

```cpp
// Simplified standard library implementation
template <class T, class U = T>
constexpr T exchange(T& obj, U&& new_value) {
    T old_value = std::move(obj);
    obj = std::forward<U>(new_value);
    return old_value;
}
```

## Code Examples

### Basic Usage

```cpp
#include <utility>
#include <iostream>

int main() {
    int x = 10;
    int old = std::exchange(x, 42);

    std::cout << "old: " << old << ", new: " << x << '\n';
    // Output: old: 10, new: 42
}
```

### Use in Move Constructors

```cpp
#include <utility>
#include <cstddef>

template <typename T>
class Buffer {
    T*     data_;
    size_t size_;
    size_t capacity_;

public:
    // Default constructor
    Buffer() : data_(nullptr), size_(0), capacity_(0) {}

    // Constructor, destructor, etc. omitted...

    // Move constructor: use exchange to leave the source in a safe state
    Buffer(Buffer&& other) noexcept
        : data_(std::exchange(other.data_, nullptr))
        , size_(std::exchange(other.size_, 0))
        , capacity_(std::exchange(other.capacity_, 0))
    {}

    // Equivalent without exchange (more verbose)
    // Buffer(Buffer&& other) noexcept
    //     : data_(other.data_), size_(other.size_), capacity_(other.capacity_)
    // {
    //     other.data_ = nullptr;
    //     other.size_ = 0;
    //     other.capacity_ = 0;
    // }
};
```

### Move Assignment Operator

```cpp
#include <utility>

template <typename T>
class UniqueHandle {
    T handle_;

public:
    UniqueHandle() : handle_(T{}) {}
    explicit UniqueHandle(T h) : handle_(h) {}

    ~UniqueHandle() { /* release handle_ */ }

    // Move assignment: swap-based for strong exception safety
    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        // First take over the new resource, then release the old one
        // exchange ensures this->handle_ gets the new value, other.handle_ becomes invalid
        T old = std::exchange(handle_, std::exchange(other.handle_, T{}));
        // old is destructed when it goes out of scope (via RAII)
        (void)old;
        return *this;
    }
};
```

### Replacing Manual Swap Patterns

```cpp
#include <utility>

// Traditional swap pattern
void traditional_swap(int& a, int& b) {
    int tmp = a;
    a = b;
    b = tmp;
}

// Equivalent using exchange
void exchange_swap(int& a, int& b) {
    b = std::exchange(a, b);
    // Equivalent to: tmp = a; a = b; b = tmp;
}

// More complex example: rotation
void rotate(int& a, int& b, int& c) {
    // a → b → c → a
    int old_a = std::exchange(a, std::exchange(b, std::exchange(c, a)));
    (void)old_a;
}
```

### State Machine Transitions

```cpp
#include <utility>
#include <string>

enum class State { Idle, Connecting, Connected, Disconnecting };

class Connection {
    State state_ = State::Idle;

public:
    // Returns old state, sets new state
    State transition(State new_state) {
        return std::exchange(state_, new_state);
    }

    void connect() {
        State prev = transition(State::Connecting);
        // prev is Idle — useful for logging or assertions
    }

    void disconnect() {
        State prev = transition(State::Disconnecting);
        if (prev != State::Connected) {
            // Unexpected state transition
        }
    }
};
```

### Working with Container Operations

```cpp
#include <vector>
#include <utility>

// Drain all elements from a vector
template <typename T>
std::vector<T> drain(std::vector<T>& vec) {
    return std::exchange(vec, {});  // Takes the contents, vec becomes empty
}

// Retrieve and replace configuration
struct Config {
    int timeout = 30;
    int retries = 3;
};

Config apply_config(Config& current, Config new_config) {
    return std::exchange(current, std::move(new_config));
}
```

### constexpr Usage

```cpp
#include <utility>

// C++14 constexpr supports local variable mutation
constexpr int example() {
    int x = 1;
    int old = std::exchange(x, 42);  // constexpr OK
    return old + x;  // 1 + 42 = 43
}

static_assert(example() == 43);
```

## Best Practices

1. **Move constructors should prefer `exchange`**: Setting each source object member to its default value via `exchange` is more concise and less error-prone than copying then manually clearing.
2. **Understand the return-old-value semantics**: The core value of `exchange` lies in simultaneously obtaining the old value and setting the new value; one-way assignment does not need it.
3. **`exchange` is not thread-safe**: It is not an atomic operation — it is merely a `move` + `assign` combination. For atomic operations, use `std::atomic::exchange`.
4. **Watch for move overhead**: For POD types (int, pointers, etc.), `exchange` compiles to zero overhead; for complex types, ensure the move constructor is efficient.
5. **State machine scenarios are a natural fit**: Any pattern of "save old value, set new value, decide behavior based on old value" can be simplified with `exchange`.
6. **Do not use for `std::swap`**: Although `exchange` can implement swap, `std::swap` has clearer semantics and is optimized. `exchange` is better suited for one-way replacement scenarios.
