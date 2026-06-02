---
title: range-v3 管道运算符源码实现
topic: libraries
feature: pipe-operator
standard: C++20
status_checked_at: 2026-06-02
implementation:
  libcxx:
    path: references/impl/llvm-project/libcxx/include/__ranges/range_adaptor.h
    symbols:
      - __range_adaptor_closure
      - __pipeable
exercises: []
solutions: []
---
# range-v3 Pipe Operator Source Implementation

> Source path: `references/impl/llvm-project/libcxx/include/__ranges/range_adaptor.h`

## Three-Layer Implementation Mechanism

```
  vec | filter(pred) | transform(fn)

  Layer 1: __range_adaptor_closure<T>  (CRTP base class, tag dispatch)
       |
  Layer 2: __pipeable<Fn>  (wraps function object as pipeable closure)
       |
  Layer 3: Two operator| overloads
       |
       |  range | closure  ->  invoke(closure, range)
       |  closure | closure -> __pipeable(__compose(c2, c1))
```

### 1. `__range_adaptor_closure` — CRTP Base Class

```cpp
template <class _Tp>
  requires is_class_v<_Tp> && same_as<_Tp, remove_cv_t<_Tp>>
struct __range_adaptor_closure {};
```

An empty class used solely for tag dispatch. The concept constraint ensures that `T` only inherits `__range_adaptor_closure<T>` once, and that `T` is not a range (to avoid ambiguity).

### 2. `__pipeable` — EBO Function Object Wrapper

```cpp
template <class _Fn>
struct __pipeable : _Fn, __range_adaptor_closure<__pipeable<_Fn>> {
  constexpr explicit __pipeable(_Fn&& __f) : _Fn(std::move(__f)) {}
};
```

Inherits both the function object and the CRTP base class. EBO ensures that for stateless lambdas, `sizeof` is the same as a pointer.

### 3. Two `operator|` Overloads

**Overload 1: range | closure**

```cpp
template <ranges::range _Range, _RangeAdaptorClosure _Closure>
constexpr decltype(auto)
operator|(_Range&& __range, _Closure&& __closure) {
  return std::invoke(std::forward<_Closure>(__closure),
                     std::forward<_Range>(__range));
}
```

**Overload 2: closure | closure**

```cpp
template <_RangeAdaptorClosure _Closure, _RangeAdaptorClosure _OtherClosure>
constexpr auto operator|(_Closure&& __c1, _OtherClosure&& __c2) {
  return __pipeable(std::__compose(
      std::forward<_OtherClosure>(__c2),
      std::forward<_Closure>(__c1)));
}
```

`__compose(g, f)` returns `g(f(x))`. The result is wrapped back into `__pipeable`, so chaining continues without stopping.

## Partial Application: How Adaptors Accept Only One Argument

```cpp
struct __fn {
  // filter(rng, pred) → directly constructs filter_view
  template <class _Range, class _Pred>
  constexpr auto operator()(_Range&& __range, _Pred&& __pred) const {
    return filter_view(std::forward<_Range>(__range),
                       std::forward<_Pred>(__pred));
  }

  // filter(pred) → returns pipeable closure
  template <class _Pred>
  constexpr auto operator()(_Pred&& __pred) const {
    return __pipeable(std::__bind_back(*this, std::forward<_Pred>(__pred)));
  }
};
```

`__bind_back` captures the CPO + pred, generating a unary function equivalent to `[=](auto&& rng) { return (*this)(rng, pred); }`, which is then wrapped into `__pipeable`.
