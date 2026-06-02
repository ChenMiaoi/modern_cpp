---
title: "Rvalue References and Move Semantics"
topic: unknown
feature: move-semantics
standard: N/A
status_checked_at: 2026-06-02
---
# Rvalue References and Move Semantics

## Core Problem

Before C++11, returning large objects from functions or swapping two objects inevitably triggered deep copies:

```cpp
std::vector<int> create_data() {
    std::vector<int> v(1000000, 42);
    return v;  // C++98: deep-copies the entire vector
}
```

The essence of move semantics: **transfer resources (heap memory, file handles, etc.) from one object to another by "stealing" rather than copying.**

## Value Categories Basics

| Value Category | Meaning | Example |
|---------------|---------|---------|
| lvalue | Has identity, addressable | Variables, `*p`, `++i` |
| xvalue | Has identity, about to be destroyed | `std::move(x)`, `static_cast<T&&>(x)` |
| prvalue | No identity, temporary value | Literal `42`, `a + b`, function return values |

Rvalue references (`T&&`) can only bind to xvalues and prvalues.

## Move Constructor and Move Assignment Operator

```cpp
class Buffer {
    int* data_;
    size_t size_;
public:
    // Move constructor
    Buffer(Buffer&& other) noexcept
        : data_(other.data_), size_(other.size_)
    {
        other.data_ = nullptr;  // Null out the source object
        other.size_ = 0;
    }

    // Move assignment operator
    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            delete[] data_;        // Release own resources
            data_ = other.data_;   // Steal the other's resources
            size_ = other.size_;
            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    ~Buffer() { delete[] data_; }
};
```

## `std::move`

`std::move` itself does not move anything — it merely casts an lvalue to an rvalue reference, signaling "I no longer need this object, you may move its resources":

```cpp
std::string s1 = "hello";
std::string s2 = std::move(s1);  // s1's contents are moved to s2
// s1 is now in a "valid but unspecified" state
```

## Perfect Forwarding

`std::forward<T>(arg)` determines whether to forward as an lvalue or rvalue reference based on the template parameter `T`:

```cpp
template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
```

Reference collapsing rules:
- `T& &` → `T&`
- `T& &&` → `T&`
- `T&& &` → `T&`
- `T&& &&` → `T&&`

## Economics of Move Semantics

```cpp
// Scenario 1: Returning a local object (compiler automatically moves / RVO)
std::vector<int> make_vec() {
    std::vector<int> v = {1, 2, 3};
    return v;  // move or RVO, zero copies
}

// Scenario 2: Inserting into a container
std::vector<std::string> vec;
std::string s = "hello world";
vec.push_back(std::move(s));  // move instead of copy

// Scenario 3: Swapping
std::string a = "hello", b = "world";
std::swap(a, b);  // internally uses std::move, three moves instead of three deep copies
```

## Importance of `noexcept`

Move operations **must** be marked `noexcept`. Standard library containers (e.g., during `vector` reallocation) only use move when the move constructor promises not to throw:

```cpp
class Widget {
public:
    Widget(Widget&&) noexcept;            // ✓ vector will move during reallocation
    Widget(Widget&&);                     // ✗ vector will copy during reallocation!
};
```

## When Not to Use `std::move`

```cpp
// Wrong: using move on a return value prevents RVO (Return Value Optimization)
std::vector<int> make_vec() {
    std::vector<int> v = {1, 2, 3};
    return std::move(v);  // Don't do this! Just `return v;` is better
}
```

## Rule of Five

If you define any one of the following five special member functions, you typically need to define all of them:

1. **Destructor**
2. **Copy constructor**
3. **Copy assignment operator**
4. **Move constructor**
5. **Move assignment operator**

```cpp
class Resource {
public:
    ~Resource();
    Resource(const Resource&);
    Resource& operator=(const Resource&);
    Resource(Resource&&) noexcept;
    Resource& operator=(Resource&&) noexcept;
};
```

## Relationship with Smart Pointers

- `std::unique_ptr`: move-only, not copyable
- `std::shared_ptr`: copyable and movable, moving is cheaper than copying (one fewer atomic operation)
