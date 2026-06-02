---
title: "Construction, Destruction & Special Members"
topic: unknown
feature: special-members
standard: N/A
status_checked_at: 2026-06-02
---
# Construction, Destruction & Special Members

## Rule of Zero / Three / Five

### Rule of Zero

If a class does not manage resources, do not define any special member functions — let the compiler generate them:

```cpp
struct Point {
  double x, y;
  // compiler auto-generates: copy constructor, copy assignment, move constructor, move assignment, destructor
};
```

### Rule of Three (C++98)

If you define any one of **destructor**, **copy constructor**, or **copy assignment**, you usually need to define all three.

### Rule of Five (C++11)

After C++11 added move semantics, this expanded to five: destructor + copy constructor + copy assignment + **move constructor** + **move assignment**.

```cpp
class Buffer {
  size_t size_;
  char* data_;
public:
  ~Buffer() { delete[] data_; }                                    // 1. destructor
  Buffer(const Buffer& o) : size_(o.size_), data_(new char[o.size_]) // 2. copy constructor
    { std::copy(o.data_, o.data_+size_, data_); }
  Buffer& operator=(const Buffer& o) { ... }                        // 3. copy assignment
  Buffer(Buffer&& o) noexcept : size_(o.size_), data_(o.data_)      // 4. move constructor
    { o.size_ = 0; o.data_ = nullptr; }
  Buffer& operator=(Buffer&& o) noexcept { ... }                    // 5. move assignment
};
```

## Copy Elision

### RVO (Return Value Optimization)

```cpp
std::string make() {
  return std::string("hello");  // C++17: constructed directly in the caller's location, no copy
}
```

### NRVO (Named Return Value Optimization)

```cpp
std::string make() {
  std::string s = "hello";
  return s;  // compiler may construct s directly in the caller's location (optional optimization)
}
```

**C++17 changes**: RVO is **mandatory** (guaranteed copy elision — prvalues don't create temporary objects). NRVO is still an optional optimization.

## Trivially Copyable

If a type can be safely copied with `memcpy` (no custom copy constructor, no virtual functions, etc.), it is trivially copyable. This is a prerequisite for `memcpy` optimization:

```cpp
static_assert(std::is_trivially_copyable_v<int>);         // ✓
static_assert(std::is_trivially_copyable_v<std::unique_ptr<int>>);  // ✓
static_assert(!std::is_trivially_copyable_v<std::string>); // ✗ has custom destructor
```

## Trivially Relocatable (Proposal Stage)

If a type can perform "move + destroy source" via `memcpy`, it is trivially relocatable. libc++ already uses this concept to optimize vector's resize:

```cpp
// unique_ptr is trivially relocatable:
// memcpy(dst, src, sizeof(unique_ptr)) is equivalent to:
// new(dst) unique_ptr(std::move(*src)); src->~unique_ptr();
```

## Aggregate Initialization

Aggregate types (no user-provided constructors, no private members, no virtual functions, etc.) can be initialized directly with braces:

```cpp
struct Point { double x; double y; };
Point p = {1.0, 2.0};  // aggregate initialization

// C++17: aggregate initialization can deduce template parameters
template<typename T, typename U>
struct Pair { T first; U second; };
Pair pr{42, 3.14};  // CTAD: Pair<int, double>
```

## Importance of noexcept

Move operations must be marked `noexcept`; otherwise, many move optimizations in the standard library will not take effect:

```cpp
// Inside std::vector::resize:
// if T's move constructor is noexcept → use move
// otherwise → fall back to copy (to guarantee strong exception safety)

// if your type is not marked noexcept → vector resize always uses copy → performance disaster
Buffer(Buffer&& o) noexcept;  // must be marked!
```
