---
title: "C++ Jargon Encyclopedia"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# C++ Jargon Encyclopedia

> The C++ community is full of specialized terminology — these terms appear repeatedly in standard documents, compiler implementations, library designs, and engineer conversations, yet few people explain them systematically. This topic attempts to dig up all this "jargon" and explain each one.

These terms were not invented out of thin air — each one corresponds to a concrete language mechanism, an observable behavioral difference, or a subtle rule that affects code correctness. Understanding them means understanding how C++ works.

---

## Terminology Category Navigation

### [Value Categories & Expressions](/topics/cpp-jargon/value-categories)
lvalue, prvalue, xvalue, glvalue, rvalue, materialization, temporary materialization

### [Object Model & Memory](/topics/cpp-jargon/object-model)
lifetime, storage duration, alignment, object representation, pointer invalidation, dangling reference, strict aliasing, placement new, std::launder

### [Overload Resolution & Name Lookup](/topics/cpp-jargon/overload-resolution)
overload resolution, ADL (Argument-Dependent Lookup), name hiding, two-phase lookup, dependent name, name mangling

### [Template Mechanics](/topics/cpp-jargon/template-mechanics)
SFINAE, CRTP, CTAD, deduction guide, explicit specialization, partial specialization, variadic template, parameter pack, fold expression, expression template, template template parameter, requires clause, concept, subsumption, if constexpr

### [Type System](/topics/cpp-jargon/type-system)
type erasure, type punning, type traits, tag dispatching, polymorphism (static/dynamic), covariance, contravariance, invariant, UB/type mismatch

### [Construction, Destruction & Special Members](/topics/cpp-jargon/special-members)
Rule of Zero/Three/Five, copy elision, NRVO, RVO, guaranteed copy elision, trivially copyable, trivially relocatable, aggregate initialization, brace elision

### [Exception Safety](/topics/cpp-jargon/exception-safety)
exception safety guarantee (basic/strong/nothrow), RAII, scope guard, noexcept, stack unwinding, exception specification

### [Concurrency & Memory Model](/topics/cpp-jargon/concurrency-terms)
data race, race condition, happens-before, sequenced before, memory order (relaxed/acquire/release/seq_cst), atomic, lock-free, ABA problem, false sharing, cache line, memory barrier

### [Compilation & Linking](/topics/cpp-jargon/compilation)
translation unit, ODR (One Definition Rule), linkage (internal/external/no), static initialization order fiasco, ABI, mangling, PCH, LTO, include guard, forward declaration, PImpl

### [Optimization & Performance Idioms](/topics/cpp-jargon/optimization-terms)
copy elision, RVO, NRVO, small buffer optimization (SBO), small string optimization (SSO), copy-on-write (COW), expression template, lazy evaluation, branch prediction, devirtualization, cache-friendly, prefetch, inline, LTO

### [Standard Library Idioms](/topics/cpp-jargon/stdlib-idioms)
RAII handle, sentinel, range, view, pipe operator, CPO (Customization Point Object), niebloid, tag_invoke, allocator model, PMR, smart pointer (unique/shared/weak)

### [UB & Safety](/topics/cpp-jargon/ub-safety)
undefined behavior, implementation-defined, unspecified behavior, nasal demons, signed overflow, null dereference, use-after-free, buffer overflow, strict aliasing violation, std::launder

---

## Top 30 Terms by Usage Frequency

| Rank | Term | One-Line Explanation |
|------|------|----------------------|
| 1 | **RAII** | Resources are acquired at construction and released at destruction — the cornerstone of C++ |
| 2 | **Move Semantics** | `std::move` doesn't move anything; it merely casts an lvalue to an rvalue reference |
| 3 | **SFINAE** | Template substitution failure is not an error — it falls back to other overloads |
| 4 | **Value Categories** | lvalue has an address and is addressable, prvalue is a pure value, xvalue is an "expiring value" |
| 5 | **ADL** | The compiler looks up functions in the namespace where the argument types are defined |
| 6 | **ODR** | Each entity in the entire program must have exactly one definition |
| 7 | **Copy Elision** | Since C++17, prvalues don't create temporary objects — they construct directly at the destination |
| 8 | **SBO/SSO** | Small objects/strings are stored inline on the stack, avoiding heap allocation |
| 9 | **noexcept** | Promises not to throw — the compiler optimizes based on this; move operations should be marked |
| 10 | **CRTP** | The base class template parameter is the derived class — compile-time polymorphism |
| 11 | **Type Erasure** | The core technique behind how `std::function` stores any callable |
| 12 | **Perfect Forwarding** | `std::forward<T>` preserves the value category of arguments |
| 13 | **Exception Safety** | Three levels of guarantee: basic/strong/nothrow |
| 14 | **CTAD** | C++17 Class Template Argument Deduction — no more need for `make_xxx` |
| 15 | **Concept** | C++20 named constraints on template parameters, replacing SFINAE black magic |
| 16 | **Vtable** | `virtual` functions dispatch at runtime through a function pointer table |
| 17 | **Iterator Invalidation** | Which iterators remain valid after container operations |
| 18 | **Rule of Five** | If you customize any one of destructor/copy/move, you usually need to define all five |
| 19 | **happens-before** | The ordering relationship in the C++ memory model that determines operation visibility |
| 20 | **PImpl** | Pointer to implementation — hides implementation details, reduces compile dependencies |
| 21 | **EBO** | Empty Base Optimization — empty type members take no space |
| 22 | **Expression Template** | A deferred computation template technique that avoids intermediate temporary objects |
| 23 | **NRVO** | Named Return Value Optimization — the compiler constructs the return object directly in the caller's stack frame |
| 24 | **Tag Dispatching** | Selects the optimal implementation path via empty type tags |
| 25 | **strict aliasing** | Pointers of different types cannot point to the same memory (except for allowed exceptions) |
| 26 | **UB** | The compiler can do anything with undefined behavior — including "working correctly" |
| 27 | **constexpr** | Functions/variables that can be evaluated at compile time |
| 28 | **Type Traits** | Compile-time type queries like `std::is_same`, `std::enable_if`, etc. |
| 29 | **Dependent Name** | Names in templates that depend on template parameters — require `typename` for disambiguation |
| 30 | **Two-Phase Lookup** | Non-dependent names are looked up at template definition; dependent names are looked up at instantiation |
