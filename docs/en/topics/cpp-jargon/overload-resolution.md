---
title: "Overload Resolution & Name Lookup"
topic: unknown
feature: overload-resolution
standard: N/A
status_checked_at: 2026-06-02
---
# Overload Resolution & Name Lookup

## Overload Resolution

When multiple functions share the same name, the compiler selects one through the following steps:

```
1. Name Lookup — find all functions with the same name
2. Candidate Functions — those with matching parameter count
3. Viable Functions — those whose parameter types can be converted
4. Best Match — ranked by conversion rank
```

### Conversion Ranks (Best to Worst)

1. **Exact match** — types are identical
2. **Promotion** — `short` → `int`, `float` → `double`
3. **Standard conversion** — `int` → `double`, `Derived*` → `Base*`
4. **User-defined conversion** — via conversion constructor or conversion operator
5. **Ellipsis** — `...` parameter

```cpp
void f(int);       // #1
void f(double);    // #2
void f(...);       // #3

f(42);     // #1 exact match
f(3.14);   // #2 exact match
f('a');    // #1 via promotion (char → int), better than #2's conversion
```

### Ambiguity

When two overloads are equally good, the compiler emits an error:

```cpp
void f(long);
void f(float);
f(42);  // error! int → long and int → float are equally good
```

## ADL (Argument-Dependent Lookup)

The compiler does not only look for functions in the scope of the call site — it also searches in the namespaces of the argument types:

```cpp
namespace MyLib {
  struct Widget {};
  void swap(Widget& a, Widget& b);  // MyLib::swap
}

MyLib::Widget a, b;
swap(a, b);  // no need to write MyLib::swap!
// the compiler found swap in the MyLib namespace
```

This is why `std::swap` is discovered via ADL — libraries should not override user-type swaps with their own.

**ADL rules**: The compiler considers the "associated namespaces" of each argument type — including the namespace where the type is defined, the namespace of template arguments, etc.

## Two-Phase Lookup

Name lookup in templates is split into two phases:

```
Phase 1 (at template definition): look up non-dependent names (names not dependent on template parameters)
Phase 2 (at template instantiation): look up dependent names (names dependent on template parameters)
```

```cpp
void f(int) { std::cout << "int version\n"; }

template<typename T>
void g(T x) {
  f(x);           // f is a dependent name (because x depends on T)
  // Phase 1: f is not looked up
  // Phase 2: looked up only at instantiation
}

void f(double) { std::cout << "double version\n"; }

g(3.14);  // calls f(double) — Phase 2 lookup sees f(double)
```

MSVC's traditional behavior: only performs Phase 2 (defers all lookups to instantiation). The `/permissive-` flag enables two-phase lookup.

## Dependent Name

Names that depend on template parameters require `typename` for disambiguation:

```cpp
template<typename T>
void foo() {
  T::type* p;         // error! compiler thinks T::type * p is multiplication
  typename T::type* p; // correct! tells the compiler T::type is a type
}
```

## Name Hiding

A name in a derived class hides a same-name entity in the base class (even if the signatures differ):

```cpp
struct Base {
  void f(int);
};

struct Derived : Base {
  void f(double);  // hides Base::f(int)
};

Derived d;
d.f(42);     // calls Derived::f(double)! int is implicitly converted to double
d.Base::f(42);  // calls Base::f(int)
```

Use `using Base::f;` to unhide the base class names.
