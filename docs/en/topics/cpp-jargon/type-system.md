---
title: "Type System Terminology"
topic: unknown
feature: type-system
standard: N/A
status_checked_at: 2026-06-02
---
# Type System Terminology

## Type Erasure

Hiding concrete type information behind a uniform interface — `std::function`, `std::any`, and `std::shared_ptr` all use type erasure.

```cpp
// Simplified implementation concept of std::function:
template<typename Ret, typename... Args>
class function<Ret(Args...)> {
  // each concrete callable type is erased into a uniform interface
  struct concept_base {
    virtual Ret invoke(Args...) = 0;
    virtual ~concept_base() = 0;
  };

  template<typename F>
  struct model : concept_base {
    F f_;
    Ret invoke(Args... args) override { return f_(args...); }
  };

  concept_base* ptr_;  // points to model<F> on the heap
};

// the type the user sees: function<int(int)>
// the type actually stored: model<concrete_lambda_type>*
// the type information has been "erased"
```

## Type Punning

Interpreting the same block of memory through different types. There are safe and unsafe forms:

```cpp
// Unsafe — UB (violates strict aliasing)
float f = 3.14f;
int i = *(int*)&f;  // UB!

// Safe via union (still controversial in C++, but GCC/Clang support it)
union { float f; int i; } u;
u.f = 3.14f;
int i2 = u.i;  // allowed by GCC/Clang; the standard's stance is unclear

// Safest — C++20
int i3 = std::bit_cast<int>(f);  // explicitly supported, no UB
```

## Type Traits

Compile-time type queries and transformations:

```cpp
// Queries
std::is_same_v<int, int>        // true
std::is_integral_v<double>      // false
std::is_trivially_copyable_v<std::string>  // false

// Transformations
std::remove_const_t<const int>  // int
std::add_pointer_t<int>         // int*
std::decay_t<const int&>        // int (removes reference and cv qualifiers)
```

## Tag Dispatching

Selecting the optimal implementation path at compile time via empty type tags:

```cpp
template<typename Iter>
void advance(Iter& it, int n, std::random_access_iterator_tag) {
  it += n;  // O(1)
}

template<typename Iter>
void advance(Iter& it, int n, std::input_iterator_tag) {
  while (n--) ++it;  // O(n)
}

template<typename Iter>
void advance(Iter& it, int n) {
  advance(it, n, typename std::iterator_traits<Iter>::iterator_category{});
  // selects the optimal path at compile time based on iterator type
}
```

## Polymorphism

### Static Polymorphism

Polymorphism resolved at compile time — CRTP, Concepts, overloading:

```cpp
template<typename T>
void process(T& obj) {
  obj.do_something();  // which do_something is determined at compile time
}
```

### Dynamic Polymorphism

Polymorphism resolved at runtime — virtual functions:

```cpp
void process(Base& obj) {
  obj.do_something();  // dispatched via vtable at runtime
}
```

| Dimension | Static Polymorphism | Dynamic Polymorphism |
|-----------|-------------------|---------------------|
| Binding time | Compile time | Runtime |
| Overhead | Zero (can be inlined) | vtable indirect call |
| Heterogeneous containers | Difficult | Easy (`vector<Base*>`) |
| Code bloat | One instantiation per type | Shared virtual function code |
