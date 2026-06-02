---
title: Name Lookup
topic: topics
feature: name-lookup
status_checked_at: 2026-06-02
standard: N/A
---

# Name Lookup

## Overview

C++ name lookup is the process by which the compiler resolves a name to a specific declaration. It is a prerequisite for subsequent steps such as overload resolution and template instantiation. The standard divides lookup into two major categories: **unqualified lookup** and **qualified lookup**, with additional special rules such as ADL and two-phase template lookup. Understanding these rules is essential for reading compiler error messages and avoiding subtle bugs.

```
Name lookup flow:
  1. Is the name qualified (contains ::)? → Qualified lookup (searches only the specified scope)
  2. Unqualified name → Unqualified lookup (searches outward layer by layer)
  3. If the call expression has function arguments → additionally perform ADL
  4. If inside a template definition → Two-phase lookup: non-dependent names are looked up immediately, dependent names are deferred to instantiation
  5. After the candidate set is found → perform overload resolution
```

## Unqualified Lookup

Unqualified lookup starts from the innermost scope at the point of use and searches outward layer by layer until at least one match is found or the global scope is reached.

```cpp
int x = 10;

namespace A {
    int x = 20;
    namespace B {
        int x = 30;
        void f() {
            int x = 40;
            // Lookup order: f local → B → A → global
            // x = 40 is found, lookup stops
            int val = x; // 40
        }
    }
}
```

**Key rule — found, stop**: once a matching name is found at a particular scope level, lookup stops, and outer declarations with the same name are **hidden** — no overload resolution occurs across them:

```cpp
void f(int);        // Global

namespace N {
    void f(double);  // N::f hides ::f
    void g() {
        f(42);       // Finds N::f(double); ::f(int) is not in the candidate set
                     // 42 → double, calls N::f(double)
    }
}
```

**Lookup in class scope**: name lookup within a member function body searches the class scope (including base classes) first, then the enclosing namespaces:

```cpp
namespace N {
    int value = 1;
}

struct Base {
    int value = 2;
};

struct Derived : Base {
    void f() {
        int value = 3;      // Local variable
        int a = value;       // 3 — local variable
        int b = this->value; // 2 — base class member via this
        int c = N::value;    // 1 — qualified lookup
    }
};
```

## Qualified Lookup

Qualified lookup searches only within the specified scope, **does not expand to outer scopes**, and **does not perform ADL**:

```cpp
namespace A {
    void f(int);
    namespace B {
        void f(double);
        void g() {
            A::f(42);   // Searches only in A → A::f(int)
            f(42);       // Unqualified lookup → B::f(double) (hides A::f)
        }
    }
}
```

Key characteristics of qualified lookup:

```cpp
// 1. No ADL
namespace N {
    struct S {};
    void serialize(S);
}

N::S s;
N::serialize(s);    // ✅ Qualified lookup finds N::serialize
// N::serialize does not trigger ADL in qualified lookup, but it happens to be found in N

// 2. Global scope prefix
::printf("hello");  // Searches only the global scope, skipping all namespaces

// 3. When looking up class members, base classes are searched
struct Base { static int x; };
struct Derived : Base {
    void g() {
        Derived::x;   // Qualified lookup follows the inheritance chain to find Base::x
    }
};
```

## ADL (Argument-Dependent Lookup / Koenig Lookup)

ADL is a **supplementary rule for unqualified lookup**: when the arguments of a function call belong to a certain namespace (or class), the compiler also searches for the function name in that namespace (or the namespace associated with the class).

```cpp
namespace StringLib {
    struct String {};
    void print(const String&);  // Declared in this namespace
}

StringLib::String s;
print(s);  // Unqualified lookup doesn't find a global print → ADL finds print in StringLib
```

**Associated namespaces for ADL lookup**:

```
The associated namespaces of argument type T include:
  1. If T is a class → the namespace in which T resides
  2. The namespaces of all base classes of T
  3. If T is a template instantiation → the associated namespaces of the template argument types
  4. If T is an enum → the namespace in which the enum resides
  5. If T is a pointer/array → the associated namespaces of the pointed-to/element type
```

```cpp
namespace A {
    struct Base {};
    void process(Base);
}

namespace B {
    struct Derived : A::Base {};
    void process(Derived);
}

B::Derived d;
process(d);  // ADL searches A and B (A is the associated namespace of Base, B is that of Derived)
             // Finds A::process(Base) and B::process(Derived)
             // Overload resolution selects B::process(Derived) (more exact match)
```

**ADL is not triggered in the following situations**:

```cpp
// 1. Qualified calls
std::swap(a, b);    // Qualified lookup, no ADL (but std::swap is the candidate found)

// 2. Function pointers
auto fp = &process; // No ADL

// 3. Declarations
using std::swap;    // No ADL
```

## ADL and operator<< (The Most Classic ADL Scenario)

Why can `std::cout << obj` find `operator<<(std::ostream&, const MyType&)` even when it is defined in `MyNamespace`? ADL.

```cpp
#include <iostream>

namespace MyLib {
    struct Point { int x, y; };

    // operator<< is defined in MyLib, not in std
    std::ostream& operator<<(std::ostream& os, const Point& p) {
        return os << '(' << p.x << ',' << p.y << ')';
    }
}

int main() {
    MyLib::Point p{1, 2};
    std::cout << p << '\n';
    // operator<<(std::cout, p)
    //   → std::cout has type std::ostream
    //   → p has type MyLib::Point
    //   → ADL searches std and MyLib
    //   → Finds operator<<(ostream&, const Point&) in MyLib
}
```

**This is why `operator<<` must be defined in the namespace where the type resides (or in `std`)** — otherwise ADL cannot find it.

```cpp
// ❌ Wrong: defined in the global scope, not in MyLib
namespace MyLib { struct Foo {}; }
std::ostream& operator<<(std::ostream& os, const MyLib::Foo&) { return os; }

// If Foo has no association with the std namespace, ADL only searches std and MyLib,
// not the global scope → compilation fails

// ✅ Correct: defined in MyLib
namespace MyLib {
    struct Bar {};
    std::ostream& operator<<(std::ostream& os, const Bar&) { return os; }
}
```

## Friend Declarations and Name Injection

A friend declaration can introduce a function into the namespace of the class, but there is a key restriction: **only when the friend declaration is the sole declaration of the function does it actually inject the name**.

```cpp
namespace N {
    struct S {
        friend void hidden_friend(S);  // Declared in N, but only visible via ADL
    };
}

N::S s;
hidden_friend(s);    // ✅ Found via ADL in N
// hidden_friend(s); // If doing unqualified lookup (without an argument of type S), it is not found
```

**Friend defined in the class body** (hidden friend) — this is a common pattern in modern C++:

```cpp
namespace util {
    template <typename T>
    struct Wrapper {
        T value;
        // Friend function defined in the class body, only visible via ADL
        // Does not pollute the enclosing namespace
        friend bool operator==(const Wrapper& a, const Wrapper& b) {
            return a.value == b.value;
        }
    };
}

util::Wrapper<int> a{1}, b{2};
a == b;  // ✅ ADL finds operator== (argument type is util::Wrapper<int>)
// operator==(a, b); // ❌ Unqualified lookup (without ADL) cannot find it
```

**Interaction with existing declarations**:

```cpp
void g(int);          // Global declaration

struct S {
    friend void g(int);  // Not a new declaration, just grants g access to S's private members
};

// If the friend declaration is the sole declaration of a brand-new function:
struct T {
    friend void h(T);    // Injected into the enclosing namespace (but only visible via ADL)
};
```

## Name Hiding in Inheritance

A member in a derived class with the same name (regardless of whether the signatures match) **hides** the base class member with the same name. This is a direct consequence of the "found, stop" rule of unqualified lookup.

```cpp
struct Base {
    void f(int);
    void f(double);
    void g(int);
};

struct Derived : Base {
    void f(double);  // Hides Base::f(int) and Base::f(double)
    // Restore using a using-declaration
    using Base::f;   // Now Base::f(int) and Base::f(double) are both visible
};

void test(Derived& d) {
    d.f(42);         // 42 → double, calls Derived::f(double)
    // Without the using-declaration, Base::f(int) is hidden and does not participate in overload resolution
}
```

**Data members are also hidden**:

```cpp
struct Base { int x = 1; };
struct Derived : Base { int x = 2; };

Derived d;
d.x;            // 2 — Derived::x hides Base::x
d.Base::x;      // 1 — Qualified lookup accesses the base class member
```

**Why C++ was designed this way**: functions with the same name in different scopes should not implicitly participate in overload resolution — the author of the derived class may not be aware of all the overloads in the base class, and implicit overloading would lead to unpredictable function calls.

## using Declarations and using Directives

### using Declaration (using-declaration)

A using declaration introduces a **specific name** into the current scope. It creates an alias, not a copy:

```cpp
namespace A {
    void f(int);
    void f(double);
}

void g() {
    using A::f;     // Introduces A::f into g's scope
    f(42);          // A::f(int)
    f(3.14);        // A::f(double)
}

// Using a using-declaration in a class to restore hidden base class members
struct Derived : Base {
    using Base::f;  // Introduces Base::f into Derived's scope, coexisting with Derived::f for overload resolution
};
```

### using Directive (using-directive)

A using directive promotes **all names** in a namespace into the **common enclosing scope** that contains both the using directive and the namespace declaration:

```cpp
namespace A {
    int x = 1;
    void f(int);
}

namespace B {
    int x = 2;
    void f(double);
}

void g() {
    using namespace A;
    using namespace B;
    // x is promoted to the global scope, both A::x and B::x are candidates → ambiguity
    // int val = x;  // ❌ Error: ambiguous

    // But f can be distinguished by overload resolution
    f(42);     // A::f(int)
    f(3.14);   // B::f(double)
}
```

**The nature of using directives**: names are injected into a "transitive scope" that is the common ancestor of the scope containing the using directive and the scope containing the namespace declaration. This means a using directive does not inject names directly into the current scope, but into a higher level.

```cpp
namespace A { int v = 1; }

void f() {
    {
        using namespace A;  // v is promoted to f's scope (not the inner block)
        v;                  // ✅ Finds A::v
    }
    // v;  // ❌ The effect of the using directive ends when the block ends (may differ before C++17)
}
```

## Namespace Aliasing

Namespace aliases are pure syntactic sugar and do not affect name lookup semantics:

```cpp
namespace VeryLongNamespaceName {
    struct Widget {};
    void process(Widget);
}

namespace VLN = VeryLongNamespaceName;  // Alias

VLN::Widget w;      // Equivalent to VeryLongNamespaceName::Widget w;
VLN::process(w);    // Equivalent to VeryLongNamespaceName::process(w);

// ADL is based on the original namespace, not the alias
// ADL for process(w) searches VeryLongNamespaceName, not VLN
```

Inline namespace aliases are more noteworthy:

```cpp
namespace v2 {
    inline namespace v2_0 {  // Inline namespace
        struct Config {};
    }
}

v2::Config c1;       // OK — v2_0 is inline, v2::Config is v2::v2_0::Config
v2::v2_0::Config c2; // OK — explicitly specified

// ADL: the associated namespaces of v2::Config are v2 and v2::v2_0
```

## Two-Phase Template Name Lookup

The C++ standard requires **two-phase lookup** for templates: non-dependent names are looked up at template definition time, and dependent names at instantiation time.

```cpp
void process(int) { std::cout << "int\n"; }

template <typename T>
void foo(T val) {
    process(42);     // Non-dependent name — looked up immediately at definition time (phase 1)
    process(val);    // Dependent name — looked up at instantiation time (phase 2)
}

void process(double) { std::cout << "double\n"; }

foo(3.14);
// process(42)   → Lookup occurs before foo's definition, only ::process(int) is found → outputs "int"
// process(val)  → Dependent name, looked up at instantiation, ADL search → finds both process overloads, overload resolution selects double
```

**MSVC's historical issue**: MSVC has long defaulted to not implementing two-phase lookup (performs only a single lookup pass); the `/permissive-` flag enables standard-conforming behavior. This is a common pitfall for cross-compiler portability.

## Dependent vs Non-Dependent Names

In templates, whether a name depends on template parameters determines when it is looked up:

```cpp
template <typename T>
struct Traits {
    using type = typename T::value_type;  // Dependent name — not looked up until T is known
};

// Example of a non-dependent name
extern int global_val;

template <typename T>
void f() {
    int x = global_val;  // Non-dependent name — looked up immediately, bound to the current declaration
}
```

**Determination rules**:

```
Dependent names:
  - T::name (where T is a template parameter)
  - expr.name where the type of expr depends on a template parameter
  - expr->name, same as above
  - func(args...) where at least one argument type depends on a template parameter
  - T(args...), T{args...}

Non-dependent names:
  - All other cases
  - Lookup occurs at template definition time (phase 1)
  - Bound to declarations visible at definition time; subsequently added declarations have no effect
```

```cpp
// ⚠️ Classic pitfall
void helper(int) { std::cout << "int\n"; }

template <typename T>
void g(T val) {
    helper(val);     // Dependent name (val's type depends on T)
    helper(1.0);     // Non-dependent name (argument type is double, does not depend on T)
}

void helper(double) { std::cout << "double\n"; }

g(42);
// helper(val)  → Dependent name, at instantiation ADL + ordinary lookup → helper(int) (globally visible)
// helper(1.0)  → Non-dependent name, looked up at definition time → only helper(int) is found
//                 → 1.0 is implicitly converted to int → outputs "int" (not double!)
```

## Necessity of the typename Keyword

In templates, a nested type name that depends on template parameters must be preceded by `typename`; otherwise the compiler defaults to parsing it as a **non-type** (variable, static member, etc.):

```cpp
template <typename T>
void f() {
    // T::type might be a type, or it might be a static member variable
    // The compiler assumes by default that it is not a type
    // T::type* p;          // ❌ Error: T::type is parsed as a value, * p is multiplication

    typename T::type* p;    // ✅ Explicitly tells the compiler that T::type is a type

    // C++20 improvement: typename can be omitted in certain contexts
    // Function return types, parameter types (since C++20)
}
```

**C++20 simplification** (P0634R3): `typename` can be omitted in the following contexts:
- Return types of function declarations
- Function parameter types
- Template arguments (as arguments to type template parameters)
- Type specifiers in data member declarations
- Type specifiers in variable declarations (other than `auto`)

```cpp
template <typename T>
// C++11: typename T::iterator foo();  // typename is required
// C++20:
T::iterator foo();  // ✅ typename omitted

template <typename T>
struct S {
    T::value_type data;  // ✅ C++20 omits typename
};
```

## The template Keyword for Dependent Template Names

When a dependent name is a template, the `template` keyword must be used to inform the compiler that the following `<` is the start of a template argument list, not a comparison operator:

```cpp
template <typename T>
void f() {
    // T::create<int>(42);        // ❌ Compiler parses < as a comparison operator
    T::template create<int>(42);  // ✅ Explicitly tells the compiler that create is a template

    // Same applies to member templates
    T obj;
    obj.template get<int>();       // ✅
    // obj.get<int>();             // ❌ May fail on some compilers
}
```

**Combining `typename` and `template`**:

```cpp
template <typename T>
void g() {
    // Obtaining an instantiation type of a nested template
    typename T::template Container<int>::value_type val{};
    //  ↑ typename: Container<int>::value_type is a type
    //                    ↑ template: Container is a template
}
```

## Dependent Name Lookup Order

For dependent names in templates, the standard defines a special lookup order:

```
Dependent name lookup order:
  1. Ordinary lookup results visible at template definition time (phase 1 results of two-phase lookup)
  2. ADL at template instantiation time (based on associated namespaces of the arguments)

Important: dependent names do NOT perform "ordinary lookup at instantiation time"
```

```cpp
namespace N {
    struct S {};
    void f(S);           // ① Visible at definition time
}

template <typename T>
void g(T val) {
    f(val);              // Dependent name: lookup ① f visible at definition time + ② ADL at instantiation time
}

// Added later
void f(int);            // ③ Not present in the definition-time lookup results

N::S s;
g(s);                   // ① Finds N::f(S) (visible at definition time); ② ADL also finds N::f(S)
g(42);                  // ① Finds N::f(S)? No, S and int are different
                        //   ① No match → ② ADL searches the associated namespaces of int (none) → fails
```

This lookup order explains why the lookup results for dependent names differ from those for non-dependent names: non-dependent names use only ordinary lookup at definition time, while dependent names additionally include ADL at instantiation time.

## CRTP and Name Lookup Pitfalls

In CRTP (Curiously Recurring Template Pattern), when the base class template accesses derived class members, the timing of name lookup is the key issue:

```cpp
template <typename Derived>
struct Base {
    void interface() {
        // this->impl();        // ✅ Dependent name (this's type depends on Derived)
        // impl();              // ❌ Non-dependent name, looked up in phase 1, Derived::impl is not found
        static_cast<Derived*>(this)->impl();  // ✅ But verbose
    }

    // C++ best practice: use this-> to access derived class members
    void better() {
        this->impl();  // ✅ The type of this is Base<Derived>* → dependent name
    }
};

struct MyDerived : Base<MyDerived> {
    void impl() { std::cout << "MyDerived\n"; }
};
```

**`this->` is the standard idiom for making base class member names dependent**. Without `this->`, the name is looked up in phase 1, at which point the derived class has not yet been defined, and lookup fails.

```cpp
// Another CRTP pitfall: static members
template <typename D>
struct Counter {
    static int count;
    void increment() {
        // count++;            // ❌ Non-dependent name, may bind to an outer count
        Counter::count++;      // ✅ Qualified lookup, does not depend on template parameters
        // or
        this->count++;         // ✅ Dependent name
    }
};
```

## P0846: ADL and Function Templates (C++20)

Before C++20, ADL only searched for **visible function declarations** in associated namespaces, not for **function template** specializations — if a function template had not yet been instantiated, its specialization was not visible to ADL.

P0846R0 (C++20) relaxed this restriction: ADL now searches for function templates in associated namespaces even if they have not yet been instantiated.

```cpp
namespace N {
    template <typename T>
    void serialize(T);  // Function template declaration

    struct Widget {};
    // serialize<Widget> is never explicitly instantiated
}

N::Widget w;
serialize(w);  // C++17: ADL finds the N::serialize template, but its specialization is not instantiated → may fail
               // C++20 (P0846): ADL finds the N::serialize template → instantiates serialize<Widget> → OK
```

This change resolved portability issues with the following pattern:

```cpp
namespace ext {
    template <typename T>
    void to_string(T const&);  // Forward declaration

    struct Error {};
    // to_string<Error> is not defined, but the declaration exists
}

ext::Error e;
to_string(e);  // C++17: behavior may vary across compilers
               // C++20: the standard requires ADL to find this template declaration
```

## Common Name Lookup Pitfalls

### 1. ADL in the std Namespace

```cpp
// ❌ Qualifying swap as std::swap prevents ADL
// Correct approach: using std::swap; then call swap(a, b)
template <typename T>
void bad_swap(T& a, T& b) {
    std::swap(a, b);  // Only searches std, does not search T's associated namespaces
}

template <typename T>
void good_swap(T& a, T& b) {
    using std::swap;    // Introduces std::swap into the current scope
    swap(a, b);         // Unqualified lookup + ADL → custom swap takes priority over std::swap
}
```

### 2. Ambiguity Caused by using Directives

```cpp
namespace A { void f(int); }
namespace B { void f(int); }

void g() {
    using namespace A;
    using namespace B;
    // f(42);  // ❌ Ambiguous: A::f(int) and B::f(int)
}
```

### 3. ADL Mismatch with Implicit Conversions

```cpp
namespace N {
    struct X {};
    void process(X);
}

N::X x;
// process(x);  // OK — ADL finds N::process
// But if there is also a global process:
void process(int);
// process(x);  // Still OK — ADL finds N::process, global process(X) doesn't match
// process(42); // OK — unqualified lookup finds the global process(int)
```

### 4. Forgetting typename in Templates

```cpp
template <typename Container>
void print_first(Container& c) {
    // Container::iterator it = c.begin();   // ❌ Compilation error
    typename Container::iterator it = c.begin();  // ✅
}
```

### 5. Forgetting template in Templates

```cpp
template <typename Allocator>
void rebind_example(Allocator& alloc) {
    // Allocator::rebind<int>::other int_alloc(alloc);           // ❌
    typename Allocator::template rebind<int>::other int_alloc(alloc);  // ✅
}
```

### 6. Inheriting a Template Base Class Without this->

```cpp
template <typename T>
struct Base {
    T data;
    void set(T val) { data = val; }
};

template <typename T>
struct Derived : Base<T> {
    void foo(T val) {
        // set(val);              // ❌ Non-dependent name, cannot find Base<T>::set
        // data = val;            // ❌ Same issue
        this->set(val);          // ✅ Dependent name
        this->data = val;        // ✅ Dependent name
        Base<T>::set(val);       // ✅ Qualified lookup (but suppresses virtual function dynamic dispatch)
    }
};
```

### 7. Interaction of using Declarations and Overloading

```cpp
struct Base {
    void f(int);
};

struct Derived : Base {
    using Base::f;    // Introduces Base::f(int)
    void f(double);   // Coexists with Base::f(int)
};

Derived d;
d.f(42);    // int → exact match for Base::f(int)
d.f(3.14);  // double → exact match for Derived::f(double)
```

### 8. ADL Does Not Consider Implicit Conversions

```cpp
namespace N {
    struct S { S(int); };  // Implicit conversion from int
    void foo(S);
}

foo(42);  // ❌ ADL does not consider implicit conversions
          // The associated namespaces of int do not include N
          // Must convert to N::S first to trigger ADL
foo(N::S{42});  // ✅ Explicit construction, ADL finds foo in N
```

## Further Reading

- [Template Instantiation](/topics/template-instantiation) — Explicit/implicit instantiation, specialization, and their interaction with name lookup
- [Overload Resolution](/topics/overload-resolution) — Candidate function selection after name lookup
- [Value Categories Deep Dive](/topics/value-categories-deep-dive) — How lvalues/rvalues affect function parameter binding
- [RAII and Resource Management](/topics/raii) — Practical applications of name lookup in resource management patterns
- C++ Standard [basic.lookup](https://eel.is/c++draft/basic.lookup) — Formal rules for name lookup
- cppreference [Name lookup](https://en.cppreference.com/w/cpp/language/lookup) — Complete reference
