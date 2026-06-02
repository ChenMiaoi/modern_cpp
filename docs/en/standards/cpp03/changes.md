---
title: "C++03 Changes and Corrections"
topic: unknown
feature: changes
standard: N/A
status_checked_at: 2026-06-02
---
# C++03 Changes and Corrections

## Value Initialization

C++03 clarified three initialization semantics:

```cpp
int a;          // Default initialization (for built-in types: indeterminate value)
int b = int();  // Value initialization (for built-in types: zero initialization) → b = 0
int c();        // Note: this is a function declaration! (most vexing parse)
```

For class types:
```cpp
struct Widget {
    int x;
    double y;
};

Widget w1;         // Default initialization: calls the default constructor
Widget w2 = Widget();  // Value initialization: zero-initialized first, then calls the default constructor
```

## Two-Phase Name Lookup for Templates

C++03 refined the two-phase name lookup rules in templates:

1. **Phase 1** (at template definition): Lookup of non-dependent names
2. **Phase 2** (at template instantiation): Lookup of dependent names

```cpp
template<typename T>
void foo(T t) {
    bar(t);        // Dependent name — looked up in Phase 2
    baz();         // Non-dependent name — looked up in Phase 1
}
```

This ensures that template code can detect a subset of errors at definition time, rather than deferring all errors to instantiation.

## Other Corrections

- Corrections to `std::allocator` member functions
- Refinement of exception specification behavior
- Fixes for several Defect Reports in library implementations
