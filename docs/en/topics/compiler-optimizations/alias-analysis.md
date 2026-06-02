---
title: "Alias Analysis"
topic: topics
feature: compiler-opt-alias-analysis
standard: C++
status_checked_at: 2026-06-02
---

# Alias Analysis

> Alias analysis is the bedrock of compiler optimization — it answers "can these two pointers refer to the same memory?" The answer directly affects a range of optimizations including load/store optimization, loop vectorization, and instruction reordering. Alias analysis failure = conservative optimization = performance loss.

---

## Why Alias Analysis Matters

```cpp
void update(int* a, int* b) {
    *a = 10;
    *b = 20;
    int x = *a;  // Is x 10 or 20?
}
```

```
If a and b do not alias (a != b):
  *a = 10;
  *b = 20;
  x = *a;  // Compiler can optimize to x = 10 (constant propagation)
  → 1 store + 1 store + 1 constant = no extra load

If a and b may alias (when a == b):
  *a = 10;
  *b = 20;  // May have overwritten *a
  x = *a;   // Compiler must reload from memory → 1 extra load
  → Cannot do constant propagation
```

---

## Layers of Alias Analysis

LLVM has multiple layers of alias analysis, from coarse to fine:

```
Analysis Level          Precision   Cost
─────────────────────────────────────────
BasicAA (basic)         Medium      Low
  · alloca does not alias with others
  · Global variables do not alias with alloca
  · const does not alias
─────────────────────────────────────────
TBAA (type-based)       High        Low
  · Pointers of different types usually do not alias
  · Follows C/C++ strict aliasing rules
─────────────────────────────────────────
ScopedNoAlias           High        Low
  · Scope-level __restrict__
─────────────────────────────────────────
GlobalsModRef           High        Medium
  · Analyzes read/write patterns of global variables
─────────────────────────────────────────
CFLAnders/Steensgaard   Med-High    Med-High
  · Based on Andersen/Steensgaard algorithms
  · Interprocedural analysis
─────────────────────────────────────────
```

```bash
# View LLVM's alias analysis results for a specific function
clang++ -O2 -mllvm -debug-only=aa test.cpp -c 2>&1
# Outputs alias query results for each pointer pair: NoAlias / MayAlias / MustAlias / PartialAlias

# GCC alias analysis
g++ -O2 -fdump-tree-alias test.cpp
# View alias analysis results in the .alias file
```

---

## TBAA: Type-Based Alias Analysis

TBAA is the most important alias analysis technique for C/C++ compilers, based on the **Strict Aliasing Rule**:

```cpp
// C++ standard specifies: accessing an object through a pointer of a different
// type is UB (except char*/void*)
int i = 42;
float* fp = reinterpret_cast<float*>(&i);
*fp = 3.14f;  // ⚠️ Undefined behavior (violates strict aliasing rule)
```

```
TBAA principle:
  If types T1 and T2 are unrelated (not parent-child),
  then T1* and T2* do not alias.

  TBAA metadata in LLVM IR:
  %x = load i32, ptr %p, !tbaa !{...}
  %y = load float, ptr %q, !tbaa !{...}
  ; If the TBAA type trees for i32 and float do not overlap
  ; → compiler knows %p and %q do not alias
  ; → can freely reorder these two loads
```

### TBAA Metadata in LLVM IR

```bash
# Generate LLVM IR with TBAA metadata
clang++ -O2 -Xclang -disable-llvm-optzns -emit-llvm -S test.cpp -o test.ll

# View TBAA metadata
grep -A 5 '!tbaa' test.ll
```

```
Typical TBAA metadata:
  !0 = !{!"Simple C++ TBAA"}
  !1 = !{!"omnipotent char", !0, i64 0}
  !2 = !{!"int", !1, i64 0}
  !3 = !{!"float", !1, i64 0}
  !4 = !{!"double", !1, i64 0}

  Type tree:
    Simple C++ TBAA (root)
        │
    omnipotent char (char type can alias any type)
        │
    ┌───┼───┬───┐
  int  float  double  ...

  TBAA query:
    load i32 (TBAA: int) vs load float (TBAA: float)
    → int and float are unrelated in the tree → NoAlias

    load i32 (TBAA: int) vs load i8 (TBAA: char)
    → char is omnipotent alias → MayAlias
```

---

## The __restrict__ Keyword

`__restrict__` is the programmer's guarantee to the compiler that "this pointer does not alias with other pointers":

```cpp
// Without restrict: compiler assumes a, b may alias
void add(float* a, float* b, float* c, int n) {
    for (int i = 0; i < n; ++i)
        c[i] = a[i] + b[i];
    // Compiler cannot reorder load/store → may not be able to vectorize
}

// With restrict: compiler trusts the programmer's guarantee
void add_restricted(float* __restrict__ a,
                    float* __restrict__ b,
                    float* __restrict__ c, int n) {
    for (int i = 0; i < n; ++i)
        c[i] = a[i] + b[i];
    // Compiler knows a, b, c do not alias → can safely vectorize
}
```

```bash
# View restrict's effect
clang++ -O2 -Rpass=loop-vectorize add.cpp -c
# Without restrict may warn "cannot prove it is safe to reorder"
# With restrict → successful vectorization

# GCC
g++ -O2 -ftree-vectorize -fdump-tree-vect-details add.cpp
```

### Scoped NoAlias Metadata

LLVM encodes `__restrict__` scope information as `!noalias` and `!alias.scope` metadata:

```
define void @add(float* noalias %a, float* noalias %b, float* noalias %c) {
  ; !alias.scope defines scopes
  ; !noalias declares non-aliasing with certain scopes
  %v = load float, ptr %a, !alias.scope !5, !noalias !6
  ; !5 = {scope_a}
  ; !6 = {scope_b, scope_c}
  ; → load from %a does not alias with %b or %c
}
```

```bash
# View scoped noalias metadata
clang++ -O2 -emit-llvm -S -Xclang -disable-O0-optnone test.cpp -o test.ll
grep -E '!noalias|!alias.scope' test.ll
```

---

## MayAlias Attribute

GCC and Clang provide `__attribute__((may_alias))` to tell the compiler that a type may alias with other types:

```cpp
// Typical usage: correct type punning implementation
typedef int __attribute__((may_alias)) aliased_int;

float bit_cast(int x) {
    aliased_int ai = x;
    float f;
    // Read through may_alias type → does not violate strict aliasing rule
    __builtin_memcpy(&f, &ai, sizeof(f));  // Preferred approach
    return f;
}

// std::bit_cast (C++20) is the standard type-safe punning mechanism
#include <bit>
float f = std::bit_cast<float>(x);
```

---

## Points-to Analysis

Points-to analysis answers "which memory locations might this pointer point to":

```
Points-to set computation example:

  int a, b, c;
  int* p;
  if (cond)
      p = &a;
  else
      p = &b;

  → p's points-to set = {&a, &b}
  → p cannot point to &c
  → *p and c do not alias
```

```
LLVM's BasicAA conservative rules:
  ┌─────────────────────────────────────────────────┐
  │ alloca does not alias with:                     │
  │  · Other allocas                                │
  │  · Global variables (unless passed via pointer) │
  │  · Function arguments (unless address explicitly│
  │    passed)                                       │
  │                                                  │
  │ Between global variables:                       │
  │  · Different globals do not alias               │
  │  · Different aliases of the same global may     │
  │    alias                                         │
  │                                                  │
  │ Function arguments:                             │
  │  · Arguments marked noalias do not alias with   │
  │    others                                        │
  │  · Unmarked arguments → conservatively assume   │
  │    MayAlias                                     │
  └─────────────────────────────────────────────────┘
```

---

## Impact of Alias Analysis on Load/Store Optimization

```cpp
// Example 1: Dead store elimination
void example1(int* p, int* q) {
    *p = 10;   // Can this store be eliminated?
    *q = 20;
    // If p and q don't alias → *p = 10 is a dead store → eliminable
    // If p and q alias → *q = 20 overwrites *p → *p = 10 is still dead store → eliminable
    // But if there's a read-through-p between *p = 10 and *q = 20 → cannot eliminate *p = 10
}

// Example 2: Load hoisting (LICM)
void example2(int* p, int* q) {
    for (int i = 0; i < n; ++i) {
        int x = *p;      // Does p's value change in the loop?
        a[i] = x + *q;   // *q may change each iteration
    }
    // If p and q don't alias → *p doesn't change in loop → *p can be LICM hoisted
    // If p and q alias → *q's store may have changed *p → cannot hoist
}

// Example 3: Store-to-load forwarding
void example3(int* p, int* q) {
    *p = 42;
    // ... no other writes in between ...
    int x = *p;  // Compiler can optimize load to x = 42
    // Prerequisite: p does not alias with any intervening operation
}
```

---

## Volatile Semantics and Aliasing

`volatile` has special aliasing semantics — it prevents the compiler from optimizing memory accesses:

```cpp
// volatile guarantees:
// 1. Every read/write actually accesses memory (cannot cache in register)
// 2. Access order cannot be reordered
// 3. Access count cannot be increased or decreased

volatile int* hw_reg = /* MMIO address */;

void poll() {
    while (*hw_reg == 0)  // Must re-read memory every time
        ;                  // Cannot optimize to read only once
}

// volatile's impact on alias analysis:
// volatile load/store cannot be reordered with any other load/store
// → Acts as a "compiler barrier"
```

---

## Atomic Operations and Aliasing

C++ atomic operations have special effects on alias analysis:

```cpp
std::atomic<int> flag;
int data;

void writer() {
    data = 42;                      // Normal write
    flag.store(1, std::release);    // Release semantics
    // Guarantee: data = 42 is visible to other threads before flag
}

void reader() {
    while (flag.load(std::acquire) == 0)
        ;
    // Acquire semantics: guarantees seeing writes before flag
    printf("%d\n", data);           // Guaranteed to see 42
}
```

```
Impact of atomic operations on alias analysis:
  ┌─────────────────────────────────────────────────┐
  │ acquire/release semantics create happens-before  │
  │ relationships                                   │
  │ → Compiler cannot reorder non-atomic accesses   │
  │   across atomic operations                      │
  │                                                  │
  │ But between atomic operations (same variable):  │
  │ · Accesses to the same atomic variable do not   │
  │   alias non-atomic variables                    │
  │   (unless related through other means)          │
  │ · Atomic loads can be merged by compiler (if    │
  │   safe)                                         │
  └─────────────────────────────────────────────────┘
```

---

## Common Cases of Alias Analysis Failure

```cpp
// Case 1: Union type punning (UB, but compilers usually accept)
union U { int i; float f; };
float bad_cast(int x) {
    U u;
    u.i = x;
    return u.f;  // Legal in C, UB in C++ (prior to C++20 constexpr relaxation)
    // Correct approach: std::bit_cast<float>(x)
}

// Case 2: void* indirection
void process(void* buf) {
    int* ip = static_cast<int*>(buf);
    float* fp = static_cast<float*>(buf);
    *ip = 42;
    *fp = 3.14f;  // Compiler may think ip and fp alias (both derived from void*)
}

// Case 3: STL container iterators
void transform(std::vector<int>& v) {
    for (auto& x : v)
        x *= 2;
    // vector's internal pointer is opaque to the compiler
    // → alias analysis may be conservative
}
```

---

## Practical: Debugging Alias Analysis

```bash
# LLVM: view alias analysis queries
clang++ -O2 -mllvm -debug-only=aa test.cpp -c 2>&1
# Output:
#   NoAlias:  %p (alloca) and %q (alloca)
#   MayAlias: %r (argument) and %s (argument)

# LLVM: use opt to view alias analysis pass
opt -passes='print<aa>' test.ll -disable-output

# GCC: view alias analysis log
g++ -O2 -fdump-tree-alias test.cpp
# View .alias file

# Compare performance with and without restrict
clang++ -O2 -fno-strict-aliasing test.cpp -c    # Disable strict aliasing
clang++ -O2 test.cpp -c                          # Enable strict aliasing
# Compare assembly and performance of both
```

---

## Further Reading

- [Vectorization](/topics/compiler-optimizations/vectorization) — Alias analysis failure is the primary cause of vectorization failure
- [SROA](/topics/compiler-optimizations/sroa) — SROA relies on alias analysis to determine escape
- [Inlining](/topics/compiler-optimizations/inlining) — Inlining exposes more alias information
- [C++ Compiler Optimization Panorama](/topics/compiler-optimizations) — Overall optimization pipeline
- [Memory Model and Concurrency](/topics/memory-model) — Atomic operations and memory ordering
- [Performance Optimization](/topics/performance) — `__restrict__` in practical projects
