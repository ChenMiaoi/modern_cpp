---
title: Value Categories Deep Dive
topic: topics
feature: value-categories-deep-dive
status_checked_at: 2026-06-02
standard: C++17
---

# Value Categories Deep Dive

## Historical Evolution: From C to C++17

The concept of value categories predates C++ itself. Understanding its evolutionary history helps explain why the current classification system is designed the way it is.

**The C era**: Expressions were simply divided into "lvalues" and "rvalues." An lvalue was an expression that could appear on the **left** side of an assignment operator — essentially an object with a memory address. An rvalue was a one-off value — a literal, the result of an arithmetic operation. This definition was clear and self-consistent in C.

**C++98's disruption**: C++ introduced `const`, making "can appear on the left side of an assignment" no longer a valid distinguishing criterion — `x` in `const int x = 42;` is clearly an lvalue, but `x = 1` won't compile. The standard therefore redefined lvalue as "an expression with identity" — you can take its address (`&x` is legal).

**C++11's expansion**: The introduction of move semantics required distinguishing two kinds of rvalues — one is a pure value (the literal `42`), and the other is a value "about to be moved" (the result of `std::move(x)`). The latter, though nameless, is bound to an object that has identity. The standard therefore introduced the concept of xvalue (expiring value).

**C++17's systematization**: The establishment of the temporary materialization conversion rules completed the final formalization of the value category system. A prvalue is no longer a "temporary object" — it is an **initializer** that is materialized into a temporary object only when needed.

```
Evolution timeline:

C:     lvalue / rvalue                    (2 categories)
C++98: lvalue / non-lvalue               (standard draft terminology)
C++11: lvalue / xvalue / prvalue          (3 basic categories + 2 composites)
C++17: Same as C++11, but prvalue semantics fundamentally changed   (temporary materialization)

Composite categories:
  glvalue = lvalue + xvalue    (generalized lvalue: expression with identity)
  rvalue  = xvalue + prvalue   (right value: expression that can bind to T&&)
```

## Complete Taxonomy

C++17 value categories are determined by the combination of two orthogonal properties:

| Category | Has Identity | May Be Moved From |
|------|:---:|:---:|
| lvalue | ✓ | ✗ |
| xvalue | ✓ | ✓ |
| prvalue | ✗ | ✓ |
| glvalue | ✓ | *(either)* |
| rvalue | *(either)* | ✓ |

**Taxonomy tree**:

```
                        expression
                    ┌───────┴───────┐
                glvalue            rvalue
              ┌───┴───┐         ┌───┴───┐
          lvalue   xvalue    xvalue   prvalue
                                   ↑
                            Intersection of both sets
```

Intuitive understanding of the classification rules:

- **lvalue**: You can take its address (`&expr` is legal); it represents a **persistent object**. Typical examples: variable names, dereference expression `*p`, pre-increment `++i`, string literal `"hello"`, function calls returning an lvalue reference.
- **prvalue**: It is a pure **computation result** without an independent memory location. Typical examples: the literal `42`, arithmetic expression `a + b`, function calls returning a non-reference type, lambda expressions, `requires` expressions.
- **xvalue**: It represents an **object whose lifetime is about to end**, from which you can "steal" resources. Typical examples: the result of `std::move(x)`, function calls returning an rvalue reference `std::move(x)`, member access expression `prvalue.member` (after C++17 materialization).

```cpp
// Classification exercises:
int x = 42;
int& lr = x;              // x is lvalue (has a name, addressable)
int&& rr = 42;            // 42 is prvalue (pure literal)
int&& mr = std::move(x);  // std::move(x) is xvalue

std::string s = "hi";
std::string&& t = std::move(s);   // std::move(s) is xvalue
std::string   u = std::move(s);   // Calls move constructor, xvalue binds to &&
                                   // Note: s itself is still an lvalue, its value is in a "valid but unspecified" state

// Counterintuitive examples:
int a = 1, b = 2;
a + b;         // prvalue — pure computation result
++a;           // lvalue — pre-increment returns lvalue reference
a++;           // prvalue — post-increment returns a copy of the old value

// Conditional expressions:
true ? a : b;  // lvalue — result is lvalue when both operands are lvalues
true ? a : 1;  // prvalue — implicit conversion when operands are not both lvalues
```

## Temporary Materialization — C++17's Key Change

C++17 introduced the temporary materialization conversion, which is the **core concept** for understanding modern C++ value category semantics.

**Core rule**: A prvalue is not an object — it is a **way** of producing objects (an initializer). When a prvalue needs to be used as a glvalue (binding to a reference, taking a member, calling a method), the compiler performs temporary materialization: it creates a temporary object and initializes it with the prvalue. The result of the conversion is an xvalue.

```cpp
struct Widget {
    int id;
    std::string name;
    Widget(int i, std::string n) : id(i), name(std::move(n)) {}
};

Widget make_widget() { return {1, "temp"}; }  // Returns prvalue

// C++17 semantics:
Widget w = make_widget();
// 1. make_widget() is a prvalue — it is not an object, but a way to initialize w
// 2. Widget is constructed directly in w's memory location — zero copies, zero moves
// This is guaranteed copy elision

// Scenarios requiring materialization:
Widget&& ref = make_widget();
// make_widget() needs to bind to a reference — references need an object to bind to
// → Triggers temporary materialization: creates a temporary Widget in memory (xvalue)
// → ref binds to that temporary object
// → The temporary object's lifetime is extended to ref's lifetime

const Widget& cref = make_widget();
// Same as above: materialization needed, then const reference extends the temporary's lifetime

auto&& uref = make_widget();
// Same as above: materialization + universal reference binding

// Member access triggers materialization:
int id = make_widget().id;
// make_widget() is a prvalue, accessing .id requires an object
// → Materializes into a temporary Widget, then takes its id member
// → The temporary object is destroyed at the end of the full expression
```

Specific timing of materialization:

```cpp
struct S { int x; };

S f() { return {42}; }

// Does not trigger materialization — prvalue directly initializes the target object
S s1 = f();

// Triggers materialization — binding to a reference
S&& s2 = f();

// Triggers materialization — member access
int v = f().x;

// Does not trigger materialization — direct initialization (C++17 guarantee)
S s3 = S{S{f()}};   // Layer-by-layer direct initialization, no temporaries
```

## Temporary Object Lifetime Extension Rules

Temporary objects are normally destroyed at the end of a full-expression, but C++ has two special extension rules:

**Rule 1: When a const lvalue reference or rvalue reference binds to a temporary object, the temporary object's lifetime is extended to the lifetime of the reference.**

```cpp
std::string make() { return "hello"; }

void example() {
    const std::string& r1 = make();       // Temporary string's lifetime extends to r1's scope
    std::string&& r2 = make();            // Same as above

    // ⚠️ Pitfall: references in function parameters do not enjoy lifetime extension
    // Because the temporary's lifetime is only extended to the "reference it binds to"
    // A function parameter's lifetime ≠ the internal reference's lifetime
}

void takes_ref(const std::string& s);    // s's lifetime is managed by the caller
void trap() {
    takes_ref(make());  // make()'s temporary is destroyed at the end of takes_ref's call expression
    // Even if takes_ref internally saved a const reference, it's already dangling
}
```

**Rule 2: Extension is conditional — it only applies on direct binding.**

```cpp
struct Holder {
    const std::string& ref;
    Holder(const std::string& r) : ref(r) {}  // ref binds to temporary
};

void pitfall() {
    Holder h(make());   // make()'s temporary string binds to parameter r
                        // Parameter r's lifetime ends after the constructor call returns
                        // But h.ref points to the temporary string that's already destroyed!
                        // h.ref is a dangling reference

    // Correct approach:
    std::string s = make();
    Holder h2(s);       // h2.ref binds to local variable s — safe
}
```

**Propagation scope of lifetime extension**:

```cpp
struct Aggregate {
    std::string s;
    int n;
};

void lifetime_extension_examples() {
    // Direct binding — extended
    const Aggregate& a = Aggregate{42, "hi"};  // ✅ Extended
    const std::string& s = Aggregate{42, "hi"}.s; // ✅ Extended (subobject)

    // ⚠️ Before C++17 there was a defect: the rules for extending when binding to subobjects were inconsistent
    // But C++17's materialization rules unified the semantics

    // Through function return — not extended
    // (The returned temporary is created in the return statement, not directly bound)
    auto make_pair = []() -> std::pair<int, int> { return {1, 2}; };
    const auto& [a1, a2] = make_pair();  // ✅ Structured binding extends the temporary
    // But note: whether structured binding extends temporaries depends on the implementation
    // P0963 (C++26 accepted) will standardize the lifetime extension behavior of structured bindings
}
```

## Move Semantics and Value Category Interaction

The core of move semantics depends on value categories: only rvalues (prvalues and xvalues) can trigger move construction/assignment. The entirety of `std::move`'s function is to convert an lvalue to an xvalue — it does not perform any actual resource movement.

```cpp
// std::move's implementation (simplified)
template <typename T>
constexpr std::remove_reference_t<T>&& move(T&& t) noexcept {
    return static_cast<std::remove_reference_t<T>&&>(t);
}
// It only performs a type cast — generates no code

// Value categories in overload resolution:
class Buffer {
    char* data_;
    std::size_t size_;
public:
    Buffer(const Buffer& o)        // 1. Copy constructor — parameter is const lvalue&
        : data_(new char[o.size_]), size_(o.size_) {
        std::memcpy(data_, o.data_, size_);
    }
    Buffer(Buffer&& o) noexcept    // 2. Move constructor — parameter is rvalue&&
        : data_(o.data_), size_(o.size_) {
        o.data_ = nullptr;
        o.size_ = 0;
    }
};

Buffer a(100);
Buffer b(a);              // Calls 1 — a is lvalue
Buffer c(std::move(a));   // Calls 2 — std::move(a) is xvalue
Buffer d(Buffer(50));     // Calls 2 — Buffer(50) is prvalue

// ⚠️ State of a moved-from object
// After std::move(a), a is still in a "valid but unspecified" state
// The only safe operations you can perform on a are: assignment, destruction
// Do not rely on a's specific value
```

**Value category overload pattern for move constructors**:

```cpp
// Universal reference + forwarding — perfectly preserves value categories
template <typename T>
void push_back(T&& value) {
    // When T is U&, value is an lvalue reference → takes the copy path
    // When T is U, value is an rvalue reference → takes the move path
    // But in practice, a container's push_back is typically two overloads:
}

// Typical container implementation (what std::vector does)
class Container {
    void push_back(const T& value);  // Copy version — parameter is lvalue
    void push_back(T&& value);       // Move version — parameter is rvalue
};

// For derived-to-base moves, be aware of the slicing problem:
struct Base {
    virtual ~Base() = default;
    Base() = default;
    Base(Base&&) = default;
};
struct Derived : Base {
    std::string data;
};

Derived d;
Base b1(d);             // Copy — slicing, Derived part lost
Base b2(std::move(d));  // Move — still slicing! What's moved is the Base subobject
// Correct approach: use pointers / smart pointers
std::unique_ptr<Base> b3 = std::make_unique<Derived>(std::move(d));
```

## Perfect Forwarding and Value Category Preservation

`std::forward` is the core tool for preserving value categories. It works together with forwarding references (universal references) to ensure that value categories are not lost during transmission.

```cpp
// Problem: value categories are lost without forward
template <typename T>
void wrapper_bad(T arg) {
    target(arg);  // arg is lvalue (named parameters are always lvalues)
                  // Even if the caller passes an rvalue, it becomes lvalue by the time it reaches target
                  // → Always takes the copy path
}

// Correct: use universal reference + forward
template <typename T>
void wrapper_good(T&& arg) {
    target(std::forward<T>(arg));
    // When T is U& (caller passed an lvalue): forward returns an lvalue reference
    // When T is U (caller passed an rvalue): forward returns an rvalue reference
}
```

**The internal mechanism of `std::forward`**:

```cpp
// Simplified implementation of std::forward
template <typename T>
T&& forward(std::remove_reference_t<T>& t) noexcept {
    return static_cast<T&&>(t);
}

// Why this works:
// 1. When T = int& (deduced when an lvalue is passed):
//    Signature becomes int& && forward(int&) → reference collapsing yields int& forward(int&)
//    Returns lvalue reference ✅
//
// 2. When T = int (deduced when an rvalue is passed):
//    Signature becomes int&& forward(int&)
//    Returns rvalue reference ✅
```

**Pitfalls and limitations of forwarding**:

```cpp
// Pitfall 1: forwarding the same argument multiple times — undefined behavior
template <typename T>
void bad(T&& arg) {
    target(std::forward<T>(arg));
    other(std::forward<T>(arg));  // ⚠️ UB: arg may have already been moved
}

// Pitfall 2: braced initializer lists cannot be forwarded
template <typename T>
void wrapper(T&& arg) { target(std::forward<T>(arg)); }

// wrapper({1, 2, 3});  // ❌ Compilation failure: T cannot be deduced
// Must use std::initializer_list to pass explicitly

// Pitfall 3: bit-fields cannot be forwarded
struct Flags {
    unsigned int mode : 3;
};
Flags f;
// wrapper(f.mode);  // ❌ Compilation failure: bit-fields cannot bind to references

// Pitfall 4: NULL pointers and string literals
template <typename T>
void func(T&& arg) { /* ... */ }
func(NULL);       // ⚠️ T deduced as int (not a pointer type)
func(nullptr);    // ✅ T deduced as std::nullptr_t
```

**Parameter pack forwarding pattern**:

```cpp
// Variadic templates + perfect forwarding — universal factory pattern
template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    // Each argument's value category is precisely preserved
}

// Interaction of perfect forwarding and copy elision
struct Widget {
    Widget(int, int) {}
    Widget(const Widget&) { std::puts("copy"); }
    Widget(Widget&&) { std::puts("move"); }
};

template <typename... Args>
Widget create(Args&&... args) {
    return Widget(std::forward<Args>(args)...);
}

// create(1, 2)           → No output: prvalue direct initialization, copy elision
// Widget w; create(w)    → Outputs "copy": lvalue takes copy path
// create(std::move(w))   → Outputs "move": xvalue takes move path
```

## decltype and Value Categories

`decltype` is the only language mechanism that can precisely capture an expression's value category. Its deduction rules differ from `auto`; understanding these rules is essential for writing generic code.

**Core rules**:

```cpp
// Rule 1: decltype(entity) → declared type
int x = 42;
decltype(x) a = 10;          // int — x's type is int

// Rule 2: decltype(expression) → exact mapping of the expression's value category
int&  lr = x;
int&& rr = 42;
decltype(lr) b = x;           // int& — lvalue expression → T&
decltype(rr) c = 42;          // int&& — lvalue expression → T&&
                               // Note: rr is a variable name, it's an lvalue! Even though its type is int&&

// Rule 3: decltype(prvalue) → T (non-reference)
decltype(42) d = 0;           // int — literal is prvalue → T
decltype(x + 0) e = 0;        // int — arithmetic result is prvalue → T

// Rule 4: decltype(lvalue of type T) → T&
int arr[3];
decltype(arr) f = {1, 2, 3};  // int(&)[3] — array name is lvalue → reference

// Rule 5: function names
void foo();
decltype(foo) g = foo;        // void() — function name is lvalue, but function types don't add reference
```

**The power of `decltype(auto)`**:

```cpp
// decltype(auto) preserves value categories — perfect return type deduction
template <typename Container, typename Index>
decltype(auto) access(Container&& c, Index i) {
    return std::forward<Container>(c)[i];
    // If c[i] returns int&, the return type is int&
    // If c[i] returns int, the return type is int
}

std::vector<int> v = {1, 2, 3};
decltype(auto) val = access(v, 0);  // val is int&

// Compared with auto:
auto x = v[0];        // x is int (copy)
auto& y = v[0];       // y is int&
auto&& z = v[0];      // z is int& (lvalue bound to && collapses to &)

// Perfect forwarding lambda
auto wrapper = [](auto&& f, auto&&... args) -> decltype(auto) {
    return std::forward<decltype(f)>(f)(std::forward<decltype(args)>(args)...);
};
```

**decltype's parenthesization trap**:

```cpp
int x = 42;
decltype(x)   a;  // int   — entity (variable name), directly takes declared type
decltype((x)) b;  // int&  — expression ((x) is an lvalue expression), adds reference
                   // This is the most common decltype trap

// Practical impact:
decltype((x)) c = x;  // c is int&, must be initialized
// decltype(x)  d;      // OK: int can be default-initialized

// auto is different — auto always strips references (unless using auto& or auto&&):
auto e = x;        // int
auto& f = x;       // int&
decltype(auto) g = (x);  // int& (decltype rule: (x) is an lvalue expression)
```

## Implicit Conversions Between Value Categories

There are specific implicit conversion paths between value categories. Understanding these conversions is essential for understanding overload resolution and template deduction.

```
Conversion directions:

  lvalue ──────────────────────→ glvalue    (lvalue is already glvalue, no conversion needed)
  xvalue ──────────────────────→ glvalue    (xvalue is already glvalue, no conversion needed)
  xvalue ──────────────────────→ rvalue     (xvalue is already rvalue, no conversion needed)
  prvalue ─────────────────────→ rvalue     (prvalue is already rvalue, no conversion needed)

  lvalue ──── (bind to T&) ────→ lvalue     (reference binding, no new object created)
  lvalue ──── (bind to const T&) ─→ lvalue  (const reference binding, can extend lifetime)
  prvalue ─── (materialization) ──→ xvalue  (C++17: temporary materialization conversion)
  xvalue ──── (bind to T&&) ───→ rvalue     (rvalue reference binding)

Implicit conversions in the type system:
  prvalue → lvalue: Impossible! prvalues have no address
  lvalue → prvalue: Via copy initialization (requires copy/move constructor)
  xvalue → lvalue: Impossible! xvalues are about to be destroyed
```

**lvalue to prvalue conversion — value "decay"**:

```cpp
int x = 42;
int y = x;        // x (lvalue) → prvalue: copy initialization
                   // What actually happens: reads the value of x, uses it to initialize y

std::string s1 = "hello";
std::string s2 = s1;  // s1 (lvalue) → needs to call the copy constructor
// This isn't simple value decay, but a call to what may be a very expensive function

// This is why move semantics are so important — they provide an "explicit authorization" path from lvalue to rvalue:
std::string s3 = std::move(s1);  // s1 → xvalue → calls move constructor
```

**Special decay rules for arrays and function names**:

```cpp
// Array-to-pointer decay
int arr[3] = {1, 2, 3};
int* p = arr;     // arr (lvalue, type int[3]) decays to int*

// But the following do not decay:
decltype(arr) ref = arr;   // int(&)[3] — decltype doesn't decay
sizeof(arr);               // 12 — sizeof doesn't decay
auto& r = arr;             // int(&)[3] — binding to a reference doesn't decay

// Function-to-pointer decay
void foo(int);
auto fp = foo;     // void(*)(int) — function name decays to function pointer
auto& fr = foo;    // void(&)(int) — binding to a reference doesn't decay
```

## Function Return Value Categories

A function's return type determines the value category of its call expression, which has direct implications for both performance and correctness.

```cpp
// 1. Returning non-reference type → prvalue
std::string make_string() {
    return "hello";   // prvalue — C++17 allows direct construction at the call site
}

// 2. Returning lvalue reference → lvalue
std::string& get_ref(std::string& s) {
    return s;   // lvalue — can take address, can assign
}
get_ref(s) = "modified";  // ✅ Can appear on the left side of an assignment

// 3. Returning rvalue reference → xvalue
std::string&& get_rref(std::string& s) {
    return std::move(s);   // xvalue — can trigger a move
}
// ⚠️ Returning an rvalue reference to a local variable is UB:
std::string&& bad() {
    std::string s = "local";
    return std::move(s);  // ⚠️ UB: s is destroyed after the function returns
}

// 4. Returning const reference → lvalue (but not assignable)
const std::string& get_cref(const std::string& s) { return s; }

// 5. Value category deduction when returning auto
auto f1() { return 42; }           // Returns int, prvalue
auto& f2() { static int x = 42; return x; }  // Returns int&, lvalue
auto&& f3() { return 42; }         // Returns int&&, xvalue
decltype(auto) f4() { return (42); } // ⚠️ Compilation failure: (42) is prvalue, cannot bind to int&&
                                     // But some compilers accept it: decltype(auto) deduces as int
```

**Common patterns for return values**:

```cpp
// Pattern 1: Return by value — prefer this
std::vector<int> make_vec() {
    std::vector<int> v = {1, 2, 3};
    return v;   // NRVO or move — zero copies
}

// ⚠️ Pattern 2: Don't use std::move on return values
std::string bad_return() {
    std::string s = "hello";
    return std::move(s);   // ❌ Prevents NRVO! Worse than return s
}
// Reason: return std::move(s) returns an xvalue, compiler must use the move constructor
//         But return s returns a prvalue, compiler can directly construct at the target location (guaranteed elision)

// But using std::move on member variables is correct:
struct Holder {
    std::string data;
    std::string release() {
        return std::move(data);   // ✅ data is a member, NRVO won't apply
    }
};

// Pattern 3: NRVO may be disabled with conditional returns
std::string conditional(bool flag) {
    std::string a = "hello";
    std::string b = "world";
    if (flag) return a;   // Two different named variables — NRVO may not apply
    return b;              // The compiler will use a move (C++11+ requires or implicitly moves)
}
```

## Operator Overloading and Value Categories

The return type design of operators directly affects the ability and semantic correctness of chained calls.

```cpp
// Assignment operators return a reference to *this — supports chained assignment
class MyString {
    std::string data_;
public:
    // Assignment operator: returns lvalue reference
    MyString& operator=(const MyString& other) {
        data_ = other.data_;
        return *this;   // lvalue — supports a = b = c
    }
    MyString& operator=(MyString&& other) noexcept {
        data_ = std::move(other.data_);
        return *this;   // Still returns lvalue reference
    }

    // Subscript operator: typically returns a reference
    char& operator[](std::size_t i) { return data_[i]; }       // Non-const version — writable
    const char& operator[](std::size_t i) const { return data_[i]; } // Const version — read-only

    // Stream operator: returns lvalue reference to support chained output
    friend std::ostream& operator<<(std::ostream& os, const MyString& s) {
        return os << s.data_;  // Returns reference to os, supports cout << a << b
    }

    // Arithmetic operators: typically return prvalue (new value)
    MyString operator+(const MyString& rhs) const {
        MyString result = *this;
        result.data_ += rhs.data_;
        return result;   // prvalue — should not modify operands
    }

    // Pre-increment returns lvalue, post-increment returns prvalue
    MyString& operator++() {    // Pre-increment ++i
        // Modify self
        return *this;           // lvalue
    }
    MyString operator++(int) {  // Post-increment i++ (int is a placeholder to distinguish pre/post)
        MyString old = *this;
        // Modify self
        return old;             // prvalue (copy of the old value)
    }
};
```

**The value category of implicit `this` — reference qualifiers (C++11)**:

```cpp
class Data {
    std::vector<int> items_;
public:
    // C++11 reference qualifiers: control the value category of *this
    std::vector<int>& get() & { return items_; }           // Called when *this is lvalue
    std::vector<int>  get() && { return std::move(items_); } // Called when *this is rvalue

    // Commonly seen in optional::value():
    // T& value() &;
    // const T& value() const&;
    // T&& value() &&;
    // const T&& value() const&&;
};

Data d;
auto& v1 = d.get();               // Calls & version — v1 is a reference
auto v2 = Data{}.get();            // Calls && version — moves ownership
```

## Reference Binding Rules

Reference binding rules determine what kinds of expressions can bind to what kinds of references. This is the most direct application scenario of the value category system.

**Binding priority (from most restrictive to least)**:

```cpp
int x = 42;
const int cx = 42;

// 1. Non-const lvalue reference T& — only binds to non-const lvalues
int& r1 = x;          // ✅
// int& r2 = cx;      // ❌ Cannot discard const
// int& r3 = 42;      // ❌ Cannot bind prvalue
// int& r4 = std::move(x); // ❌ Cannot bind xvalue

// 2. Const lvalue reference const T& — binds to anything
const int& r5 = x;              // ✅ lvalue
const int& r6 = cx;             // ✅ const lvalue
const int& r7 = 42;             // ✅ prvalue (materialization + lifetime extension)
const int& r8 = std::move(x);   // ✅ xvalue (lifetime extension)

// 3. Rvalue reference T&& — binds to rvalues (prvalues and xvalues)
// int&& r9 = x;                 // ❌ Cannot bind lvalue
int&& r10 = 42;                  // ✅ prvalue
int&& r11 = std::move(x);       // ✅ xvalue
// int&& r12 = cx;               // ❌ Cannot bind const lvalue

// 4. Const rvalue reference const T&& — binds to const rvalues
const int&& r13 = 42;           // ✅ But rarely used
// Primary use case: rvalue reference overloads that prevent accidental moves
```

**Reference binding for user-defined types**:

```cpp
struct Base {};
struct Derived : Base {};

Derived d;

// Derived-to-base reference binding
Base& rb = d;              // ✅ lvalue binding (implicit derived-to-base conversion)
const Base& crb = d;       // ✅
Base&& rrb = Derived{};    // ✅ xvalue binding

// ⚠️ Types must match exactly (reference binding does not create new objects)
// int& ri = double_val;   // ❌ Requires creating a temporary int — cannot bind to non-const reference
const int& cri = 3.14;     // ✅ const reference can bind to the result of a conversion
// The compiler creates a temporary int(3.14), then binds cri to that temporary
// Lifetime extension rules apply

// ⚠️ Lifetime extension trap when base class reference binds to derived class rvalue
struct Base {
    virtual ~Base() = default;
};
struct Derived : Base {
    std::string payload;
};
const Base& ref = Derived{};  // Temporary Derived object's lifetime is extended
// ✅ Safe — the entire Derived object is what gets extended
```

## Copy Elision

Copy elision is where the value category system has the most direct impact on performance. C++17 divides it into guaranteed elision and optional elision.

**C++17 guaranteed copy elision**:

```cpp
struct Widget {
    Widget() { std::puts("default"); }
    Widget(const Widget&) { std::puts("copy"); }
    Widget(Widget&&) { std::puts("move"); }
};

// Scenario 1: prvalue direct initialization
Widget w1 = Widget();  // Outputs "default" — no copy, no move
// C++17: Widget() is a prvalue, constructed directly in w1's location

// Scenario 2: return prvalue
Widget make() {
    return Widget();    // Outputs "default" — constructed directly at the call site
}

// Scenario 3: nested prvalue initialization
Widget w2 = Widget(Widget(Widget()));
// Outputs "default" — only once, all intermediate prvalues are elided

// Scenario 4: function argument is also a prvalue
void take(Widget w) {}
take(Widget());  // Widget is constructed directly at take's parameter location — zero copies
```

**NRVO (Named Return Value Optimization) — still optional**:

```cpp
Widget make_named() {
    Widget w;
    return w;   // NRVO may elide the copy/move, but it's not guaranteed
}

// C++17 standard behavior:
// 1. If the compiler performs NRVO → w is constructed directly at the return location (zero copies)
// 2. If the compiler does not perform NRVO → w is implicitly moved (guaranteed since C++11)
//    But Widget must be movable (if the move constructor is deleted, copy is used)

// NRVO limitations — NRVO does not apply in the following cases:
Widget make_two(bool flag) {
    Widget a, b;
    if (flag) return a;  // Two different named variables — NRVO may not apply
    return b;
}

// Returning function parameters — NRVO also doesn't apply:
Widget pass_through(Widget w) {
    return w;  // Parameter is not a local variable — NRVO doesn't apply, but implicit move occurs
}
```

**Before and after C++17 comparison**:

```
Before C++17 (simplified view):
  Widget w = Widget();
  1. Widget() creates temporary object T1
  2. T1 copy/move constructs into w (compiler optimization may elide)
  3. T1 is destroyed

After C++17:
  Widget w = Widget();
  1. Widget() is constructed directly in w's location
  2. Done. No temporary object, no optimization needed.

Impact: Code that relied on copy constructors having side effects behaves differently before and after C++17.
        If your copy constructor performs observable operations (printing, counting, etc.),
        C++17's guaranteed elision changes the behavior.
```

## Common Pitfalls and Lessons

**Pitfall 1: Continuing to use a moved-from object**:

```cpp
std::string s = "hello";
std::string t = std::move(s);
// s is now in a "valid but unspecified" state
std::cout << s.size();   // ⚠️ Result is unpredictable (may be 0, may be 5)
std::cout << s;           // ⚠️ May output an empty string, may output something else
s.clear();                // ✅ Safe operation
s = "new value";          // ✅ Assignment is safe
```

**Pitfall 2: Dangling references and return value categories**:

```cpp
// Returning a reference to a local variable — UB
std::string& bad() {
    std::string s = "hello";
    return s;   // ❌ Compiler usually warns
}

// Returning an rvalue reference to a local variable — also UB
std::string&& also_bad() {
    std::string s = "hello";
    return std::move(s);   // ❌ Still UB
}

// Correct: return by value
std::string good() {
    std::string s = "hello";
    return s;   // ✅ Move or NRVO
}
```

**Pitfall 3: `auto` deduction loses value category information**:

```cpp
std::string get();

auto a = get();           // std::string — prvalue is copied (before C++17) / direct initialization
auto& b = get();          // ❌ Compilation error: cannot bind prvalue to non-const reference
const auto& c = get();    // ✅ const reference extends temporary's lifetime
auto&& d = get();         // ✅ Universal reference, binds to materialized temporary
decltype(auto) e = get(); // std::string — prvalue → non-reference type
```

**Pitfall 4: List initialization and value categories**:

```cpp
std::vector<int> v1{1, 2, 3};  // Initializer list construction
std::vector<int> v2 = {1, 2, 3};  // Same (copy list initialization)

// But note:
auto v3 = {1, 2, 3};  // std::initializer_list<int>, not vector!
auto v4{1, 2, 3};      // C++17: std::initializer_list<int>
auto v5 = 1;           // int

// The special nature of initializer_list:
// Elements of an initializer_list are always const — cannot move from them
std::vector<std::string> v6 = {"hello", "world"};
// Each string literal is copy-constructed into the vector — cannot move
// C++26's std::inplace_vector and P2243 may improve this
```

**Pitfall 5: Perfect forwarding compilation error messages**:

```cpp
template <typename T>
void call_target(T&& arg) {
    target(std::forward<T>(arg));
}

// When target has no matching overload, the error message will be very verbose
// Because the compiler reports errors in the context of template instantiation
// C++20 Concepts can improve this:
template <typename T>
concept Callable = requires(T t) { target(std::forward<T>(t)); };

template <Callable T>
void call_target(T&& arg) {
    target(std::forward<T>(arg));
}
```

**Pitfall 6: Value category of conditional expressions**:

```cpp
int a = 1, b = 2;

// The ternary operator's value category depends on both branches
auto&& r1 = (true ? a : b);    // int& — both are lvalues → lvalue
auto&& r2 = (true ? a : 42);   // int — one is prvalue → prvalue (performs type conversion)
auto&& r3 = (true ? a : std::move(b)); // int — lvalue + xvalue → prvalue

// ⚠️ This means conditional expressions can unexpectedly cause copies:
std::string x = "hello", y = "world";
std::string z = true ? x : std::move(y);
// Result: z is a copy of x (not a move! Because the conditional expression as a whole is a prvalue,
//         but x needs to be copied into the result)
```

**Pitfall 7: Combined trap of reference members and value categories**:

```cpp
struct Trap {
    const std::string& ref;
};

Trap make_trap() {
    std::string s = "temp";
    return Trap{s};   // ⚠️ When copying Trap, ref may point to a destroyed temporary
}

Trap t = make_trap();
// The Trap returned by make_trap() contains a dangling reference
// Even if the compiler performs NRVO, s is destroyed after make_trap() returns
```

## Further Reading

- [Value Category Terminology Explained](/topics/cpp-jargon/value-categories) — Concise definitions and quick reference for the five value categories
- [Move Semantics and Performance Optimization](/topics/performance) — Practical guide to move semantics, SSO, and cache-friendly design
- [RAII and Resource Management](/topics/raii) — Rule of 5, smart pointers, scope guards
- [Template Metaprogramming](/topics/template-metaprogramming) — SFINAE, Concepts, and perfect forwarding template techniques
- C++ Standard [\[expr.type\]](https://eel.is/c++draft/expr.type) — Formal definition of value categories
- C++ Standard [\[class.temporary\]](https://eel.is/c++draft/class.temporary) — Rules for creation and destruction of temporary objects
- cppreference [Value categories](https://en.cppreference.com/w/cpp/language/value_category) — Authoritative reference
- Nicolai Josuttis, *C++ Move Semantics* — Comprehensive guide to move semantics
- Arthur O'Dwyer, ["A Brief Introduction to `std::move` and `std::forward`"](https://quuxplusone.github.io/blog/2022/01/23/move-forward/) — Precise analysis of common misconceptions about move/forward
