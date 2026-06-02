---
title: "RTTI and Virtual Function Table Implementation"
topic: topics
feature: rtti-vtable
status_checked_at: 2026-06-02
standard: N/A
---

# RTTI and Virtual Function Table Implementation

> C++ polymorphism is not magic — it is a precise dispatch mechanism composed of compiler-generated function pointer tables (vtables) and type metadata (type_info). Understanding this mechanism is essential for writing good C++.

---

## 1. Virtual Function Mechanism

### 1.1 From Source Code to Machine Code

```cpp
class Base {
public:
    virtual void speak() { std::cout << "Base\n"; }
    virtual void walk()  { std::cout << "Base walk\n"; }
    virtual ~Base() = default;
};

class Derived : public Base {
public:
    void speak() override { std::cout << "Derived\n"; }
    // walk() inherited from Base
    ~Derived() override = default;
};

void polymorphic_call(Base* p) {
    p->speak();  // virtual call: target determined at runtime
}
```

The actual execution process of a virtual call:

```
Pseudocode for compiled polymorphic_call(Base* p):

    // 1. Load vptr (pointer to vtable) from the start of the object
    vptr = *(void***)p;

    // 2. Index into the vtable with vptr to get entry 0 (speak's function pointer)
    fn_ptr = vptr[0];   // speak()'s offset in the vtable

    // 3. Indirect call through the function pointer, passing p as implicit this
    fn_ptr(p);
```

### 1.2 vptr Location

The position of vptr in the object's memory layout (Itanium ABI):

```
┌────────────────────────────────────┐
│ vptr                               │ ← object start address (offset 0)
├────────────────────────────────────┤
│ Base::member1                      │
├────────────────────────────────────┤
│ Base::member2                      │
├────────────────────────────────────┤
│ Derived::member3                   │
└────────────────────────────────────┘

The Itanium ABI places vptr at the object's start address (offset 0).
MSVC uses the same strategy.
```

vptr initialization happens in constructors — the compiler inserts vptr assignment code before each constructor body:

```
Pseudocode for compiled Base::Base():

    p->vptr = &vtable_for_Base;   // compiler-inserted
    // user constructor body
    ...

Pseudocode for compiled Derived::Derived():

    Base::Base(p);                // call base constructor first (sets vptr = &vtable_for_Base)
    p->vptr = &vtable_for_Derived; // compiler-inserted: overwrite with derived vtable
    // user constructor body
    ...
```

---

## 2. vtable Layout (Itanium C++ ABI)

### 2.1 vtable Layout Under Single Inheritance

The Itanium C++ ABI is the de facto standard for GCC/Clang on Unix-like platforms.

```cpp
class A {
public:
    virtual void f1();
    virtual void f2();
    virtual void f3();
    ~A();  // virtual (default destructor)
};

class B : public A {
public:
    void f1() override;    // overrides
    virtual void f4();     // new
    ~B() override;
};
```

```
vtable for class A:

┌──────────────────────────────┬──────────────────────┐
│ Index │ Content              │ Description           │
├───────┼──────────────────────┼──────────────────────┤
│  -3   │ offset_to_top        │ 0 (offset to primary │
│       │                      │  base class)          │
│  -2   │ RTTI pointer         │ typeinfo_for_A        │
│  -1   │ ...                  │ (ABI reserved slot)   │
│   0   │ A::f1()              │ First virtual function│
│   1   │ A::f2()              │                       │
│   2   │ A::f3()              │                       │
│   3   │ A::~A() (deleting)   │ Virtual destructor    │
│       │                      │  (deleting variant)   │
│   4   │ A::~A() (complete)   │ Virtual destructor    │
│       │                      │  (complete variant)   │
└───────┴──────────────────────┴──────────────────────┘

vtable for class B:

┌──────┬───────────────────────┬──────────────────────┐
│ Index │ Content              │ Description           │
├───────┼──────────────────────┼──────────────────────┤
│  -3   │ offset_to_top        │ 0 (single inheritance,│
│       │                      │  no offset)           │
│  -2   │ RTTI pointer         │ typeinfo_for_B        │
│  -1   │ ...                  │                       │
│   0   │ B::f1()              │ Overrides A::f1       │
│   1   │ A::f2()              │ Inherited             │
│   2   │ A::f3()              │ Inherited             │
│   3   │ B::~B() (deleting)   │ Overridden destructor │
│   4   │ B::~B() (complete)   │                       │
│   5   │ B::f4()              │ Appended at end       │
└───────┴──────────────────────┴──────────────────────┘

Key rules:
  · Virtual functions are ordered by declaration order
  · Overrides replace the corresponding slot; new virtual functions are appended at the end
  · Starting from offset 0 are virtual functions; negative offsets contain RTTI and auxiliary info
```

### 2.2 Two Slots for Virtual Destructors

The Itanium ABI reserves two slots for virtual destructors:

```
Deleting destructor:
    Calls the complete destructor, then frees object memory (operator delete)
    Used in the delete p; scenario

Complete destructor:
    Calls the destructor body, but does not free memory
    Used for stack object destruction, placement delete, etc.

Base object destructor:
    Only destroys base class and member subobjects, not the derived part
    Called by the derived destructor; does not appear in the vtable
```

---

## 3. vtable Layout Under Multiple Inheritance

Multiple inheritance introduces two key issues: an object needs multiple vptrs (one per base class), and `this` pointer adjustment.

```cpp
class A {
public:
    virtual void f();
    int a;
};

class B {
public:
    virtual void g();
    int b;
};

class C : public A, public B {
public:
    void f() override;
    void g() override;
    int c;
};
```

```
Memory layout of object C:

┌──────────────────────────────────┐  ← this points here (C*)
│ vptr_for_A (primary base vptr)   │
├──────────────────────────────────┤
│ A::a                             │
├──────────────────────────────────┤  ← (char*)this + offset_to_B
│ vptr_for_B (secondary base vptr) │
├──────────────────────────────────┤
│ B::b                             │
├──────────────────────────────────┤
│ C::c                             │
└──────────────────────────────────┘

C's primary vtable (vptr_for_A points here):

┌──────┬───────────────────────────┐
│  -3  │ offset_to_top = 0         │  ← primary base, offset is 0
│  -2  │ RTTI pointer for C        │
│   0  │ C::f()                    │  Overrides A::f
│   1  │ C::g()                    │  Overrides B::g (thunk adjusts this)
└──────┴───────────────────────────┘

C's secondary vtable (vptr_for_B points here):

┌──────┬───────────────────────────┐
│  -3  │ offset_to_top = sizeof(A) │  ← points back to start of C
│  -2  │ RTTI pointer for C        │
│   0  │ C::g()                    │  Overrides B::g (thunk adjusts this)
└──────┴───────────────────────────┘
```

### 3.1 Thunk: this Pointer Adjustment

When calling `C::g()` through a `B*` pointer, `this` points to the B subobject (not the start of C). But `C::g()` needs C's `this`, so the compiler generates a thunk:

```cpp
// Pseudocode: thunk for C::g() via B
void C::g() [thunk for B] {
    this = this - sizeof(A);  // adjust this from B* back to C*
    return C::g();            // jump to the real implementation
}
```

A thunk does only two things: adjust `this`, then jump to the actual function. The compiler places the thunk's address in the secondary vtable; the caller does not need to know.

---

## 4. Virtual Inheritance and VTT

### 4.1 The Diamond Problem with Virtual Inheritance

```cpp
class A { public: virtual void f(); int a; };

class B : public virtual A {
public:
    virtual void g();
    void f() override;
    int b;
};

class C : public virtual A {
public:
    virtual void h();
    void f() override;
    int c;
};

class D : public B, public C {
public:
    void f() override;
    int d;
};
```

```
Object layout with diamond inheritance (instance of D):

┌──────────────────────────────────┐  ← D* / B* points here
│ vptr_for_B                       │
├──────────────────────────────────┤
│ B::b                             │
├──────────────────────────────────┤  ← C* points here
│ vptr_for_C                       │
├──────────────────────────────────┤
│ C::c                             │
├──────────────────────────────────┤
│ D::d                             │
├──────────────────────────────────┤  ← A* points here (shared copy)
│ vptr_for_A                       │
├──────────────────────────────────┤
│ A::a                             │
└──────────────────────────────────┘

The A subobject exists only once — this is the purpose of virtual inheritance.
Each base subobject locates the shared A through the offset field in its vtable.
```

### 4.2 VTT (Virtual Table Table)

The vtables for virtual inheritance cannot be statically determined — derived class constructors must dynamically adjust offset values in the base class vtables. For this reason, the ABI introduces VTT (Virtual Table Table), an array of pointers to all relevant vtables.

```
VTT structure (using D as example):

┌────────────────────────────────────────────────────────────┐
│ VTT for D                                                  │
├────────────────────────────────────────────────────────────┤
│ [0] → D's primary vtable (complete, all offsets resolved)  │
│ [1] → B subobject's vtable portion within D's primary vtbl │
│ [2] → C subobject's vtable portion within D's primary vtbl │
│ [3] → B's construction vtable (used when constructing B    │
│       subobject)                                           │
│ [4] → A's portion within B's construction vtable           │
│ [5] → C's construction vtable (used when constructing C    │
│       subobject)                                           │
│ [6] → A's portion within C's construction vtable           │
└────────────────────────────────────────────────────────────┘

Compiler-generated pseudocode when constructing a D object:

D::D(D* this) {
    // Construct B subobject: use construction vtable first (A's offset not yet resolved)
    this->B_vptr = VTT[3];  // point to B's construction vtable
    B::B();
    // Construct C subobject
    this->C_vptr = VTT[5];  // point to C's construction vtable
    C::C();
    // Shared A subobject: use full A vtable
    this->A_vptr = &vtable_for_A;
    A::A();
    // Finally: switch to D's complete vtable (all offsets now resolved)
    this->B_vptr = VTT[1];  // B's portion in D's primary vtable
    this->C_vptr = VTT[2];  // C's portion in D's primary vtable
    // D's own constructor body
}
```

VTT exists in every class that participates in a virtual inheritance hierarchy, used to correctly set vptrs during construction/destruction. After the object is fully constructed, vptrs point to the stable "complete" vtable.

---

## 5. Pure Virtual Functions and `__cxa_pure_virtual`

### 5.1 Representation of Pure Virtual Functions in the vtable

```cpp
class Abstract {
public:
    virtual void concrete() { /* ... */ }
    virtual void must_implement() = 0;  // pure virtual function
};
```

```
vtable for Abstract:

┌──────┬──────────────────────────────┐
│  -3  │ offset_to_top = 0            │
│  -2  │ RTTI pointer                 │
│   0  │ Abstract::concrete()         │
│   1  │ __cxa_pure_virtual           │  ← points to a runtime termination function
└──────┴──────────────────────────────┘

If a pure virtual function is erroneously called at runtime:

__cxa_pure_virtual() implementation (libcxxabi pseudocode):

    extern "C" void __cxa_pure_virtual() {
        // Print error message to stderr, then terminate the program
        abort_message("Pure virtual function called!");
        // Typically outputs something like:
        // "pure virtual method called\n"
        // terminate() — ultimately calls std::abort()
    }

Triggering scenarios:
  · Calling a pure virtual function from a base class constructor
    (vptr still points to the base class vtable at that point)
  · Calling a pure virtual function from a base class destructor
    (vptr has already reverted to the base class)
  · Accidental call through undefined behavior (dangling pointer)
```

---

## 6. RTTI Implementation: The type_info Object

### 6.1 Structure of type_info

```cpp
// Internal structure of type_info in Itanium ABI (simplified)
class type_info {
    // ABI-required fields
    void* __typeinfo_name;  // pointer to mangled name (lazily computed)

    // Compiler-extended fields (vary by implementation)
    const char* __name;

    // Virtual functions (implement hierarchy comparison)
    virtual ~type_info();
    virtual bool __is_pointer_p() const;
    virtual bool __is_function_p() const;
    virtual bool __do_catch(const type_info* thrown_type,
                            void** thrown_object,
                            unsigned outer) const;
    virtual bool __do_upcast(const __cxx_class_info* target,
                             void** obj_ptr) const;
};

// One type_info object per class (statically allocated in .rodata section)
// The compiler generates typeinfo_for_ClassName for each class that participates in polymorphism
```

```
Memory layout of type_info:

┌──────────────────────────────────────┐
│ vptr (type_info's own vtable)        │  ← type_info also has virtual functions
├──────────────────────────────────────┤
│ __typeinfo_name (void*)              │  → mangled name (lazily obtained)
├──────────────────────────────────────┤
│ __name (const char*)                 │  → demangled name (used by name())
└──────────────────────────────────────┘

Storage location:
  · The type_info object itself resides in the .rodata section (read-only data)
  · The mangled name string is also in the .rodata section
  · At most one type_info instance per type in each translation unit
  · The linker merges type_info objects for the same type across different .o files
    via weak symbols
```

### 6.2 name() and Comparison

```cpp
#include <typeinfo>
#include <iostream>

struct Widget {};
struct Gadget : Widget {};

void demo() {
    const std::type_info& t1 = typeid(Widget);
    const std::type_info& t2 = typeid(Gadget);

    // name() returns an implementation-defined mangled name
    std::cout << t1.name() << "\n";  // 6Widget (GCC) | struct Widget (MSVC)
    std::cout << t2.name() << "\n";

    // Comparison operations
    // before(): used for ordering; implementation defines total ordering
    bool ordered = t1.before(t2);

    // ==: determines if the same type (pointer comparison, O(1))
    bool same = (t1 == t2);  // false

    // hash_code(): C++11, for use with unordered containers
    size_t h = t1.hash_code();
}
```

---

## 7. `dynamic_cast` Implementation

### 7.1 Cast Types

```cpp
class Base { public: virtual ~Base() = default; };
class Derived : public Base { int x; };
class Unrelated { int y; };

Base* bp = new Derived;

// 1. Downcast — type_info comparison + offset adjustment
Derived* dp = dynamic_cast<Derived*>(bp);  // succeeds → returns adjusted pointer

// 2. Crosscast — traverse the type hierarchy
class Left  { public: virtual ~Left() = default; };
class Right { public: virtual ~Right() = default; };
class Diamond : public Left, public Right { public: ~Diamond() override = default; };
Left* lp = new Diamond;
Right* rp = dynamic_cast<Right*>(lp);  // succeeds → offset from Diamond → Right

// 3. Failed cast
Unrelated* up = dynamic_cast<Unrelated*>(bp);  // returns nullptr (pointer version)
// The reference version throws std::bad_cast
```

### 7.2 Runtime Algorithm

```
Steps of dynamic_cast<TargetType*>(src_ptr) implementation:

  1. If src_ptr == nullptr → return nullptr (null pointer cast is always null)

  2. Load the RTTI pointer from src_ptr's vtable
     → obtain src_type = *RTTI_pointer

  3. Compare src_type with target_type:
     a. If src_type == target_type (same type_info) → return directly
     b. If it is a subclass of the target type → adjust offset upward, return
     c. If it is a superclass of the target type → check downward; return nullptr on failure

  4. Crosscast:
     · Traverse the complete type hierarchy graph (DFS/BFS)
     · Check each intermediate type's type_info for a match with target_type
     · The Itanium ABI uses __class_type_info hierarchy structures
     · This path is slower — may need to traverse the entire inheritance tree

  5. If no match found → return nullptr (pointer) or throw bad_cast (reference)

Performance characteristics:
  · Same-type cast: O(1) — one type_info pointer comparison
  · Single-inheritance downcast: O(depth) — depth is the inheritance hierarchy depth
  · Crosscast: O(N) — N is the number of classes in the inheritance tree
```

---

## 8. `typeid` Implementation

```cpp
// Two usages of typeid
void demo(Base* p) {
    // 1. Static type known (compile time)
    const std::type_info& t1 = typeid(int);     // returns static type_info directly
    const std::type_info& t2 = typeid(Base);     // does not dereference pointer

    // 2. Polymorphic type (runtime — obtained through vtable)
    const std::type_info& t3 = typeid(*p);       // dereferences a polymorphic pointer
    // Implementation: p->vptr[-2] → RTTI pointer → type_info object
}
```

```
Compiled code for typeid(*p):

    // p points to a polymorphic object
    vptr = *(void***)p;        // load vptr
    rtti_ptr = vptr[-2];       // RTTI pointer is at vtable offset -2
    return *rtti_ptr;          // return type_info reference

Notes:
  · typeid(int) and other built-in types are resolved at compile time, with no runtime cost
  · typeid(non-polymorphic type) does not need a vtable; determined at compile time
  · typeid(*p) performs a runtime query only when p's static type is polymorphic
  · If p is null → std::bad_typeid exception
```

---

## 9. Impact of `-fno-rtti`

### 9.1 Behavior When RTTI Is Disabled

```
Effects of the -fno-rtti compiler flag:

  ┌─────────────────────────────────────────────────────────────┐
  │ Disabled features                                           │
  ├─────────────────────────────────────────────────────────────┤
  │ · typeid on polymorphic types → compile error               │
  │ · dynamic_cast → compile error                              │
  │ · type_info class → unavailable                             │
  │ · RTTI slots in vtable → not generated (reduces binary size)│
  ├─────────────────────────────────────────────────────────────┤
  │ Preserved features                                          │
  ├─────────────────────────────────────────────────────────────┤
  │ · Virtual function calls → work normally (vtable still      │
  │   exists)                                                   │
  │ · typeid(int) and other built-in types → compile-time       │
  │   resolution, unaffected                                    │
  │ · typeid of non-polymorphic types → compile-time resolution,│
  │   unaffected                                                │
  └─────────────────────────────────────────────────────────────┘

Typical use cases:
  · Embedded systems (reducing binary size)
  · Game engines (eliminating RTTI metadata when dynamic_cast is known to be unnecessary)
  · The LLVM/Clang project itself uses -fno-rtti + LLVM's own type system
```

### 9.2 Alternatives Under `-fno-rtti`

```cpp
// Approach 1: Enum-based type ID
class Shape {
public:
    enum class Kind { Circle, Square, Triangle };
    virtual Kind kind() const = 0;  // much faster than dynamic_cast
};

class Circle : public Shape {
    Kind kind() const override { return Kind::Circle; }
};

// Usage
void draw(Shape* s) {
    switch (s->kind()) {  // direct virtual call, no type_info comparison
    case Shape::Kind::Circle:  draw_circle(static_cast<Circle*>(s)); break;
    case Shape::Kind::Square:  draw_square(static_cast<Square*>(s)); break;
    }
}

// Approach 2: LLVM-style RTTI (classof + isa/cast/dyn_cast)
// Uses enum IDs for type checking and static_cast instead of dynamic_cast
// https://llvm.org/docs/HowToSetUpLLVMStyleRTTI.html
```

---

## 10. RTTI Overhead Model

```
Space overhead:
  ┌───────────────────────────────────────────────────────┐
  │ Per polymorphic class:                                │
  │   · 1 type_info object (approx. 16-32 bytes, pointer │
  │     + name)                                           │
  │   · 1 mangled name string (length varies by class    │
  │     name)                                             │
  │   · 1 RTTI pointer slot in the vtable (8 bytes)      │
  │                                                       │
  │ Total: approx. 50-100 bytes per class (including name)│
  │ 1000 polymorphic classes ≈ 50-100 KB                  │
  └───────────────────────────────────────────────────────┘

Time overhead:
  ┌───────────────────────────────────────────────────────┐
  │ Operation                 │ Overhead                   │
  ├───────────────────────────┼───────────────────────────┤
  │ typeid(*p)                │ 1 vtable read + 1 pointer │
  │                           │ dereference (~nanoseconds) │
  ├───────────────────────────┼───────────────────────────┤
  │ dynamic_cast<T*>(p)       │ Same-type O(1), hierarchy  │
  │ (same type)               │ traversal O(depth),        │
  │                           │ crosscast O(N)             │
  ├───────────────────────────┼───────────────────────────┤
  │ dynamic_cast<T*>(p)       │ type_info::before() compare│
  │ (crosscast)               │ traverse inheritance graph │
  │                           │ — relatively slow          │
  ├───────────────────────────┼───────────────────────────┤
  │ type_info::name()         │ Returns pointer only       │
  │                           │ (zero overhead)            │
  ├───────────────────────────┼───────────────────────────┤
  │ type_info == comparison   │ Pointer comparison O(1)    │
  └───────────────────────────┴───────────────────────────┘

Compared to the overhead of a virtual call itself:
  · Virtual call: 1 vptr read + 1 function pointer read + indirect jump
  · RTTI query (typeid): 1 additional pointer dereference
  · dynamic_cast: significantly more than typeid — especially for crosscasts
```

---

## 11. Devirtualization

Compilers replace virtual calls with direct calls where possible, eliminating indirect jump overhead and enabling inlining.

### 11.1 Scenarios Where the Compiler Can Automatically Devirtualize

```cpp
// Scenario 1: Known concrete type
Derived d;
Base& b = d;
b.speak();  // compiler knows b is bound to Derived → calls Derived::speak() directly

// Scenario 2: Final class
class Final final : public Base {
    void speak() override final;  // no subclass can override
};

void call(Final& f) {
    f.speak();  // compiler calls Final::speak() directly

// Scenario 3: Final virtual function
class Base2 {
    virtual void done() final;  // cannot be overridden further
};

// Scenario 4: LTO (Link-Time Optimization) — full hierarchy visible across
// translation units. The compiler can determine all override relationships at link time.
```

### 11.2 LLVM's Devirtualization Implementation

```
LLVM's devirtualization passes:

  1. -fstrict-vtable-pointers (annotate vptr invariance assumptions)
     Between completion of object construction and the start of destruction,
     the vptr is invariant.
     LLVM inserts llvm.assume to annotate this.

  2. GlobalDerefPass (IPO pass)
     · Collects all virtual function override relationships
     · If a virtual function has only one possible implementation → replace
       with direct call
     · Works best in combination with LTO

  3. Annotations (attributes):
     · __attribute__((annotate("vtable_visibility", "all")))
       tells the compiler that all subclasses are known
     · [[clang::noescape]] and similar attributes assist analysis

Manual devirtualization techniques:
    auto& d = dynamic_cast<Derived&>(b);  // after the cast, the compiler knows the
                                           // concrete type
    d.speak();  // can be called directly

Hinting the compiler:
    __builtin_assume(dynamic_cast<Derived*>(p) != nullptr);
    p->speak();  // some compilers can use this assumption to devirtualize
```

---

## 12. CRTP: Compile-Time Polymorphism Alternative

CRTP (Curiously Recurring Template Pattern) achieves static polymorphism at compile time, completely eliminating virtual function overhead.

```cpp
// CRTP base class
template <typename Derived>
class ShapeBase {
public:
    void draw() const {
        // Compile-time dispatch: calls Derived's draw_impl
        static_cast<const Derived*>(this)->draw_impl();
    }

    double area() const {
        return static_cast<const Derived*>(this)->area_impl();
    }
};

// Derived classes
class Circle : public ShapeBase<Circle> {
    friend class ShapeBase<Circle>;  // allow base to access private implementation
    double radius_;
    void draw_impl() const { /* draw circle */ }
    double area_impl() const { return 3.14159265 * radius_ * radius_; }
};

class Square : public ShapeBase<Square> {
    friend class ShapeBase<Square>;
    double side_;
    void draw_impl() const { /* draw square */ }
    double area_impl() const { return side_ * side_; }
};

// Usage (cannot store in a homogeneous container — types differ)
Circle c;
c.draw();        // compiler calls Circle::draw_impl() directly
                 // fully inlined, zero indirect call overhead
```

### 12.1 CRTP vs Virtual Functions Comparison

```
┌──────────────────────┬───────────────────┬──────────────────────┐
│ Feature              │ Virtual Functions │ CRTP                 │
├──────────────────────┼───────────────────┼──────────────────────┤
│ Dispatch mechanism   │ Runtime (vtable)  │ Compile-time         │
│                      │                   │ (template            │
│                      │                   │  instantiation)      │
├──────────────────────┼───────────────────┼──────────────────────┤
│ Indirect call        │ Yes (every call)  │ No (inlined)         │
│ overhead             │                   │                      │
├──────────────────────┼───────────────────┼──────────────────────┤
│ Code size            │ Shared            │ One instance per type │
├──────────────────────┼───────────────────┼──────────────────────┤
│ Homogeneous          │ Yes (Base*)       │ No (different types) │
│ container            │                   │                      │
├──────────────────────┼───────────────────┼──────────────────────┤
│ Runtime polymorphism │ Supported         │ Not supported        │
├──────────────────────┼───────────────────┼──────────────────────┤
│ Inlining potential   │ Poor (indirect    │ Excellent (static    │
│                      │  jump)            │  dispatch)           │
├──────────────────────┼───────────────────┼──────────────────────┤
│ Compile time         │ Fast              │ Template bloat →     │
│                      │                   │  slower              │
├──────────────────────┼───────────────────┼──────────────────────┤
│ Debugging experience │ Intuitive         │ Template error       │
│                      │                   │  messages are hard   │
│                      │                   │  to read             │
└──────────────────────┴───────────────────┴──────────────────────┘

Selection guidance:
  · Need runtime polymorphism (base class pointers/references, factory pattern)
    → virtual functions
  · Performance-critical path, fixed set of types, no heterogeneous container needed
    → CRTP
  · std::visit + std::variant → also provides compile-time polymorphism
```

---

## 13. Virtual Destructors: Necessity and Implementation

### 13.1 Why Virtual Destructors Are Needed

```cpp
class Base {
public:
    ~Base() { /* non-virtual */ }  // Dangerous!
};

class Derived : public Base {
    std::vector<int> data_;
public:
    ~Derived() { /* destroy data_ */ }
};

Base* p = new Derived;
delete p;  // UB: deleting derived object through base pointer with non-virtual destructor
           // Only ~Base() is called, ~Derived() is not called
           // → data_ leaks, and ~Derived() may release critical resources
```

```
How virtual destructors work:

Compiled code for delete p:

    // p's static type is Base*, Base has a virtual destructor
    vptr = *(void***)p;
    fn_ptr = vptr[3];  // deleting destructor is at vtable slot 3

    fn_ptr(p);
    // Calls Derived's deleting destructor:
    //   1. Calls ~Derived()    ← derived class destruction
    //   2. Calls ~Base()       ← base class destruction
    //   3. operator delete(p)  ← free memory

Rules of thumb:
  · Any class that has at least one virtual function → must have a virtual destructor
  · Any class not used as a base (e.g. std::string_view) → does not need one
  · C++ Core Guidelines C.35: a base class destructor should be either public virtual
    or protected non-virtual
```

### 13.2 `protected non-virtual` Destructor Alternative

```cpp
// When you don't want to support deletion through base pointer, use protected
// non-virtual destructor
class Interface {
protected:
    ~Interface() = default;  // external code cannot delete Interface*
public:
    virtual void process() = 0;
};

class Impl : public Interface {
public:
    void process() override { /* ... */ }
    ~Impl() = default;       // can only delete Impl*
};

// Interface* p = new Impl;
// delete p;        // compile error: ~Interface() is protected
// Impl* q = new Impl;
// delete q;        // OK
```

---

## 14. Implementation Details and Platform Differences

### 14.1 Itanium ABI vs MSVC ABI

```
┌────────────────────────┬──────────────────────┬────────────────────────┐
│ Feature                │ Itanium ABI          │ MSVC ABI               │
│                        │ (GCC/Clang/Linux)    │ (MSVC/Windows)         │
├────────────────────────┼──────────────────────┼────────────────────────┤
│ RTTI position in       │ Negative offset      │ First slot in vtable   │
│ vtable                 │ (offset -2)          │ (positive offset 0)    │
├────────────────────────┼──────────────────────┼────────────────────────┤
│ Function start offset  │ Offset 0             │ Offset 1 (skip RTTI)   │
│ in vtable              │                      │                        │
├────────────────────────┼──────────────────────┼────────────────────────┤
│ offset_to_top          │ vtable negative      │ Does not exist         │
│                        │ offset -3;           │ Uses offset embedded   │
│                        │ secondary vtable use │ in RTTI                │
├────────────────────────┼──────────────────────┼────────────────────────┤
│ Virtual destructor     │ 2 (deleting +        │ 1 (scalar/vector) +    │
│ slots                  │  complete)           │  separate destructor   │
├────────────────────────┼──────────────────────┼────────────────────────┤
│ RTTI implementation    │ type_info + derived  │ Complete class         │
│                        │ __class_type_info    │ hierarchy              │
│                        │                      │ RTTIClassHierarchy     │
├────────────────────────┼──────────────────────┼────────────────────────┤
│ VTT (virtual           │ Yes                  │ No (uses a different   │
│ inheritance vtable     │                      │  initialization        │
│ table)                 │                      │  mechanism)            │
├────────────────────────┼──────────────────────┼────────────────────────┤
│ type_info name()       │ Returns mangled name │ Returns demangled name │
└────────────────────────┴──────────────────────┴────────────────────────┘
```

### 14.2 MSVC's RTTI Structure

```
MSVC's Complete Object Locator (COL):
  vtable[0] = RTTI Complete Object Locator
  vtable[1] = first virtual function
  ...

COL contains:
  · signature (identifier)
  · offset (from object start to vtable)
  · cdOffset (construction displacement table offset)
  · type_descriptor (the class's type_info)
  · class_hierarchy_descriptor (class hierarchy description)

MSVC's ClassHierarchyDescriptor:
  · signature
  · attributes (whether multiple inheritance, etc.)
  · num_base_classes
  · base_class_array[] → points to each base class's BaseClassDescriptor

This is more explicit than the Itanium ABI — MSVC stores the complete class
hierarchy graph in RTTI, used for hierarchy traversal during dynamic_cast.
```

---

## 15. Best Practices Summary

```
┌────────────────────────────────────────────────────────────────────┐
│ Design Decisions                                                   │
├────────────────────────────────────────────────────────────────────┤
│ 1. Need polymorphism → virtual functions + virtual destructor      │
│ 2. Performance-critical with fixed types → CRTP or std::variant    │
│ 3. Frequent dynamic_cast → consider refactoring (virtual function  │
│    dispatch, visitor pattern)                                      │
│ 4. Definitely don't need RTTI → -fno-rtti (saves space, use       │
│    enum IDs as replacement)                                        │
│ 5. Don't need inheritance-based deletion → protected non-virtual   │
│    destructor                                                      │
│ 6. Final classes/functions → help the compiler devirtualize        │
├────────────────────────────────────────────────────────────────────┤
│ Performance Guidelines                                             │
├────────────────────────────────────────────────────────────────────┤
│ · Virtual call overhead itself is minimal (~nanoseconds), not a    │
│   performance bottleneck                                           │
│ · The real performance issue is: virtual functions prevent          │
│   inlining → amplifying cumulative overhead in call chains         │
│ · dynamic_cast crosscasts are slow → avoid on hot paths            │
│ · Virtual function memory layout is not cache-friendly → consider  │
│   SoA / flattened design                                           │
│ · Place virtual functions at the outermost layer of the call chain,│
│   use non-virtual functions internally                             │
└────────────────────────────────────────────────────────────────────┘
```

---

## Further Reading

- [Compiler Optimizations Overview](/topics/compiler-optimizations) — the complete picture of devirtualization optimizations
- [Template Metaprogramming](/topics/template-metaprogramming) — advanced applications of CRTP
- [Object Model and Memory Layout](/topics/memory-model) — memory alignment and layout optimization
- Itanium C++ ABI Specification — https://itanium-cxx-abi.github.io/cxx-abi/abi.html
- LLVM HowToSetUpLLVMStyleRTTI — https://llvm.org/docs/HowToSetUpLLVMStyleRTTI.html
- C++ Core Guidelines C.35, C.127 — virtual destructor rules
