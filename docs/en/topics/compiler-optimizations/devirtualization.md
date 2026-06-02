---
title: "Devirtualization"
topic: topics
feature: compiler-opt-devirtualization
standard: C++
status_checked_at: 2026-06-02
---

# Devirtualization

> Virtual function calls are the cost of C++ polymorphism: an indirect jump (vtable lookup), plus the side effect of preventing inlining. Devirtualization replaces virtual calls with direct calls, eliminating indirect jump overhead and unlocking inlining. This is a key piece of "zero-overhead abstraction" in C++ performance optimization.

---

## The Cost of Virtual Calls

```
Virtual Call vs Direct Call:

  Direct Call:                   Virtual Call:
  ┌──────────────────┐         ┌──────────────────────────────┐
  │ call _ZN5Base7 │         │ load vptr from obj           │
  │   computeEi     │         │ load fn_ptr from vtable[slot]│
  │                  │         │ call fn_ptr                  │
  │ Overhead:       │         │                              │
  │ · 1 call instr  │         │ Overhead:                    │
  │ · Fixed target  │         │ · 2 loads (memory access)    │
  │ · Easy branch   │         │ · 1 call instruction         │
  │   prediction    │         │ · Indirect jump (hard branch │
  └──────────────────┘         │   prediction)                │
                               │ · Blocks inlining            │
                               └──────────────────────────────┘

  Measured difference:
  · Direct call: ~1-2 cycles (with branch prediction hit)
  · Virtual call: ~5-15 cycles (including vtable load + indirect jump + I-cache miss)
  · Virtual calls cannot be inlined → miss constant propagation, dead code elimination, etc.
```

---

## Automatic Devirtualization by the Compiler

### Type Inference Devirtualization

When the compiler can statically infer the dynamic type of an object, it can directly replace the virtual call:

```cpp
class Base {
public:
    virtual int compute(int x) { return x; }
};

class Derived : public Base {
    int compute(int x) override { return x * 2; }
};

void direct_call() {
    Derived d;
    d.compute(42);  // The compiler knows d's type must be Derived
                     // → Directly calls Derived::compute, no vtable needed
}
```

```bash
# View devirtualization effect
clang++ -O2 -S -masm=intel test.cpp -o test.s
# Search assembly: if call _ZN7Derived7computeEi → devirtualization succeeded
#                  if call rax (indirect jump) → not devirtualized

# GCC: view devirtualization decisions
g++ -O2 -fdump-tree-fre test.cpp
# FRE is the pass in GCC that performs early devirtualization
```

### Devirtualization with final Class/Method

```cpp
class Widget final {  // final class: cannot have subclasses
public:
    virtual int process(int x) { return x + 1; }
};

void call(Widget& w) {
    w.process(42);  // Widget is final → compiler knows the dynamic type
                    // → directly calls Widget::process
}

class Engine {
public:
    virtual int run(int x) final { return x; }  // final method
    // Even with subclasses, they cannot override run()
    // → For references of type Engine, run() can be devirtualized
};
```

### Direct Effect of the final Keyword

```cpp
class Base {
public:
    virtual int f() { return 0; }
};

class Derived final : public Base {
    int f() override { return 1; }
};

// When Derived is final, the compiler knows:
// An object pointed to by Derived* must be of type Derived (cannot have subclasses)
// → Can safely replace the virtual call with a direct call
```

---

## Speculative Devirtualization

When the compiler learns from PGO data that a virtual call **almost always** targets the same function, it inserts a type check + direct call + fallback to indirect call:

```
Before speculative devirtualization:
  call obj->process(x)          ; always an indirect call

After speculative devirtualization:
  if (typeid(obj) == typeid(Derived)) {    ; type guard
      // Hot path: direct call (can be inlined)
      return Derived::process(obj, x);      ; direct call
  } else {
      // Cold path: keep the indirect call
      return obj->process(x);               ; indirect call
  }

  Approximated in C++ source:
  if (auto* d = dynamic_cast<Derived*>(&obj)) {
      return d->process(x);   // Takes this path 95% of the time
  } else {
      return obj.process(x);  // Takes this path 5% of the time
  }
```

```bash
# GCC speculative devirtualization
g++ -O2 -fdevirtualize-speculatively test.cpp
# Enabled by default at -O2 and above

# LLVM speculative devirtualization (requires PGO data)
clang++ -O2 -fprofile-use=default.profdata test.cpp
# Compiler decides whether to speculate based on profile data
```

---

## Whole-Program Devirtualization

LTO enables the compiler to see all type definitions and call sites across the entire program:

```
Without LTO (single translation unit perspective):
  a.cpp: void process(Base* obj) { obj->compute(); }
  Compiler doesn't know what subclasses Base has → cannot devirtualize

With LTO (whole-program perspective):
  Linker sees:
    Base is inherited by Derived1, Derived2
    process() is only passed objects of type Derived1
  → Can safely replace the virtual call with Derived1::compute()
```

```bash
# LLVM whole-program devirtualization
clang++ -flto=thin -O2 -fwhole-program-vtable a.cpp b.cpp -o app

# View whole-program devirtualization decisions
clang++ -flto=thin -O2 -fwhole-program-vtable \
        -Rpass=wholeprogramdevirt a.cpp -c

# GCC whole-program optimization
g++ -flto -O2 -fwhole-program a.cpp b.cpp -o app
# GCC automatically performs cross-module devirtualization during LTO
```

### Impact of Visibility on Devirtualization

```bash
# If a dynamic library exports a type, the compiler cannot assume
# there are no external subclasses
# Solution: use visibility attributes to restrict exports

# Specify visibility at compile time
clang++ -fvisibility=hidden -flto=thin -O2 a.cpp

# Or mark in source code
class __attribute__((visibility("hidden"))) Internal final {
    virtual int f() { return 0; }
};
```

---

## CRTP: Compile-Time Polymorphism Alternative

CRTP (Curiously Recurring Template Pattern) resolves polymorphism at compile time, completely eliminating virtual function overhead:

```cpp
// CRTP base class
template <typename Derived>
class Shape {
public:
    void draw() const {
        // Compile-time call to Derived's implementation
        static_cast<const Derived*>(this)->draw_impl();
    }
    double area() const {
        return static_cast<const Derived*>(this)->area_impl();
    }
};

// Derived classes
class Circle : public Shape<Circle> {
    double radius_;
public:
    explicit Circle(double r) : radius_(r) {}
    void draw_impl() const { /* draw circle */ }
    double area_impl() const { return 3.14159265 * radius_ * radius_; }
};

class Rect : public Shape<Rect> {
    double w_, h_;
public:
    Rect(double w, double h) : w_(w), h_(h) {}
    void draw_impl() const { /* draw rectangle */ }
    double area_impl() const { return w_ * h_; }
};

// Usage
template <typename T>
void render(const Shape<T>& shape) {
    shape.draw();       // Resolved at compile time → direct call → can be inlined
    double a = shape.area();  // After inlining → constant folding
}
```

```
Virtual Functions vs CRTP:

  ┌────────────────────────────────────┬──────────────────────────────┐
  │ Virtual Functions                  │ CRTP                         │
  │                                    │                              │
  │ Runtime polymorphism               │ Compile-time polymorphism    │
  │ Indirect call (vtable lookup)      │ Direct call (template        │
  │                                    │ instantiation)               │
  │ Heterogeneous containers allowed   │ No heterogeneous containers  │
  │                                    │ (different types)            │
  │ 1 vtable slot (8 bytes/method)     │ Zero extra memory overhead   │
  │ Can dynamically add subclasses     │ All types must be known at   │
  │                                    │ compile time                 │
  │ Cannot be inlined                  │ Fully inlineable             │
  └────────────────────────────────────┴──────────────────────────────┘
```

---

## [[likely]] / [[unlikely]] and Devirtualization

Branch hints help the compiler make better layout decisions during speculative devirtualization:

```cpp
// C++20 branch hints
if (auto* d = dynamic_cast<FastPath*>(obj)) [[likely]] {
    d->fast_compute();   // Placed on hot path → better I-cache locality
} else [[unlikely]] {
    obj->slow_compute(); // Placed on cold path → doesn't affect hot path performance
}
```

```bash
# Both GCC/Clang support [[likely]]/[[unlikely]]
# Compiler places unlikely branches in .text.unlikely section
# Hot path stays compact → I-cache friendly

# View code layout
clang++ -O2 -S test.cpp -o test.s
# Check the distribution of .text.hot and .text.unlikely sections
```

---

## [[gnu::flatten]] and Devirtualization

The `flatten` attribute recursively inlines the entire call tree — it's the "sledgehammer" of devirtualization:

```cpp
__attribute__((flatten))  // Inline all inlineable calls within this function
void hot_loop(Engine* engines, int n) {
    for (int i = 0; i < n; ++i)
        engines[i].step();  // If step() can be devirtualized, recursively inline its body
}
```

---

## Limitations of Devirtualization

```
┌─────────────────────────────────────────────────────────┐
│ Scenarios Where Devirtualization Does Not Apply         │
│                                                         │
│  1. Heterogeneous containers:                           │
│     std::vector<std::unique_ptr<Base>> v;               │
│     for (auto& p : v) p->process();                     │
│     // Element types are inconsistent → compiler cannot │
│     // devirtualize                                     │
│     // Only PGO + speculative devirtualization may      │
│     // partially solve this                             │
│                                                         │
│  2. Plugin architecture:                                │
│     Base* obj = load_plugin(name);                      │
│     obj->execute();                                     │
│     // Dynamic loading → type unknown until runtime     │
│                                                         │
│  3. Cross-DSO calls:                                    │
│     // Virtual function calls in shared libraries →     │
│     // compiler cannot cross DSO boundaries             │
│                                                         │
│  4. Deep inheritance + virtual inheritance:              │
│     // Virtual base class offsets are in vtable →       │
│     // more complex devirtualization logic              │
└─────────────────────────────────────────────────────────┘
```

---

## Practical: Measuring Devirtualization Effect

```bash
# Step 1: Count virtual calls
clang++ -O2 -S test.cpp -o test.s
grep -c 'call.*rax\|call.*rdx' test.s   # Approximate indirect call count

# Step 2: Compare with devirtualization disabled
clang++ -O2 -fno-devirtualize test.cpp -c
# Performance regression = cost of virtual calls

# Step 3: View compiler devirtualization remarks
clang++ -O2 -Rpass=inline test.cpp -c
# Search for "virtual"-related inline remarks

# Step 4: Analyze indirect jumps with perf
perf stat -e branch-misses,branches ./test_app
# High branch-miss rate may indicate excessive virtual calls
```

---

## Further Reading

- [Inlining](/topics/compiler-optimizations/inlining) — Devirtualization unlocks inlining
- [LTO](/topics/compiler-optimizations/lto) — Infrastructure for whole-program devirtualization
- [PGO](/topics/compiler-optimizations/pgo) — Profile-guided speculative devirtualization
- [C++ Compiler Optimization Panorama](/topics/compiler-optimizations) — Overall optimization pipeline
- [Template Metaprogramming](/topics/template-metaprogramming) — CRTP and template techniques
- [Design Patterns](/topics/design-patterns) — Polymorphism in design patterns
- [Value Categories Deep Dive](/topics/value-categories-deep-dive) — Move semantics and virtual function interaction
