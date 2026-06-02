---
title: libc++ Ranges and unique_ptr
topic: libraries
feature: ranges-unique-ptr
standard: C++20
status_checked_at: 2026-06-02
implementation:
  libcxx:
    paths:
      - references/impl/llvm-project/libcxx/include/ranges
      - references/impl/llvm-project/libcxx/include/__memory/unique_ptr.h
    symbols:
      - __range_adaptor_closure
      - __pipeable
      - std::unique_ptr
      - __unique_ptr_deleter_sfinae
exercises: []
solutions: []
---
# libc++ Ranges and unique_ptr

## Ranges: Pipe Operator

For detailed implementation, see the [range-v3 pipe operator chapter](/libraries/range-v3/pipe-operator).

libc++'s Ranges implementation evolved directly from range-v3, with the same core mechanism:
- CRTP base class `__range_adaptor_closure<T>` for tag dispatch
- `__pipeable<Fn>` wraps function objects
- Two `operator|` overloads: range|closure and closure|closure

## std::unique_ptr: compressed_pair

```
sizeof(unique_ptr<T, default_delete<T>>) = 8 bytes (64-bit)

  _LIBCPP_COMPRESSED_PAIR expansion:
  ┌──────────────────────────────────┐
  │ [[no_unique_address]] T* __ptr_  │  ← 8 bytes
  │ [[no_unique_address]] default_   │  ← 0 bytes (empty type)
  │            delete<T> __deleter_  │
  └──────────────────────────────────┘
  sizeof = 8
```

**trivial_abi**: libc++ marks `unique_ptr` with `[[clang::trivial_abi]]`, allowing it to be passed in registers (instead of via an implicit pointer on the stack), reducing function call overhead. GCC and MSVC do not support this attribute.
