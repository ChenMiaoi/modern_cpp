---
title: C++ Object Model
topic: topics
feature: object-model
standard: C++
status_checked_at: 2026-06-02
exercises: []
solutions: []
---

# C++ Object Model

> The C++ object model is the language's most fundamental abstraction: how a block of memory becomes an object, how objects are laid out, how pointers track object identity, and when lifetimes begin and end. Understanding the object model is a prerequisite for understanding RAII, templates, polymorphism, and concurrency.

---

## Object Creation and Storage Duration

Every object in C++ has a **storage duration**, which determines when its memory is allocated and released.

### Four Storage Durations

```cpp
// 1. Automatic storage duration — local variables in functions
void f() {
    int x = 42;          // x lives on the stack frame, destroyed when leaving scope
    std::string s = "hi"; // s and its internal buffer — destructor called at }
}

// 2. Static storage duration — globals, namespace scope, static locals
int global = 100;              // initialized at program startup, destroyed at program exit
void g() {
    static int counter = 0;   // initialized on first call, destroyed at program exit
    static std::mutex mtx;    // same — strict rules for construction/destruction order across translation units
}

// 3. Thread storage duration (thread_local) — C++11
thread_local int tls_var = 0;  // independent copy per thread, destroyed on thread exit

// 4. Dynamic storage duration — new / delete
void h() {
    auto* p = new Widget{};    // allocated on the heap, must be explicitly deleted
    delete p;
}
```

### Initialization Order for Static Storage Duration

```
┌──────────────────────────────────────────────────────────┐
│  Static storage duration initialization — two-phase model│
│                                                          │
│  Phase 1: zero-initialization                            │
│    All variables with static storage duration are zeroed  │
│    before any other initialization                        │
│                                                          │
│  Phase 2: constant initialization or dynamic initialization│
│    · constant initialization: constexpr-evaluable → done  │
│      at compile time                                      │
│    · dynamic initialization: constructors/initializer     │
│      expressions executed at runtime                      │
│                                                          │
│  Within the same translation unit: initialized in order   │
│  Across translation units: order is undefined →           │
│  "the static initialization order fiasco"                 │
└──────────────────────────────────────────────────────────┘
```

---

## Object Representation and Value Representation

The standard distinguishes two concepts:

- **Object representation**: the contiguous sequence of memory bytes occupied by the object
- **Value representation**: the set of bits that determine the object's value

```cpp
// In most cases they are the same, but bit-fields break this alignment
struct Flags {
    unsigned int active  : 1;  // 1 bit
    unsigned int visible : 1;  // 1 bit
    unsigned int padding : 30; // 30 bits — padded to 32 bits
};

// sizeof(Flags) == 4 (object representation = 4 bytes)
// value representation = all 32 bits out of 32 bits (all participate in value determination)

// For bool:
// sizeof(bool) == 1 (object representation = 1 byte = 8 bits)
// value representation = 1 bit (only 0 and 1 are used, the other 7 bits are padding)
// reading padding bits → undefined behavior
```

---

## Alignment and Padding

Every type has an **alignment requirement**, measured in bytes.

```
struct Example {
    char  a;    // offset 0, size 1
    // ---- 3 bytes padding (to align b to a multiple of 4) ----
    int   b;    // offset 4, size 4
    char  c;    // offset 8, size 1
    // ---- 3 bytes padding (to make overall size a multiple of alignment) ----
};
// sizeof(Example) == 12, alignof(Example) == 4

Memory layout:
offset:  0    1    2    3    4    5    6    7    8    9   10   11
       ┌────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┐
       │ a  │pad │pad │pad │ b  . b  . b  . b  │ c  │pad │pad │pad │
       └────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┘
       ├─char─┤├──── 3B pad ───┤├──── int ─────┤├char┤├── 3B pad ──┤
```

```cpp
// C++11: alignas controls alignment
struct alignas(64) CacheLine {
    char data[64];
};
// alignof(CacheLine) == 64 — suitable for aligning to cache line boundaries

// alignof queries
static_assert(alignof(double) == 8);
static_assert(alignof(int)    == 4);
```

### Reordering Members to Reduce Padding

```cpp
// Wastes space
struct Wasteful {
    char  a;   // +1, pad 3
    int   b;   // +4
    char  c;   // +1, pad 7
    double d;  // +8
};  // sizeof == 24

// Compact layout (ordered by descending alignment requirement)
struct Compact {
    double d;  // +8
    int    b;  // +4
    char   a;  // +1
    char   c;  // +1, pad 2
};  // sizeof == 16 — saves 33%
```

---

## POD / Trivially Copyable / Standard Layout

The standard classifies types into several layers of "triviality" and "layout-ness":

```
┌──────────────────────────────────────────────────────────────────┐
│  Type trait hierarchy (C++17/20 simplified view)                 │
│                                                                  │
│  is_trivial = is_trivially_copyable + is_trivially_default_constructible
│                                                                  │
│  is_standard_layout:                                             │
│    · All non-static data members have the same access control    │
│    · No virtual functions / virtual base classes                 │
│    · All non-static data members are also standard_layout        │
│    · At most one base class has non-static data members          │
│    · First member type differs from base class (relaxed in C++17)│
│                                                                  │
│  is_trivially_copyable:                                          │
│    · No non-trivial copy/move constructors or assignment ops     │
│    · No non-trivial destructor                                   │
│    · Can be safely copied with memcpy                            │
│                                                                  │
│  C++20: POD = is_trivial + is_standard_layout (conceptualized)   │
└──────────────────────────────────────────────────────────────────┘
```

```cpp
struct Pod { int x; float y; };             // trivial + standard layout
struct StdLayout { int x; virtual ~StdLayout(){} }; // standard layout? No — has virtual function
struct Trivial { std::string s; };           // trivially copyable? No — string is non-trivial

// Why does this matter?
// 1. trivially_copyable → can safely memcpy/memmove
// 2. standard_layout → interoperable with C, pointers can be reinterpret_cast
// 3. trivial → reading uninitialized storage has defined behavior (C++20 implicit object creation)
static_assert(std::is_trivially_copyable_v<Pod>);
static_assert(std::is_standard_layout_v<Pod>);
```

---

## Strict Aliasing Rule and Type Punning

**Strict aliasing rule**: a program shall not access an object through an lvalue of an incompatible type (exceptions: `char*`/`unsigned char*`/`std::byte*` are allowed to inspect any object at byte granularity).

```cpp
// ❌ Violates strict aliasing — undefined behavior
float f = 3.14f;
int* ip = reinterpret_cast<int*>(&f);
int bits = *ip;  // UB: accessing float object through int*

// ❌ Also UB — union type punning (allowed in C, not guaranteed in C++)
union { float f; int i; } u;
u.f = 3.14f;
int bits = u.i;  // UB in C++ (GCC/Clang may produce wrong results with strict aliasing optimizations)

// ✅ Correct approach — use memcpy
float f = 3.14f;
int bits;
std::memcpy(&bits, &f, sizeof(bits));  // defined behavior, compiler optimizes away the copy

// ✅ C++20: std::bit_cast
int bits = std::bit_cast<int>(3.14f);  // compile-time friendly, no UB
```

---

## Object Identity and Value Identity

```cpp
// Object identity: the address of the object in memory
int a = 1, b = 1;
&a != &b;          // different objects, even though the values are the same

// Value identity: the value currently represented by the object
a == b;             // same value

// Key scenario: references bind to object identity, not value
struct Animal { virtual ~Animal() = default; };
struct Dog : Animal { void bark() {} };
Dog d;
Animal& ref = d;   // ref binds to the object d (identity)
// dynamic_cast<Dog&>(ref) is safe — because the dynamic type of ref is indeed Dog

// std::string's SSO (Small String Optimization) complicates value identity:
// Short strings store characters in the object's own stack space; long strings use the heap
// But s.data()'s pointer may be invalidated after assignment — it points to the object's internal storage
```

---

## Empty Base Optimization (EBO)

C++ guarantees that distinct objects have distinct addresses, but **base class subobjects** can share an address with other subobjects.

```cpp
struct Empty {};
struct NonEmpty { int x; };

// Normal case: sizeof(Empty) == 1 (guarantees distinct addresses for distinct instances)
static_assert(sizeof(Empty) == 1);

// Empty base optimization: base class subobjects can occupy zero space
struct Derived : Empty {
    int value;
};
// sizeof(Derived) == 4 (not 8), Empty subobject's address == Derived's address

// Multiple empty bases
struct A {};
struct B {};
struct C : A, B {
    int value;
};
// sizeof(C) == 4 — both empty bases take no extra space
// But &static_cast<A&>(c) != &static_cast<B&>(c) — the two base subobjects have different addresses
// Compiler assigns different offsets to different empty bases (0 and 1 byte, etc.)

// C++20: [[no_unique_address]] — members can also benefit from EBO
struct CompressedPair {
    [[no_unique_address]] Empty e;
    int value;
};
// sizeof(CompressedPair) == 4 — e takes no space

// Practical use: allocators are typically empty classes
template <typename T, typename Alloc = std::allocator<T>>
class Vector {
    T* data_;
    std::size_t size_, capacity_;
    [[no_unique_address]] Alloc alloc_;  // default allocator takes no space
};
// sizeof(Vector<int>) == 24 (three pointers), not 32
```

```
EBO memory layout:

Normal inheritance:                  Empty base optimization:
┌───────────────────┐            ┌───────────────────┐
│ Empty subobject(1B)│            │ Empty+int (shared  │
│ padding (3B)      │            │  address)           │
│ value (4B)        │            │ value (4B)          │
└───────────────────┘            └───────────────────┘
 sizeof == 8                       sizeof == 4

[[no_unique_address]] member:
┌────────────────────────────┐
│ e (shares address with     │
│  value, 0B)                │
│ value (4B)                 │
└────────────────────────────┘
 sizeof == 4
```

---

## Virtual Functions and vtable Layout

Virtual function calls use a **virtual table** (vtable) for indirect dispatch. Each class with virtual functions has a static vtable; each object with virtual functions holds a pointer to the vtable (vptr).

```cpp
class Base {
public:
    virtual void f() { /* ... */ }
    virtual void g() { /* ... */ }
    virtual ~Base() = default;
    int x = 0;
};

class Derived : public Base {
public:
    void f() override { /* ... */ }
    virtual void h() { /* ... */ }
    int y = 0;
};
```

```
Base object layout:              Derived object layout:

┌──────────────┐              ┌──────────────┐
│ vptr ──────────┐            │ vptr ──────────┐
├──────────────┤  │            ├──────────────┤  │
│ x (4B)       │  │            │ x (4B)       │  │
├──────────────┤  │            ├──────────────┤  │
│ padding (4B) │  │            │ y (4B)       │  │
└──────────────┘  │            └──────────────┘  │
sizeof(Base)==16  │            sizeof(Derived)==16│
                  ▼                               ▼
Base::vtable:     Derived::vtable:
┌──────────────────┐  ┌──────────────────┐
│ &Base::f()       │  │ &Derived::f()    │ ← override
├──────────────────┤  ├──────────────────┤
│ &Base::g()       │  │ &Base::g()       │ ← inherited
├──────────────────┤  ├──────────────────┤
│ &Base::~Base()   │  │ &Derived::~Derived() │
├──────────────────┤  ├──────────────────┤
│ typeinfo (RTTI)  │  │ &Derived::h()    │ ← added
│                  │  ├──────────────────┤
└──────────────────┘  │ typeinfo (RTTI)  │
                      └──────────────────┘
```

```cpp
// Overhead of virtual calls:
// 1. One pointer dereference (read vptr → read vtable entry)
// 2. Indirect jump (prevents inlining — compiler usually cannot determine target at compile time)
// 3. One extra pointer of storage per object (vptr)

// Devirtualization: when the compiler can determine the dynamic type at compile time
Derived d;
Base& b = d;
b.f();  // compiler may directly call Derived::f(), skipping the vtable lookup

// C++17: final classes prevent further inheritance → aids devirtualization
class FinalDerived final : public Base {
    void f() override final { /* ... */ }
};
```

---

## Dynamic Object Creation: new/delete Internals

```cpp
// operator new call chain:
Widget* p = new Widget(args);
// Equivalent to:
void* raw = ::operator new(sizeof(Widget));  // 1. allocate raw memory
Widget* p = new(raw) Widget(args);            // 2. placement new — construct

// operator delete:
delete p;
// Equivalent to:
p->~Widget();           // 1. destruct
::operator delete(p);   // 2. release memory

// Custom global operator new/delete (usually only for special scenarios)
void* operator new(std::size_t size) {
    void* ptr = std::malloc(size);
    if (!ptr) throw std::bad_alloc{};
    return ptr;
}
void operator delete(void* ptr) noexcept {
    std::free(ptr);
}
```

Calling `operator new` to allocate raw memory → placement new on that memory to construct the object → return pointer on success; if the constructor throws, `operator delete` is automatically called to release the memory (no leak). When `operator new` fails, it invokes `new_handler`; if that is `nullptr`, it throws `std::bad_alloc`.

---

## Placement new

```cpp
#include <new>  // header required for placement new

// 1. Construct an object in a pre-allocated buffer
alignas(Widget) char buf[sizeof(Widget)];
Widget* w = new(buf) Widget(args);
// w points to buf, object is constructed in-place within buf
w->~Widget();  // must manually destruct — cannot delete (buf was not allocated by new)

// 2. Typical usage in containers: separate allocation from construction
//  First allocate raw memory with ::operator new, then construct elements one by one with placement new
//  C++20: std::construct_at / std::destroy_at (constexpr friendly)
auto* p = std::construct_at(ptr, args...);
std::destroy_at(p);  // C++17
```

---

## std::launder and Pointer Provenance

`std::launder` (C++17) solves this problem: when an object is replaced in-place (e.g., via placement new overwriting), a pointer to the old object may be optimized by the compiler to still refer to the old value.

```cpp
struct X { const int n; };

X x{42};
X* p = &x;

// Reconstruct a new object at x's location
new (&x) X{100};

// ❌ Undefined behavior: p may be optimized by the compiler to still read 42
int val = p->n;

// ✅ Correct: use launder to get a pointer to the new object
int val = std::launder(p)->n;  // guaranteed to read 100

// When you need launder:
// 1. A new object is created at the same address after the old object's lifetime ended
// 2. The old and new objects have the same type but const members or reference members
// 3. The pointer value is unchanged but the object it points to has been "replaced"
//
// When you do not need launder:
// 1. The object's lifetime has not ended (only its value was modified)
// 2. A pointer returned by placement new (it is already a pointer to the new object)
// 3. A pointer obtained via std::start_lifetime_as (C++23)
```

---

## Object Lifetime Rules

### When Lifetime Begins and Ends

```
Object lifetime begins:
  · For trivially copyable + implicit lifetime types → implicitly created (see below)
  · Otherwise → when the constructor completes

Object lifetime ends:
  · When the destructor call begins
  · Or when the storage is released or reused (e.g., placement new overwrites)

After an object's lifetime ends:
  · The name is still in scope, but references/pointers to the object become invalid
  · Reading through an invalid pointer → UB (unless trivially copyable + implicit lifetime)
```

### C++20 Lifetime Rule Changes

```cpp
// Before C++20: accessing a const member after destruction was UB
// C++20 relaxed the rules for trivially destructible objects

struct Trivial { int x; };  // trivially destructible
Trivial t{42};
t.~Trivial();  // lifetime ends

// Before C++20: UB
// C++20: if conditions for implicit object creation are met, behavior is defined
//        but still not recommended — use std::launder or construct_at for safety
```

---

## Implicit Object Creation (C++20)

C++20 introduces **implicit object creation**: certain operations automatically create objects in raw storage, thereby legitimizing pointers.

```cpp
// Core idea: the following operations, if needed, implicitly create objects
// · malloc / calloc / realloc / aligned_alloc
// · operator new / operator new[]
// · std::allocator::allocate
// · std::start_lifetime_as (C++23)
// · memcpy / memmove / memset, etc. (from C++20)

// Example: using memory directly after malloc
struct S { int x; };
void* raw = std::malloc(sizeof(S));
S* p = static_cast<S*>(raw);    // C++20: implicitly creates an S object
p->x = 42;                       // defined behavior! (was UB before C++20)

// But there are limitations: implicit creation only occurs in storage that
// meets alignment requirements, and only creates objects of trivially copyable types

// Implicit creation of multiple objects
struct Header { int size; };
struct Payload { char data[64]; };
struct Packet { Header h; Payload p; };

void* buf = std::malloc(sizeof(Packet));
Packet* pkt = static_cast<Packet*>(buf);
pkt->h.size = 42;  // C++20: implicitly creates the entire Packet → Header and Payload are also implicitly created
```

---

## Comprehensive Example: Hand-written fixed_capacity_vector

```cpp
#include <new>
#include <memory>
#include <type_traits>

template <typename T, std::size_t N>
class fixed_capacity_vector {
    // Storage: sufficiently large aligned buffer
    alignas(T) unsigned char buf_[N * sizeof(T)];
    std::size_t size_ = 0;

public:
    static_assert(std::is_nothrow_destructible_v<T>,
        "T must be nothrow destructible for exception safety");

    fixed_capacity_vector() = default;

    ~fixed_capacity_vector() {
        clear();
    }

    void push_back(auto&&... args) {
        if (size_ >= N) throw std::length_error("full");
        std::construct_at(data() + size_++, std::forward<decltype(args)>(args)...);
    }

    void pop_back() {
        if (size_ == 0) return;
        std::destroy_at(data() + --size_);
    }

    void clear() {
        std::destroy_n(data(), size_);
        size_ = 0;
    }

    T& operator[](std::size_t i) { return data()[i]; }
    const T& operator[](std::size_t i) const { return data()[i]; }
    std::size_t size() const { return size_; }

private:
    T* data() { return std::launder(reinterpret_cast<T*>(buf_)); }
    const T* data() const { return std::launder(reinterpret_cast<const T*>(buf_)); }
};
```

---

## Common Pitfalls

1. **`sizeof` includes padding, `alignof` is not always equal to `sizeof`**. `sizeof(long double)` may be 8, 12, or 16 depending on the platform.
2. **`reinterpret_cast<T*>(buf)` does not immediately create an object**. Before C++20, a placement new is required first; C++20 only auto-implicitly-creates for trivially copyable types.
3. **The vptr is written multiple times during construction**. Each base class constructor sets the vptr to point to its own vtable; it is ultimately set to the most-derived vtable in the most-derived class's constructor.
4. **`delete this` is legal** — but no member may be accessed afterward, and the object must have been created with `new`.
5. **Zero-size `new` returns a unique non-null pointer**. `new char[0]` does not return `nullptr`; it is guaranteed to differ from any other `new` return value.
6. **`std::launder` is not a universal fix**. It can only "revive" an object of the same type at the same address; it cannot cross type boundaries, nor can it legitimize a pointer to freed memory.

## Further Reading

- [RAII and Resource Management](/topics/raii) — idioms for construction/destruction and resource management
- [Value Categories Deep Dive](/topics/value-categories-deep-dive) — lvalues, rvalues, xvalues, and move semantics
- [Compiler Optimizations Overview](/topics/compiler-optimizations) — how devirtualization, inlining, and alias analysis leverage the object model
- [Memory Model and Concurrency](/topics/memory-model) — visibility guarantees for objects across threads
- [Template Metaprogramming](/topics/template-metaprogramming) — implementation principles behind traits like `is_trivially_copyable`
