---
title: Overload Resolution
topic: topics
feature: overload-resolution
status_checked_at: 2026-06-02
standard: N/A
---

# Overload Resolution

## Overview

Overload resolution is the process by which the compiler selects the **best match** from a set of candidate functions sharing the same name in a function call expression. This is the core mechanism of C++'s type system and interface design — it determines which `operator<<` `std::cout << 42` calls, whether `std::move(x)` will accidentally hijack an object, and why your perfect-forwarding wrapper blows up at certain call sites.

The standard breaks the entire process into three steps:

1. **Build the candidate function set** (candidate functions)
2. **Filter viable functions** (viable functions) — argument count matches and implicit conversion sequences exist
3. **Select the best viable function** (best viable function) — compare conversion quality parameter by parameter

If step three cannot determine a unique winner, the call is **ill-formed** (ambiguous).

## Constructing the Candidate Function Set

Candidate functions come from the following sources, organized by scope level:

```cpp
namespace N {
    void f(int);           // ① namespace scope
    struct X {
        void f(double);    // ② member function
        friend void f(long); // ③ friend function (injected into enclosing namespace)
    };
}

void test(N::X x) {
    x.f(42);   // candidates: X::f(double) + f functions in namespace N + f found by ADL
    f(42);     // candidates: ordinary lookup N::f(int) + f(long) found by ADL
}
```

Sources of candidate functions:

- **Unqualified lookup**: searches up the scope chain, stops outer lookup upon first match (name hiding)
- **Argument-dependent lookup (ADL)**: searches based on the associated namespaces and classes of the argument types
- **Operators**: for `a @ b`, candidates include member `a.operator@(b)` and non-member `operator@(a, b)`, merged via unqualified lookup and ADL

```cpp
namespace lib {
    struct Widget {};
    void serialize(Widget, int);  // found by ADL
}

void test() {
    lib::Widget w;
    serialize(w, 10); // unqualified lookup fails → ADL finds it in lib
}
```

## Filtering Viable Functions

From the candidate set, retain **viable functions** satisfying:

1. **Argument count matches**: the call has N arguments, the function's parameter count ≤ N, and missing parameters must have defaults
2. **Implicit conversion sequences exist**: for each argument to the corresponding parameter, an implicit conversion sequence can be constructed

```cpp
void f(int, int = 0);     // viable: f(1) and f(1,2)
void f(int, int, int);    // not viable: f(1) has too few arguments
void f(const char*);      // not viable: f(1) has no int → const char* implicit conversion
```

## Implicit Conversion Sequences

Each argument-to-parameter conversion is described by an **implicit conversion sequence (ICS)**. An ICS consists of at most three parts:

```
Standard conversion sequence:   standard conversion → standard conversion → standard conversion
User-defined conversion seq:    standard conversion → user-defined conversion (constructor or conversion operator) → standard conversion
Ellipsis conversion sequence:   (any argument → ...)
```

A **total order** always exists among these three categories: standard conversion sequence > user-defined conversion sequence > ellipsis conversion sequence.

## Standard Conversion Sequences

Standard conversion sequences are the most common case, consisting of a chain of zero to one standard conversions. Standard conversions fall into four categories:

### Lvalue Transformations

Converting lvalues to rvalues, and decaying functions/arrays to pointers:

```cpp
int x = 42;
int& ref = x;
int val = ref;   // lvalue-to-rvalue: reads the value of ref

void g(int*);
int arr[3];
g(arr);          // array-to-pointer: arr decays to int*

void h(int(&)(int));
int foo(int);
h(foo);          // function-to-pointer
```

### Numeric Promotions and Conversions

- **Promotion**: `char`/`short` → `int`, `float` → `double` — value preserved but type is "wider"
- **Conversion**: `int` → `double`, `double` → `int`, pointer conversions, etc. — may lose information

```cpp
void f(int);      // ①
void f(double);   // ②

f('a');            // ① wins: char → int is a promotion, int → double is a conversion
f(3.14);           // ② wins: exact match for double
```

Promotion is **strictly better than** conversion — this is why `f('a')` selects `f(int)` rather than `f(double)`.

### Qualification Conversions

Adding `const`/`volatile` qualifiers on pointers/references:

```cpp
int* p = nullptr;
const int* cp = p;          // int* → const int*: qualification conversion, legal
int* q = cp;                // const int* → int*: removing qualifiers, illegal

void foo(const int&);
foo(/* int& */);            // int& → const int&: qualification conversion, part of a standard conversion sequence
```

Qualification conversions have strict hierarchical rules: qualifiers can only be added, never removed; multi-level pointer qualifiers must be consistent layer by layer (`const int**` cannot implicitly convert to `int**`, but `int**` can convert to `const int* const*`).

### Complete Standard Conversion Sequence Structure

```
[Lvalue transformation] → [Promotion or Conversion] → [Qualification conversion]
```

Each part may be empty. A conversion that does nothing (between the same types) is an **exact match**, ranked highest.

## User-Defined Conversion Sequences

When standard conversions are insufficient, the compiler attempts a single user-defined conversion step via a **conversion operator** or **converting constructor**. The full sequence is:

```
Standard conversion → User-defined conversion → Standard conversion
```

```cpp
struct Meter {
    double value;
    Meter(double v) : value(v) {}  // converting constructor
};

struct Foot {
    double value;
    operator double() const { return value * 0.3048; }  // conversion operator
};

void print(Meter m);

Foot f{10};
print(f);
// ICS: Foot → double (user-defined conversion: Foot::operator double)
//      → Meter (user-defined conversion: Meter::Meter(double))
//
// Two user-defined conversions → not a single implicit conversion sequence → not viable!
// Only one user-defined conversion is allowed
```

Key rule: **within the entire implicit conversion sequence, there can be at most one user-defined conversion**. If two are needed (type A → type B → type C, each step requiring a user-defined conversion), the compiler emits an error.

### Implicit Invocation of Conversion Operators

```cpp
struct StringWrapper {
    std::string s;
    operator const char*() const { return s.c_str(); }
};

void log(const char*);
void log(const std::string&);

StringWrapper w{"hello"};
log(w);  // Ambiguous? operator const char* is a user-defined conversion
         // std::string's converting constructor is also a user-defined conversion
         // Both require one user-defined step → ambiguous
```

C++11's `explicit` conversion operators are only used in explicit contexts, eliminating accidental implicit conversions:

```cpp
struct SafeBool {
    explicit operator bool() const { return true; }
};

SafeBool b;
if (b) { }          // OK: bool context is an explicit conversion context
bool flag = b;       // OK: direct-initialization is also an explicit context
bool flag2 = {b};    // error: copy-initialization is not an explicit context
int x = b;           // error: bool → int also requires a user-defined conversion, cannot chain
```

## Ellipsis Conversion Sequences

Ellipsis conversion (`...`) is the last resort — any argument can match, but with the lowest quality:

```cpp
void f(int);     // standard conversion sequence
void f(...);     // ellipsis conversion sequence

f(42);           // selects f(int): standard conversion strictly better than ellipsis
```

Ellipsis conversion does not check type compatibility and may cause undefined behavior at runtime — this is the root cause of the fragility of the `printf` family of functions.

## Conversion Sequence Ordering

Selecting the best viable function is a **per-parameter comparison**: for each argument, compare the quality of the corresponding ICS.

### Ordering Rules

1. **Different categories**: standard conversion sequence > user-defined conversion sequence > ellipsis conversion sequence

2. **Both standard conversion sequences**:
   - Exact match (no conversion needed) is best
   - Promotion is better than ordinary conversion
   - Remaining cases are compared by the standard conversion type hierarchy

3. **Both user-defined conversion sequences**: compare the standard conversion parts before and after the user-defined conversion — but generally two different user-defined conversions cannot be distinguished (only the surrounding standard conversions can be compared)

4. **Subtle distinctions among exact matches**:
   - Unqualified conversion is better than qualified conversion (`int` → `int` is better than `int` → `const int`)
   - In `bool` conversions, pointer/integer-to-`bool` conversion is worse than other exact matches

```cpp
void g(int*);            // ①
void g(const int*);      // ②

int x = 0;
g(&x);  // ① wins: ① is an exact match (int* → int*), ② requires a qualification conversion (int* → const int*)
```

### When F1 Is Better Than F2 on All Parameters

```cpp
void f(int, double);     // ①
void f(double, int);     // ②

f(1, 1);  // Ambiguous!
          // Parameter 1: ① exact match (int→int) better than ② conversion (int→double) → ① wins
          // Parameter 2: ② exact match (int→int) better than ① conversion (int→double) → ② wins
          // No single function wins on all parameters → ambiguous
```

### Additional Preferences for Exact Matches

For exact matches (no numeric conversions involved), the compiler also prefers:

- When binding to a reference, direct binding is preferred over binding through a temporary
- For template specializations vs. non-template functions, the non-template function is generally preferred (see below)

## Partial Ordering of Function Templates

When two function templates can both match, the compiler uses **partial ordering** to determine which is more specialized. This is a process of using each template's parameters as arguments for deduction against the other:

```cpp
template <typename T>
void f(T);              // ① more general

template <typename T>
void f(T*);             // ② more specialized

int* p;
f(p);  // ② wins: T* is more specialized than T
```

Partial ordering rules:
1. Generate a unique synthesized type from `f1`'s parameters, substitute into `f2` for deduction
2. Generate a unique synthesized type from `f2`'s parameters, substitute into `f1` for deduction
3. If only one direction succeeds in deduction, the successful one is more specialized
4. If both succeed or both fail, neither is more specialized

```cpp
template <typename T>
void h(T, T);           // ①

template <typename T, typename U>
void h(T, U);           // ②

h(1, 2);    // ① wins: ① is more specialized than ② (both parameter types must be the same)
h(1, 2.0);  // ② wins: ① deduces T=int and T=double, which contradicts → not viable
```

Partial ordering sorts along two dimensions: the **number** of template parameters (fewer parameters = more specialized) and the **form** (more specific pattern = more specialized).

## Constraints and Concepts (C++20)

After C++20 introduced concepts, constraints became **preconditions** for overload resolution — before comparing ICS, the compiler first checks whether constraints are satisfied. Constraint checking occurs after candidate filtering and before ICS comparison:

```
Candidate set → Viable function filtering → Constraint checking → ICS comparison → Best viable
```

```cpp
#include <concepts>

template <typename T>
void process(T x) requires std::integral<T> {
    // handle integers
}

template <typename T>
void process(T x) requires std::floating_point<T> {
    // handle floating-point
}

process(42);       // only the first constraint is satisfied → selects the first
process(3.14);     // only the second constraint is satisfied → selects the second
```

### Constraint Partial Ordering

When multiple constrained functions have the same signature, the compiler compares constraint strength:

```cpp
template <typename T> requires std::integral<T>
void serialize(T);                          // constraint A

template <typename T> requires (std::integral<T> && std::signed_integral<T>)
void serialize(T);                          // constraint B: A ∧ additional condition

serialize(42);       // selects the second: B subsumes A (more specialized), but A does not subsume B
serialize(42u);      // selects the first: unsigned does not satisfy signed_integral, B is not satisfied
```

## Subsumption Rules

Constraint comparison is based on the **subsumption** relationship over conjunctive normal form:

- Constraint P **subsumes** constraint Q if and only if P's conjunctive clauses are a superset of Q's conjunctive clauses
- Subsumption is a partial order: P subsuming Q means P is more specialized than Q

```cpp
// Constraint atoms
template <typename T> concept C1 = std::integral<T>;
template <typename T> concept C2 = std::signed_integral<T>;
template <typename T> concept C3 = C1<T> && C2<T>;  // C3 = {integral ∧ signed_integral}

// C3 subsumes C1 (because C1 ⊂ C3's conjunctive clause set)
// C3 subsumes C2
// C1 does not subsume C3
// C2 does not subsume C1 (signed_integral does not imply the full set of integral — actually integral includes both signed and unsigned)
// Note: subsumption only examines syntactic structure, not semantic implication
```

Key limitation: subsumption only performs structural comparison on atomic constraints and conjunctions (`&&`). **Disjunctions** (`||`) do not participate in subsumption:

```cpp
template <typename T> concept A = std::integral<T>;
template <typename T> concept B = std::floating_point<T>;
template <typename T> concept C = A<T> || B<T>;  // disjunction

// C does not subsume A (disjunctions do not appear in subsumption analysis)
// A does not subsume C either
// Neither can distinguish the other → ambiguous if signatures are the same
```

### Practical Effects of Constraint Subsumption

```cpp
template <typename T> requires std::regular<T>
void sort(T* arr, std::size_t n);                     // ①

template <typename T> requires std::totally_ordered<T>
void sort(T* arr, std::size_t n);                     // ②

// totally_ordered subsumes regular only if regular's definition includes totally_ordered
// If regular = default_initializable + movable + copyable + equality_comparable + totally_ordered
// then regular's constraint set contains totally_ordered → regular subsumes totally_ordered
// Therefore ① is more specialized (regular is a conjunction of more constraints)
```

## SFINAE and Overload Resolution

SFINAE (Substitution Failure Is Not An Error) is a core mechanism present since C++98: when template parameter substitution fails, the overload is silently removed from the candidate set rather than producing an error.

### Where SFINAE Applies

SFINAE occurs during the **candidate function construction** phase — when substituting template parameters in the function signature, if an invalid type or expression results, that candidate is discarded:

```cpp
template <typename T>
auto size(const T& c) -> decltype(c.size()) {
    return c.size();           // participates in overload only when size() member exists
}

template <typename T>
std::size_t size(const T* arr, std::size_t n) {
    return n;                  // pointer path
}

int a[5];
size(a, 5);                    // array decays to pointer, second candidate matches
std::vector<int> v;
size(v);                       // first candidate matches
```

### The enable_if Pattern

```cpp
// C++11/14 style
template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
std::string to_string(T val) {
    return std::to_string(val);  // integers only
}

// C++17 simplification
template <typename T>
std::string to_string(T val) requires std::is_integral_v<T> {
    return std::to_string(val);  // C++20: equivalent effect, cleaner
}
```

### Scope of SFINAE

SFINAE only applies to substitution failures directly involving the function signature. Errors inside the **function body** are not SFINAE — they are hard compilation errors:

```cpp
template <typename T>
auto f(T x) -> decltype(x + x) {
    T::nonexistent_member();   // body error, not SFINAE → hard error
}
```

## Default Arguments and Overload Resolution

The interaction with default arguments is very subtle — they affect **viability** filtering but do not affect conversion sequence **quality** comparison.

```cpp
void f(int);              // ①
void f(int, int = 0);     // ②

f(42);  // Ambiguous! Both ① and ② are viable, ① does an exact match on the sole argument
        // ② also does an exact match on the first argument
        // Both have identical ICS quality → indistinguishable → ambiguous
```

This example shows that having a default parameter does not mean a "worse" match. Default parameters only help a candidate become viable; they do not participate in quality ranking.

```cpp
void g(long);             // ①
void g(int, int = 0);     // ②

g(42);  // ② wins: ② is an exact match (int→int), ① requires a conversion (int→long)
```

## Deleted Functions and Overload Resolution

Deleted functions (`= delete`) participate in the entire overload resolution process — they are in the candidate set, can be selected as the best match, but produce an error upon use. This is fundamentally different from "the function does not exist":

```cpp
void process(int);                        // ①
void process(double) = delete;            // ② deleted but present in the candidate set

process(42);     // OK: selects ① (exact match for int)
process(3.14);   // error: selects ② (exact match for double) → use of deleted function
process('c');    // OK: char → int is a promotion, char → double is a conversion, selects ①
```

If ② did not exist at all, `process(3.14)` would implicitly convert to `int` and call ①. The deleted function precisely blocks this accidental conversion — a very powerful tool in API design.

```cpp
// Classic usage: prevent accidental narrowing conversions
class SafeInt {
    int val;
public:
    SafeInt(int v) : val(v) {}
    SafeInt(long long) = delete;  // prevent implicit narrowing from long long
};
```

## Operator Overload Resolution

Candidate functions for the operator expression `a @ b` come from:

1. **Member candidates**: `a.operator@(b)` (if `a` is of class type)
2. **Non-member candidates**: `operator@(a, b)` found via unqualified lookup + ADL
3. **Built-in candidates**: the compiler's built-in operators for built-in types

```cpp
struct Vec {
    double x, y;
    Vec operator+(const Vec& o) const { return {x + o.x, y + o.y}; }
};

Vec a{1,2}, b{3,4};
a + b;  // candidates: Vec::operator+(const Vec&) — member version
        //           operator+(Vec, Vec) — no non-member version found
        //           built-in + — Vec is not an arithmetic type, not viable
        // → selects the member version
```

### Implicit Conversions and Operators

```cpp
struct Meter {
    double v;
    Meter(double d) : v(d) {}       // converting constructor
    Meter operator+(Meter o) const { return {v + o.v}; }
};

Meter m{1.0};
m + 2.0;  // 2.0 implicitly converts to Meter → calls Meter::operator+(Meter)
2.0 + m;  // Error! double is not a class type, no member operator+
           // non-member operator+(double, Meter) also does not exist
```

Fix: define the operator as a friend non-member function:

```cpp
struct Meter {
    double v;
    Meter(double d) : v(d) {}
    friend Meter operator+(Meter a, Meter b) { return {a.v + b.v}; }
};

Meter m{1.0};
m + 2.0;   // OK: 2.0 → Meter, calls operator+(Meter, Meter)
2.0 + m;   // OK: same as above
```

### Ordering of Operator Candidates

When both member and non-member operators exist, standard conversion sequence quality determines the winner. But **extra implicit conversions** are costly — if the member version requires an implicit conversion on the left operand while the non-member version does not, the non-member version wins (and vice versa).

```cpp
struct A {
    operator int() const { return 0; }
    bool operator==(int) const { return true; }      // member
};
bool operator==(int, const A&) { return true; }      // non-member

A a;
a == 0;   // Candidate 1: member operator==(int) — a is an exact match for this, 0 is an exact match for int
          // Candidate 2: non-member operator==(int, const A&) — int(0) exact match for int, a implicitly converts to const A&
          // Candidate 1 requires zero conversion on left operand, Candidate 2 requires zero conversion on right operand
          // Candidate 1 is better: a binding to the this reference is an exact match, better than a user-defined conversion to int
```

## Constructor Overload Resolution

Constructor selection follows standard overload resolution rules, with some special considerations:

```cpp
struct Config {
    Config();                               // ① default constructor
    Config(int port);                       // ② int constructor
    Config(std::string host, int port);     // ③ two-parameter constructor
    explicit Config(const char* host);      // ④ explicit, prevents implicit conversion
};

Config a;                    // ① default construction
Config b(8080);              // ② direct initialization
Config c("localhost", 8080); // ③
Config d = "localhost";      // error: ④ is explicit, cannot be used in copy-initialization
Config e{"localhost"};       // OK: direct list-initialization, explicit is allowed
```

### Impact of explicit Constructors

`explicit` constructors are unavailable in **copy-initialization** contexts (because copy-initialization requires an implicit conversion), but are available in **direct initialization** and **list initialization**:

```cpp
struct Foo {
    explicit Foo(int) {}
};

Foo a = 42;    // error: copy-initialization, explicit constructor not available
Foo b(42);     // OK: direct initialization
Foo c{42};     // OK: direct list-initialization

void bar(Foo);
bar(42);       // error: requires implicit conversion, explicit prevents it
bar(Foo{42});  // OK: explicit construction
```

## Copy vs. Move Constructor Selection

When both copy and move constructors are viable, **rvalues preferentially bind to rvalue references**:

```cpp
struct Buffer {
    int* data;
    std::size_t size;

    Buffer(std::size_t n) : data(new int[n]), size(n) {}
    ~Buffer() { delete[] data; }

    // copy constructor
    Buffer(const Buffer& o) : data(new int[o.size]), size(o.size) {
        std::copy(o.data, o.data + o.size, data);
    }

    // move constructor
    Buffer(Buffer&& o) noexcept : data(o.data), size(o.size) {
        o.data = nullptr;
        o.size = 0;
    }
};

Buffer a(100);
Buffer b(a);              // copy: a is an lvalue → const Buffer& matches
Buffer c(std::move(a));   // move: std::move(a) is an rvalue → Buffer&& preferred

// Function return value: NRVO is preferred over move, move is preferred over copy
Buffer make() {
    Buffer tmp(50);
    return tmp;  // If NRVO applies, constructs directly in the target location
                 // Otherwise: tmp as return value is an rvalue → move construction
}
```

### Interaction with Reference Qualifiers

C++11 allows adding reference qualifiers to member functions, affecting overload resolution:

```cpp
struct Data {
    std::string value;

    std::string& get() & { return value; }             // lvalue object
    std::string&& get() && { return std::move(value); } // rvalue object
};

Data d;
auto& s = d.get();          // lvalue version: returns lvalue reference
auto s2 = Data{"x"}.get();  // rvalue version: returns rvalue reference, can be move-constructed
```

## Perfect Forwarding and Overload Sets

Perfect forwarding (`std::forward`) preserves the value category of arguments, but it has a profound impact on overload resolution — especially between constructors and forwarding constructors:

```cpp
template <typename T, typename... Args>
T create(Args&&... args) {
    return T(std::forward<Args>(args)...);
}

struct Widget {
    Widget(int) {}            // ①
    Widget(const Widget&) {}  // ② copy constructor
};

Widget w1(42);
Widget w2 = create<Widget>(w1);  // Args = Widget& → forward returns Widget&
                                   // matches ② copy constructor, correct
Widget w3 = create<Widget>(std::move(w1));  // Args = Widget → forward returns Widget&&
                                              // matches move constructor
```

### Forwarding Constructors (C++11 Idiom)

```cpp
struct Wrapper {
    std::string name;
    int value;

    // Forwarding constructor: accepts arbitrary arguments, perfectly forwards to members
    template <typename Name, typename... Rest,
              typename = std::enable_if_t<
                  !std::is_same_v<std::decay_t<Name>, Wrapper> &&
                  std::is_constructible_v<std::string, Name>>>
    explicit Wrapper(Name&& name, Rest&&... rest)
        : name(std::forward<Name>(name)), value(std::forward<Rest>(rest)...) {}

    // Must explicitly define copy/move constructors, otherwise the template "intercepts" them
    Wrapper(const Wrapper&) = default;
    Wrapper(Wrapper&&) = default;
};
```

Without the `enable_if` guard, `Wrapper`'s copy/move operations might be "stolen" by the forwarding constructor template — the intuition that templates are generally preferred over non-templates is wrong: in fact **non-template functions are preferred over templates**, but when a template provides a more precise match (e.g., exact match for an lvalue reference), the template wins.

## Common Overload Resolution Pitfalls

### Pitfall 1: Implicit Conversions Cause Unexpected Selections

```cpp
void process(std::string_view sv);  // ①
void process(const char* s);         // ②

process("hello");  // selects ②: const char* is an exact match, std::string_view requires a user-defined conversion
                   // If you expected the string_view path, this can be confusing
```

### Pitfall 2: By-Value Capture vs. By-Reference Capture

```cpp
void f(int);        // ①
void f(int&);       // ②

int x = 42;
f(x);   // Ambiguous? Actually ② is more precise: int& directly binds to x
        // ① requires an lvalue-to-rvalue conversion
        // → ② wins

f(42);  // ① wins: 42 is an rvalue, cannot bind to int&
```

### Pitfall 3: Priority of initializer_list

```cpp
void f(std::initializer_list<int>);  // ①
void f(int);                          // ②

f({1});    // selects ①: braced list preferentially matches initializer_list
f(1);      // selects ②: no braces, initializer_list is not considered
```

List-initialization has a **special preference**: when a braced list can directly match an `initializer_list` parameter, it is preferred over other overloads accepting the same element type. This leads to many confusing behaviors:

```cpp
std::vector<int> v1(10, 1);   // 10 elements, each with value 1
std::vector<int> v2{10, 1};   // 2 elements: 10 and 1
// Reason: {10, 1} matches the initializer_list<int> constructor, not the (size, value) constructor
```

### Pitfall 4: Template vs. Non-Template Interaction

```cpp
void f(int) {}                  // non-template

template <typename T>
void f(T) {}                    // template

f(42);   // selects non-template: for exact matches, non-template functions are preferred over template instantiations
f<int>(42);  // explicitly specified template parameter → forces selection of the template version
```

### Pitfall 5: Base Class Member Hiding

```cpp
struct Base {
    void f(int);
};

struct Derived : Base {
    void f(double);  // hides Base::f(int)
};

Derived d;
d.f(42);     // calls Derived::f(double), not Base::f(int)
             // name lookup finds f in Derived → f in Base is no longer a candidate
             // even though Base::f(int) is a more precise match

using Base::f;  // Solution: using declaration brings Base::f into scope
d.f(42);        // now both candidates are present → Base::f(int) wins
```

### Pitfall 6: Rvalue Reference Binding Accidents

```cpp
void process(std::string&&);   // accepts only rvalues

std::string s = "hello";
process(s);                    // error: s is an lvalue, cannot bind to string&&
process(std::move(s));         // OK: explicitly converted to rvalue
// Warning: at this point s has been moved and is in a valid but unspecified state
```

### Pitfall 7: Unexpected Interaction Between auto and Overloads

```cpp
void log(const std::string&);  // ①
void log(int);                  // ②

auto x = 42;
log(x);        // selects ②: x deduced as int

auto y = "hello";
log(y);        // selects ①: y deduced as const char[6], can convert to string_view but cannot implicitly convert to string
               // → actually const char* → std::string via converting constructor → user-defined conversion
               // → compared with ② (const char* → int standard conversion)?
               // Error: const char* cannot be standard-converted to int
               // → only ① is viable
```

Understanding overload resolution comes down to always asking three questions: what are the candidates? Which are viable? Among the viable set, which is better or at least no worse on **every parameter**? When no function satisfies the "at least no worse" condition, the compiler rejects the call — it would rather emit an error than guess.
