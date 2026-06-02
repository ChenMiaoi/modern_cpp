---
title: Object Lifetime
topic: topics
feature: lifetime
status_checked_at: 2026-06-02
standard: N/A
---

# Object Lifetime

## Birth and Death of Objects

C++ standard definition: **an object is a region of storage** ([intro.object]) that has a type, an address, and a lifetime. A lifetime begins when **initialization is complete** and ends when **destruction** occurs or **storage is reclaimed**.

```cpp
int main() {
    int x = 42;              // initialization complete → lifetime begins
    std::string s = "hello"; // constructor completes → lifetime begins
} // x's storage is reclaimed, s's destructor is called → both lifetimes end
```

**Core rules** ([basic.life]):

```
Lifetime begins:
  - class types: the entire construction process completes (not just the constructor body)
  - scalar types: the instant initialization completes
  - aggregates: after each member is initialized

Lifetime ends:
  - class types: the destructor is called
  - scalar/POD types: storage is reclaimed or reused
```

### When Destructors Are Called

```cpp
{
    std::string s = "hello";
    std::string t = std::move(s); // t is constructed, s is valid but in an unspecified state
} // t is destroyed first, then s (reverse order of construction)
```

Destructors are **not** called in the following cases: an object is constructed via placement new but the destructor is not explicitly called, the program terminates via `std::exit()`/`std::abort()`, or a destructor throws during stack unwinding triggering `std::terminate`.

### Partial Construction in Constructors

```cpp
class Widget {
    std::string name_;
    std::unique_ptr<int> data_;
public:
    Widget(std::string n)
        : name_(std::move(n))            // constructed first
        , data_(std::make_unique<int>(42)) // constructed second
    {
        // If an exception is thrown here: name_ is already constructed and will be destroyed,
        // data_ is not yet constructed so it will not be destroyed.
        // Already-constructed base classes and members are destroyed in reverse order.
    }
};
```

## Four Storage Durations

### Automatic Storage Duration

Local variables and function parameters are allocated on the stack and destroyed when leaving scope. The compiler guarantees destruction in reverse order, including during exception-triggered stack unwinding:

```cpp
void risky() {
    std::string a = "first";
    std::string b = "second";
    throw std::runtime_error("boom");
    // Compiler guarantees: b.~string() → a.~string() → exception propagation
}
```

**⚠️ Pitfall** — returning a reference/pointer to a local variable causes a dangling reference:

```cpp
int& bad() { int x = 42; return x; }      // ❌ dangling reference
std::string good() { std::string s = "h"; return s; } // ✅ NRVO / move
```

### Static Storage Duration

Global variables, `static` local variables, and `namespace`-scope variables have static storage duration. They are allocated at program startup and destroyed at program termination.

```cpp
int global = 42;
void f() { static int counter = 0; ++counter; } // initialized on first call
```

Initialization occurs in two phases: **zero initialization** (at program startup, guarantees no garbage values) → **dynamic initialization** (before `main()`). The order of dynamic initialization across translation units is undefined (**static initialization order fiasco**).

```cpp
// ❌ Classic pitfall: across translation units
// a.cpp: std::string a = "hello";
// b.cpp: extern std::string a; std::string b = a + " world"; // a may not be initialized yet

// ✅ Meyer's Singleton (since C++11, local static has thread-safe initialization)
std::string& get_a() { static std::string a = "hello"; return a; }
```

**`constinit` (C++20)**: forces constant initialization, avoiding the ordering and thread-safety issues of dynamic initialization:

```cpp
constinit int safe = 42;      // compile-time initialization, no runtime cost
constinit int mut = 0;
mut = 1;                       // ✅ constinit does not mean const
// constinit int bad = get_value(); // ❌ not a constant expression
```

### Thread-Local Storage Duration

`thread_local` variables exist independently per thread, initialized when the thread is created and destroyed when the thread ends:

```cpp
thread_local int tls_counter = 0; // independent copy per thread
void f() { static thread_local std::string s = "init"; } // per-thread lazy init
```

### Dynamic Storage Duration

Managed indirectly via `new`/`delete` or allocators:

```cpp
auto p = new int(42);           // lifetime begins
delete p;                       // lifetime ends

alignas(std::string) char buf[sizeof(std::string)];
auto ps = new (buf) std::string("hello"); // placement new
ps->~basic_string();                      // must explicitly destroy

// ⚠️ Common mistakes: forgetting delete (leak), double free (UB), use-after-free (UB)
```

## Lifetime of Sub-objects

Objects can contain member sub-objects, base-class sub-objects, and array elements. Construction proceeds in declaration order; destruction proceeds in reverse.

### Member Sub-objects

```cpp
class Container {
    std::vector<int> vec_;  // constructed first
    std::string name_;      // constructed second
public:
    Container() : vec_{1,2,3}, name_("c") {}
    // Destruction order: name_ → vec_
};

// ⚠️ The order of the initializer list is determined by declaration order
struct Trap {
    int a; int b;
    Trap() : b(42), a(b) {} // ❌ a is initialized first, b is not yet initialized
};
```

### Arrays and Derived Classes

```cpp
std::string arr[3]; // Construction: [0]→[1]→[2]  Destruction: [2]→[1]→[0]
// ⚠️ Using delete (instead of delete[]) on an array is UB

class Derived : public Base {
    std::string data_;
public:
    // Construction: Base → data_ → Derived body
    // Destruction: Derived body → data_ → Base
};
// ⚠️ Deleting a derived class object through a base class pointer requires virtual ~Base(), otherwise UB
```

## Accessing Objects Outside Their Lifetime — Undefined Behavior

Accessing an object outside its lifetime is UB ([basic.life]). The compiler may assume this does not happen and performs aggressive optimizations based on that assumption:

```cpp
int* p = new int(42);
delete p;
// The compiler may assume *p is no longer legally accessible
if (*p == 42) { /* may be optimized away entirely */ }
```

### std::launder — Crossing Lifetime Boundaries

Introduced in C++17, allows legal access to a "reborn" object under specific conditions:

```cpp
struct X { const int n; };
alignas(X) unsigned char buf[sizeof(X)];
X* p1 = new (buf) X{42};
X* p2 = new (buf) X{100}; // construct a new object in the same storage
// p1->n may be cached by the compiler as 42 (const member + same address)
int val = std::launder(p1)->n; // ✅ 100: tells the compiler to re-read from memory
```

### P0137R1 — Core Revision of the Object Model

Adopted in C++17: the compiler may assume a pointer is only used within the lifetime of the object it points to. This enables more aggressive optimizations for cross-type pointers and makes circumventing the type system via `reinterpret_cast` more clearly UB.

## std::start_lifetime_as (C++23)

Allows implicitly starting the lifetime of a new object in existing storage without calling a constructor:

```cpp
#include <memory>
alignas(int) unsigned char buf[4];
*std::start_lifetime_as<int>(buf) = 42; // ✅ implicitly starts int lifetime

// Typical use case: reading structured data from an I/O buffer
struct PacketHeader { uint32_t magic; uint16_t length; uint16_t checksum; };
void process(std::span<std::byte> raw) {
    auto* h = std::start_lifetime_as<PacketHeader>(raw.data());
    // Legal access to h->magic and other members (no memcpy needed)
}
```

**Difference from placement new**: `placement new` executes a constructor; `start_lifetime_as` only informs the compiler that a valid object already exists in the storage. Only applicable to implicit-lifetime types (scalars, trivial classes, etc.).

## Implicit Object Creation (C++20)

C++20's P0593R6 introduces the concept of "implicit object creation," fixing a large class of hidden UB behind `malloc`/`memcpy`.

### P0593R6 Core Idea

```cpp
// Before C++20: malloc does not create objects, direct assignment is UB
void* p = malloc(sizeof(int));
*static_cast<int*>(p) = 42;  // ❌ no int object exists

// After C++20: malloc is an "operation that implicitly creates objects"
// Automatically creates implicit-lifetime type objects in the allocated storage
*static_cast<int*>(p) = 42;  // ✅
```

### Operations That Implicitly Create Objects

Includes `operator new`, `std::malloc`/`calloc`/`realloc`, `std::allocator::allocate`, `std::memcpy`/`memmove`, `std::start_lifetime_as` (C++23). These operations implicitly create enough objects of implicit-lifetime types in the storage.

```cpp
// memcpy now implicitly creates objects in the destination
alignas(int) unsigned char buf[sizeof(int)];
std::memcpy(buf, &src, sizeof(int));
int* p = std::launder(reinterpret_cast<int*>(buf)); // ✅ *p is legal

// FlexArray-style variable-length buffer
struct Flex { std::size_t len; };
void* raw = std::malloc(sizeof(Flex) + 100);
auto* f = static_cast<Flex*>(raw);
f->len = 100;
char* data = reinterpret_cast<char*>(f + 1);
// C++20: malloc implicitly created a Flex and 100 chars → data[0]..[99] are legal
```

## Pointer and Reference Lifetime Rules

### Pointer Invalidation

```cpp
int* p;
{ int x = 42; p = &x; }
// After x is destroyed, p becomes an invalid pointer value
// Dereferencing, comparing, or incrementing is UB (only comparison with nullptr or use as delete operand is allowed)
```

### Reference Lifetime Extension

When `const T&` or `T&&` binds to a temporary object, the temporary's lifetime is extended to the end of the reference's scope. **But there are key exceptions**:

```cpp
const std::string& ref = std::string("temp"); // ✅ extended to end of ref's scope

// ❌ Three cases where extension does NOT apply:
void f(const std::string& s);           // function parameter reference — no extension
f(std::string("temp"));                 // temporary is destroyed after the full expression

const auto& r = std::pair{1,2}.first;   // binding to a sub-object — no extension
const auto& bad = get_string();          // may bind to a returned temporary — no extension
```

## Dangling References and Pointers

### Common Patterns That Produce Them

```cpp
// 1. Returning a reference/pointer to a local variable
int* make() { int x = 42; return &x; } // ❌

// 2. Holding a reference to a container element that has been destroyed
std::vector<int> v = {1,2,3};
int& ref = v[0];
v.push_back(4); // may trigger reallocation → ref dangles

// 3. Lambda capturing a reference to a local variable
auto make_lambda() {
    int x = 42;
    return [&x]() { return x; }; // ❌ x is destroyed after function returns
}

// 4. Borrowing from unique_ptr via get() then resetting
Holder h; h.data = std::make_unique<int>(42);
int& ref = *h.data;
h.data.reset(); // ref dangles
```

### Detection Tools

- **Compiler**: `-Wall -Wextra -Wdangling-reference` (GCC 13+)
- **Runtime**: AddressSanitizer (`-fsanitize=address`) detects use-after-free
- **Static analysis**: Clang's `-Wlifetime` (experimental)

## Lifetime Safety in Containers

### Iterator Invalidation

```cpp
std::vector<int> v = {1,2,3};
auto it = v.begin();
v.push_back(4); // if reallocation is triggered → it dangles

v.reserve(100);  // after pre-allocation, push_back won't reallocate
auto it2 = v.begin();
v.push_back(4);  // it2 is still valid ✅
```

**Iterator stability per container**:

| Container | Insertion | Deletion |
|-----------|-----------|----------|
| `vector` | All invalidated (on reallocation) | Erased and subsequent invalidated |
| `deque` | Middle insertion: all invalidated; ends: all invalidated | Middle deletion: all invalidated; ends: only iterators invalidated |
| `list` | Not invalidated ✅ | Only erased element invalidated |
| `unordered_map` | May rehash → all iterators invalidated, but references stable | Only erased element invalidated |

### Lifetime of span / ranges::view

```cpp
// span does not own data; the underlying data must remain alive
std::span<int> make_span() {
    std::vector<int> v = {1,2,3};
    return std::span<int>(v); // ❌ v is destroyed after return, span dangles
}
// ranges views are the same: v | std::views::filter(...) does not extend v's lifetime
```

## RAII and Lifetime Management

The core of RAII: resource lifetime is bound to object lifetime — the constructor acquires, the destructor releases. The compiler guarantees that local objects are destroyed when leaving scope (including during stack unwinding), so RAII systematically avoids leaks caused by hand-written release paths.

```cpp
// ❌ Manual management — every exit point can leak
void raw(const char* path) {
    FILE* f = fopen(path, "r"); if (!f) return;
    char* buf = (char*)malloc(4096); if (!buf) { fclose(f); return; }
    // ... more exit points
    fclose(f); free(buf);
}

// ✅ RAII — destructors guarantee release
void safe(const char* path) {
    auto f = std::unique_ptr<FILE, decltype(&fclose)>(fopen(path, "r"), &fclose);
    auto buf = std::make_unique<char[]>(4096);
    // Any complex control flow, including exceptions — destructors handle cleanup
}
```

**Destruction order guarantee**: reverse order of construction. During stack unwinding, the compiler calls the destructor for every constructed local object, so even exceptions mid-way release resources correctly. However, destructors must not throw exceptions themselves — that triggers `std::terminate`.

See [RAII and Resource Management](/topics/raii) for details.

## Smart Pointer Lifetime Semantics

### unique_ptr

Exclusive ownership, zero overhead (size equals a raw pointer with the default deleter):

```cpp
auto p = std::make_unique<Widget>(42);
auto p2 = std::move(p);  // ownership transferred, p becomes nullptr
Widget* raw = p2.get();   // borrows a raw pointer — validity of raw depends on p2's lifetime
```

### shared_ptr and Reference Counting

Shared ownership; the object is destroyed when the last `shared_ptr` is destroyed:

```cpp
auto sp1 = std::make_shared<Widget>(); // control block + Widget allocated together (one malloc)
auto sp2 = sp1;  // reference count = 2
sp1.reset();      // reference count = 1
sp2.reset();      // reference count = 0 → Widget destroyed

// ⚠️ Atomic reference counting has performance overhead — consider unique_ptr or value semantics on hot paths
```

### weak_ptr — Breaking Circular References

```cpp
struct Node {
    std::vector<std::shared_ptr<Node>> children;
    std::weak_ptr<Node> parent; // ✅ breaks parent-child cycle
};

auto root = std::make_shared<Node>();
auto child = std::make_shared<Node>();
root->children.push_back(child);
child->parent = root; // weak_ptr does not increment reference count

if (auto p = child->parent.lock()) { /* parent is alive */ }
```

### enable_shared_from_this

Used when a callback in an object needs to hold a `shared_ptr` to itself — avoids creating multiple independent reference-count control blocks:

```cpp
class Session : public std::enable_shared_from_this<Session> {
public:
    void start() {
        auto self = shared_from_this(); // ✅ reuses existing control block
        async_read([self](auto...) { self->on_data(); });
    }
};
// ⚠️ Prerequisite: the object must already be managed by a shared_ptr. Calling on a raw object is UB.
```

## constexpr Lifetime Evaluation

Object lifetime in `constexpr`/`consteval` functions follows special compile-time rules.

```cpp
constexpr int compute() {
    int a = 1, b = 2; // compile-time automatic storage duration — lifetime during compute() evaluation
    return a + b;
}

// C++20: constexpr containers (return value transferred via move)
constexpr auto make_array() {
    std::array<int,3> arr = {1,2,3};
    return arr; // compile-time move semantics
}
constexpr auto a = make_array();

// C++20: constexpr dynamic allocation — no leaks at compile time
constexpr int test() {
    auto p = std::make_unique<int>(42); // C++23 constexpr unique_ptr
    return *p;
} // Compiler guarantees p is released when constant evaluation ends

// ⚠️ constexpr cannot return a pointer to a local variable (lifetime rules prevent dangling even at compile time)
// ✅ Literals and global variables have static storage duration, safe to return from constexpr
constexpr const char* hello() { return "hello"; }
```

### consteval — Enforcing Compile-Time Evaluation

```cpp
consteval int must_compile(int n) {
    int result = 0;
    for (int i = 0; i < n; ++i) result += i;
    return result;
}
static_assert(must_compile(10) == 45);
// Object lifetime in consteval is entirely at compile time; the compiler guarantees all resources are correctly released after evaluation

// C++20: constexpr virtual functions
struct Base { constexpr virtual int value() const { return 0; } };
struct Derived : Base { constexpr int value() const override { return 42; } };
constexpr int v = Derived{}.value(); // ✅
```

## Best Practices Checklist

```
1. Prefer value semantics and RAII containers; avoid manual new/delete
2. Return local variables by value (do not return references/pointers)
3. Do not reuse old iterators after push_back/insert
4. Break shared_ptr cycles with weak_ptr
5. Use enable_shared_from_this instead of "two independent shared_ptrs managing the same raw pointer"
6. span/string_view/view are non-owning — the underlying data must remain alive
7. After placement new, the destructor must be explicitly called (unless using start_lifetime_as with an implicit-lifetime type)
8. Enable -Wall -Wextra -Wdangling-reference; run ASan/UBSan in CI
9. Use constinit instead of dynamic initialization for namespace-scope non-constexpr variables
10. Avoid reinterpret_cast to unrelated types — violating strict aliasing is UB
```

## Further Reading

- [RAII and Resource Management](/topics/raii) — Construction/destruction, smart pointers, scope guards
- [Memory Model and Concurrency](/topics/memory-model) — Object storage and multi-thread visibility
- [Compile-Time Computation](/topics/compile-time-computation) — constexpr/consteval/constinit
- [Performance Optimization](/topics/performance) — Value semantics, move semantics performance impact
- [C++ Core Guidelines Lifetime](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#pro-lifetime-safety) — Static analysis rules
- P0593R6 — Implicit creation of objects for low-level object manipulation
- P0137R1 — Core Issue 1776: Replacement of class objects containing reference members
