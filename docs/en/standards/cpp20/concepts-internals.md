---
title: "Concepts Internals: Atomic Constraints, Normalization, Subsumption, and Diagnostics"
topic: cpp20
feature: concepts-internals
standard: C++20
status_checked_at: 2026-06-02
standard_refs:
  - draft: N4861
    clause: "[temp.constr]"
  - draft: N4861
    clause: "[temp.constr.atomic]"
  - draft: N4861
    clause: "[temp.constr.constr]"
  - draft: N4861
    clause: "[temp.req]"
proposals:
  - P0857R0
  - P1141R2
  - P1452R2
  - P2095R0
exercises: []
solutions: []
---

# Concepts Internals (Atomic Constraints, Normalization, Subsumption, and Diagnostics)

## Overview

Concepts appear syntactically as `template <typename T> concept C = expr;`, but the compiler's processing behind the scenes is far more complex. Constraint matching is not a simple `true`/`false` determination — the compiler must **normalize** constraints into a tree of atomic constraints, and use **subsumption** to determine which constraint is more specialized for ordering in overload resolution. This article delves into these internal mechanisms.

## Atomic Constraints: The Fundamental Unit of Constraints

### Definition

An atomic constraint is the smallest indivisible unit after constraint normalization. **It does not depend on relationships between template parameters** — it contains only an expression and a parameter mapping.

```cpp
// This is an atomic constraint
template <typename T>
concept Integral = std::is_integral_v<T>;
// Encoded as: atomic constraint { expr: std::is_integral_v<T>, params: {T} }

// This is not an atomic constraint — it is a conjunction of two atomic constraints
template <typename T>
concept SignedIntegral = Integral<T> && std::is_signed_v<T>;
// Normalized to: AND( atomic constraint{Integral<T>}, atomic constraint{std::is_signed_v<T>} )
```

### Equivalence of Atomic Constraints

Two atomic constraints are equivalent if and only if all the following conditions are met:
1. **Expression-equivalent**: identical at the lexical level (no semantic equivalence judgment)
2. **Parameter mapping identical**: the mapping from template parameters to the atomic constraint expression is consistent

```cpp
template <typename T>
concept C1 = sizeof(T) > 1;
template <typename T>
concept C2 = sizeof(T) > 1;
// C1 and C2 have equivalent constraint trees (lexically identical expressions), but they are different concept declarations
// The compiler determines equivalence from normalized atomic constraint form, independent of concept names

template <typename T>
concept C3 = requires(T a) { a + 1; };
template <typename T>
concept C4 = requires(T a) { a + 1; };
// C3 and C4 have lexically identical requires expressions → equivalent
```

## Constraint Normalization

Normalization is the process of flattening nested concept references and logical operators into conjunctive normal form (CNF). The compiler recursively applies the following rules to each constraint expression:

### Normalization Rules

```
Given constraint expression E, normalization N(E):

N(P<T>...)    = N(constraint-expression of P)  // concept expansion
                (substituting parameter mapping)

N(E1 && E2)   = N(E1) ∧ N(E2)                 // conjunction

N(E1 || E2)   = N(E1) ∨ N(E2)                 // disjunction

N(!E)         = ¬N(E)                          // negation

N(requires{..}) = requires{..}                 // requires expression → atomic constraint

N(expr)       = expr                           // other expression → atomic constraint
```

### Normalization Example

```cpp
template <typename T>
concept A = sizeof(T) > 1;

template <typename T>
concept B = A<T> && std::is_signed_v<T>;

template <typename T>
concept C = B<T> || std::is_floating_point_v<T>;
```

Normalization process:

```
N(A<T>) = sizeof(T) > 1                              → atomic constraint α1

N(B<T>) = N(A<T> && std::is_signed_v<T>)
        = N(A<T>) ∧ N(std::is_signed_v<T>)
        = α1 ∧ α2                                     → conjunction

N(C<T>) = N(B<T> || std::is_floating_point_v<T>)
        = N(B<T>) ∨ N(std::is_floating_point_v<T>)
        = (α1 ∧ α2) ∨ α3                             → disjunction
```

**Constraint tree structure**:

```
Constraint tree for C<T>:

       ∨ (disjunction)
      / \
     ∧    α3
    / \
  α1   α2

α1: sizeof(T) > 1
α2: std::is_signed_v<T>
α3: std::is_floating_point_v<T>
```

### Template Parameter Mapping

When a concept is referenced, template parameters need to be substituted with actual types. Parameter mapping records this substitution relationship:

```cpp
template <typename T, typename U>
concept SameSize = sizeof(T) == sizeof(U);

template <typename T>
concept HasSameSizeAsInt = SameSize<T, int>;
// During normalization:
//   N(SameSize<T, int>)
//   = N(sizeof(T) == sizeof(U)) with substitution {T→T, U→int}
//   = atomic constraint { expr: sizeof(T) == sizeof(U), map: {T→T, U→int} }
```

## Subsumption

### Definition

Constraint P **subsumes** constraint Q (P subsumes Q), denoted P ⊇ Q, if and only if the normalized constraint P implies the normalized constraint Q.

The compiler judges this through the following recursive rules:

```
Conjunction P = P1 ∧ P2:
  P subsumes Q if and only if P1 subsumes Q or P2 subsumes Q

Disjunction P = P1 ∨ P2:
  P subsumes Q if and only if P1 subsumes Q and P2 subsumes Q

Atomic constraint P:
  P subsumes Q if and only if P and Q are equivalent
```

### Subsumption Example

```cpp
template <typename T>
concept A = sizeof(T) > 1;                     // α1

template <typename T>
concept B = A<T> && std::is_signed_v<T>;       // α1 ∧ α2

// Does B subsume A?
// B = α1 ∧ α2
// Subsumption check: (α1 ∧ α2) ⊇ α1
//   → α1 ⊇ α1 or α2 ⊇ α1
//   → α1 ⊇ α1 ✓ (α1 equivalent to α1)
// Conclusion: B ⊇ A ✓

// Does A subsume B?
// Subsumption check: α1 ⊇ (α1 ∧ α2)
//   Atomic constraints can only subsume equivalent atomic constraints
//   α1 is not equivalent to (α1 ∧ α2)
// Conclusion: A ⊉ B ✗
```

**Practical effect**: In overload resolution, the overload constrained by B is more specialized than the one constrained by A — because B subsumes A.

### Subsumption and Overload Resolution

```cpp
template <typename T>
    requires A<T>      // weaker constraint
void f(T x) { puts("A"); }

template <typename T>
    requires B<T>      // stronger constraint, subsumes A
void f(T x) { puts("B"); }

f(42);        // int satisfies B → outputs "B" (more specialized)
f('c');       // char satisfies both A and B → outputs "B"
f(3.14);      // double satisfies A but not B → outputs "A"
```

### Important Limitation: Semantic Equivalence ≠ Syntactic Equivalence

```cpp
template <typename T>
concept C1 = sizeof(T) == 4;

template <typename T>
concept C2 = sizeof(T) == 2 + 2;
// Semantically C1 and C2 are equivalent, but syntactically different
// The compiler does not perform semantic analysis → C1 does not subsume C2, and vice versa
// If two overloads are constrained by C1 and C2 respectively, calling with a type satisfying both → ambiguity error
```

## Four Kinds of Requirements in requires Expressions

A `requires` expression `{ requirements... }` can contain four kinds of requirements internally:

### 1. Simple Requirements

Only check whether an expression is syntactically valid, not its type or properties:

```cpp
template <typename T>
concept Addable = requires(T a, T b) {
    a + b;           // simple requirement: is T + T valid?
    a.operator+(b);  // check if the member function exists
};

// Note: the expression's result is discarded; only validity is verified
// No noexcept requirement, no specific return type required
```

### 2. Type Requirements

Check that a type name is valid, introduced by the `typename` keyword:

```cpp
template <typename T>
concept HasValueType = requires {
    typename T::value_type;           // nested type must exist
    typename T::iterator;             // iterator type must exist
    typename std::pair<T, T>;         // class template instantiation must be valid
};

// Type requirements are commonly used to check type members of containers
// No need to declare a variable — only verify that the type exists
```

### 3. Compound Requirements

Check the type and properties of an expression, with optional return type constraint and noexcept:

```cpp
template <typename T>
concept Sortable = requires(T& container) {
    // Compound requirement: { expr } -> type-constraint ;
    { container.begin() } -> std::input_or_output_iterator;
    { container.end() }   -> std::input_or_output_iterator;
    { container.size() }  -> std::convertible_to<std::size_t>;
};

template <typename T>
concept NothrowSwappable = requires(T& a, T& b) {
    // noexcept is part of the constraint
    { std::swap(a, b) } noexcept;
};

// A compound requirement is equivalent to:
// Expression e is valid
// AND decltype((e)) satisfies type-constraint
// (if specified) e is noexcept
```

### 4. Nested Requirements

Use the `requires` keyword to introduce nested constraint checks:

```cpp
template <typename T>
concept SemiRegular = requires {
    // Nested requirement: checks additional constraints, not limited to expression validity
    requires std::default_initializable<T>;
    requires std::copy_constructible<T>;
    requires std::destructible<T>;
};

// Nested requirements can check arbitrary constraint expressions, not just expression validity
// Commonly used to reference other concepts within requires expressions
```

### Comprehensive Example

```cpp
template <typename T>
concept Container = requires(T& c, const T& cc) {
    typename T::value_type;                              // type requirement
    typename T::iterator;
    { cc.size() } -> std::convertible_to<std::size_t>;   // compound requirement
    { cc.begin() } -> std::input_or_output_iterator;     // compound requirement
    { cc.end() }   -> std::input_or_output_iterator;
    c.clear();                                           // simple requirement
    requires std::destructible<T>;                       // nested requirement
};
```

## Abbreviated Function Templates

Functions with `auto` parameters are syntactic sugar for function templates. Each `auto` parameter introduces an independent template parameter:

```cpp
// Abbreviated form
void f(std::integral auto a, std::floating_point auto b);

// Equivalent full template
template <std::integral T1, std::floating_point T2>
void f(T1 a, T2 b);

// Note: the two autos are different template parameters
// f(1, 2.0) is valid: T1=int, T2=double
```

### Plain auto vs Constrained auto

```cpp
void g(auto x);                      // unconstrained: any type
void h(std::integral auto x);        // constrained: integral only

// Plain auto is equivalent to template <typename T>
// Constrained auto is equivalent to template <Concept T>

// Multiple independent autos
void multi(auto a, auto b);          // a and b can have different types
// void same(auto a, auto a);       // error: parameter names cannot repeat
```

## Constraint Satisfaction vs Substitution Failure

### Constraint Satisfaction

A constraint is satisfied when all atomic constraints evaluate to `true`. Constraint satisfaction checking occurs after template argument substitution:

```cpp
template <typename T>
    requires std::is_integral_v<T>
T add(T a, T b) { return a + b; }

add(1, 2);  // T=int, after substitution check std::is_integral_v<int> == true → satisfied
add(1.0, 2.0); // T=double, std::is_integral_v<double> == false → not satisfied
// Unsatisfied overloads are excluded from the overload candidate set (not a hard error)
```

### Preservation of SFINAE Semantics

Concept constraints preserve the SFINAE "substitution failure is not an error" semantics. When a constraint is not satisfied, the overload is silently excluded:

```cpp
template <typename T>
    requires requires { typename T::value_type; }
auto get_value_type(const T&) -> typename T::value_type;

struct Has { using value_type = int; };
struct NoHas {};

get_value_type(Has{});    // OK: constraint satisfied
get_value_type(NoHas{});  // not satisfied → excluded from candidate set (not a compilation error)
// If there are no other candidate overloads → then "no matching function" error is reported
```

### Comparison with static_assert

```cpp
template <typename T>
T bad(T a, T b) {
    static_assert(std::is_integral_v<T>, "must be integral");
    return a + b;
}
bad(1.0, 2.0); // hard error! static_assert does not participate in overload resolution

template <typename T>
    requires std::is_integral_v<T>
T good(T a, T b) { return a + b; }
good(1.0, 2.0); // soft failure: overload excluded, no hard error
```

## Constraint Ordering in Overload Resolution

### Ordering Rules

In constrained overload resolution, candidate functions are ordered by the following priority:

1. **More constrained first**: constraints that subsume others are more specialized
2. **When constraints are equivalent, fall back to traditional overload rules**: non-template functions preferred, argument conversion rank, etc.
3. **When constraints are incomparable, an ambiguity error occurs**

```cpp
// Unconstrained → weakest
template <typename T>
void f(T) { puts("unconstrained"); }

// Constrained → stronger
template <typename T>
    requires std::integral<T>
void f(T) { puts("integral"); }

// Strongest constraint
template <typename T>
    requires std::signed_integral<T>
void f(T) { puts("signed_integral"); }

f(42);      // signed_integral (most specialized)
f('c');     // integral (char satisfies integral but not signed_integral)
f(3.14);    // unconstrained (only the unconstrained version matches)
```

### Incomparable Constraints

```cpp
template <typename T>
    requires std::integral<T>
void g(T) { puts("integral"); }

template <typename T>
    requires std::floating_point<T>
void g(T) { puts("floating_point"); }

g(42);    // OK: only integral matches
g(3.14);  // OK: only floating_point matches
// But no type satisfies both, so no ambiguity
```

If there are two incomparable constraints and a type satisfies both:

```cpp
template <typename T>
    requires std::copyable<T> && std::equality_comparable<T>
void h(T) { puts("copyable+eq"); }

template <typename T>
    requires std::movable<T> && std::totally_ordered<T>
void h(T) { puts("movable+ordered"); }

h(42); // ambiguity error: both satisfied, constraints are incomparable
```

## Why std::same_as<T,U> is Defined Symmetrically

```cpp
// Standard library definition
template <typename T, typename U>
concept same_as = std::is_same_v<T, U>;

// But the symmetric definition ensures bidirectional satisfaction
template <typename T, typename U>
concept same_as = std::is_same_v<T, U> && std::is_same_v<U, T>;
```

The key reason for the symmetric definition: **parameter mappings are asymmetric during normalization**.

```cpp
// If defined unidirectionally
template <typename T, typename U>
concept same_as = std::is_same_v<T, U>;  // only checks T == U

template <typename T, typename U>
    requires same_as<T, U>  // checks T == U
void f(T, U);

template <typename T, typename U>
    requires same_as<U, T>  // checks U == T
void g(T, U);

f(1, 2);   // same_as<int, int> ✓
g(1, 2);   // same_as<int, int> ✓
// But f and g have different constraint forms (different parameter mappings)
// Symmetric definition ensures same_as<int, int> has a unique normalized form
```

In practice, the standard library's `std::same_as` definition uses a conjunction of two clauses. The `is_same_v<T, U>` itself is already symmetric. What truly matters is that when two different overloads use `same_as<T,U>` and `same_as<U,T>`, their parameter mappings differ and would not produce "subsumption." The symmetric definition ensures they are equivalent.

**Core reason**: During normalization, parameter mappings `{T→int, U→int}` and `{T→int, U→int}` are the same, making constraints equivalent. But writing `same_as<T, U>` and `same_as<U, T>` produces lexically different mappings. The symmetric definition avoids this ambiguity.

## Diagnostics: Writing Readable Constraint Error Messages

### Default Compiler Diagnostics

When a constraint is not satisfied, major compilers produce:

```cpp
template <typename T>
    requires std::integral<T>
void process(T val) { /* ... */ }

process(3.14);
```

```
# Clang:
error: no matching function for call to 'process'
note: candidate template ignored: constraints not satisfied
note: because 'double' does not satisfy 'integral'

# GCC:
error: no matching function for call to 'process(double&)'
note: candidate: 'template<class T> requires integral<T> void process(T)'
note: template argument deduction/substitution failed
note: constraints not satisfied
```

### Techniques for Improving Diagnostic Quality

**1. Use named concepts instead of anonymous expressions**

```cpp
// Bad: error message shows the raw expression
template <typename T>
    requires std::is_arithmetic_v<T> && (!std::is_same_v<T, bool>)
void f(T);

// Good: error message shows the concept name
template <typename T>
concept Numeric = std::is_arithmetic_v<T> && (!std::is_same_v<T, bool>);

template <Numeric T>
void f(T);
// Error: "because 'std::string' does not satisfy 'Numeric'"
```

**2. Provide context in requires expressions**

```cpp
template <typename T>
concept Printable = requires(std::ostream& os, T val) {
    // Use concrete expressions to make error messages more meaningful
    { os << val } -> std::same_as<std::ostream&>;
};
// Error message will point out that os << val is invalid or return type doesn't match
```

**3. Custom static_assert as a last resort**

```cpp
template <typename T>
    requires std::regular<T>
void store(T val) {
    static_assert(std::regular<T>,
        "T must satisfy std::regular (copyable, movable, "
        "default-constructible, equality-comparable). "
        "Common issues: missing operator== or default constructor.");
    // ...
}
```

## Concepts as API Contracts

Concepts not only constrain syntax but also imply **semantic requirements**. The compiler cannot check semantics, but correctly using concepts is a programmer's contractual obligation:

### Syntactic vs Semantic Requirements

```cpp
// std::equality_comparable syntactic requirement: t == u returns bool
// Semantic requirements:
//   - Reflexivity: a == a
//   - Symmetry: a == b ⟺ b == a
//   - Transitivity: a == b && b == c → a == c
//   - Stability: consecutive == evaluations produce the same result

// The compiler can only verify syntax. The programmer is responsible for semantics.
```

### Documenting Semantics in Concept Definitions

```cpp
// Don't just look at syntax — annotate semantic contracts with comments
template <typename T>
concept Sortable = requires(T& a, T& b) {
    // Syntactic requirements
    { a < b } -> std::convertible_to<bool>;
    { a == b } -> std::convertible_to<bool>;
}
// Semantic requirements (not compiler-checkable):
// - < is a strict total order (irreflexive, asymmetric, transitive)
// - == is an equivalence relation
// - a < b and !(b < a) and !(a < b) && !(b < a) are all equivalent to a == b
;
```

### Graduated Concept Constraints

```cpp
// Weakest constraint: only needs to be destructible
template <typename T>
concept BasicObject = std::destructible<T>;

// Medium constraint: copyable
template <typename T>
concept CopyableObject = BasicObject<T>
    && std::copy_constructible<T>
    && std::move_constructible<T>;

// Strongest constraint: full semantic type
template <typename T>
concept RegularObject = CopyableObject<T>
    && std::default_initializable<T>
    && std::equality_comparable<T>;
// Semantic contract: Regular supports value semantics, copy, compare, default-construct

// Algorithms select constraint level as needed
template <typename T>
    requires BasicObject<T>
void destroy_only(T& obj) { obj.~T(); }

template <RegularObject T>
void store_in_container(T obj) { /* needs default construct, copy, compare */ }
```

## Summary

```
Concepts Internal Processing Flow
──────────────────────────────────────────
Source: concept C = expr; / requires { ... }
    ↓
Normalization: flatten concept references → atomic constraint tree (CNF)
    ↓
Evaluation: substitute template parameters → evaluate bool for each atomic constraint
    ↓
Satisfaction: all atomic constraints true → constraint satisfied
    ↓
Overload: satisfied candidate set → order by subsumption → select most specialized
    ↓
Diagnostics: on failure → reference concept name → clear error
──────────────────────────────────────────
```

Understanding these internal mechanisms yields:
- Avoiding overload ambiguity from "semantically identical but syntactically different" constraints
- Correctly designing concept hierarchies to leverage automatic subsumption ordering
- Writing compiler-friendly constraints for meaningful diagnostics
- Distinguishing syntactic checks (compiler's job) from semantic contracts (programmer's job)
