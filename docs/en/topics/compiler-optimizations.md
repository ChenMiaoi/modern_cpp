---
title: "C++ Compiler Optimization Panorama"
topic: unknown
feature: compiler-optimizations
standard: N/A
status_checked_at: 2026-06-02
---
# C++ Compiler Optimization Panorama

> Compiler optimization is the "hidden layer" of C++ performance. Between the code you write and the actual machine instructions that execute lies an entire optimization pipeline. Understanding these optimizations is not optional — it determines whether you can write truly efficient code, and whether you can diagnose the root cause when performance issues arise.

---

## Optimization Pipeline Overview

```
C++ Source Code
    |
    v
+-------------------------------------------------------------+
| Frontend                                                     |
|  Lexical Analysis -> Parsing -> Semantic Analysis -> IR Gen  |
|                                                             |
|  C++-specific optimizations:                                 |
|  - constexpr/consteval evaluation                            |
|  - Copy elision (RVO/NRVO)                                   |
|  - Template instantiation deduplication                      |
|  - [[likely]]/[[unlikely]] branch weight annotation          |
+------------------------------+-------------------------------+
                               | LLVM IR / GIMPLE (GCC)
                               v
+-------------------------------------------------------------+
| Middle-end — The Main Battlefield of Optimization            |
|                                                             |
|  Function-level optimizations:                               |
|  - Inlining  <-- The single most important optimization      |
|  - SROA (Scalar Replacement of Aggregates)                   |
|  - Dead Code Elimination (DCE) / Dead Store Elimination (DSE)|
|  - Constant Propagation / Constant Folding                   |
|  - Global Value Numbering (GVN)                              |
|  - Common Subexpression Elimination (CSE)                    |
|                                                             |
|  Loop optimizations:                                         |
|  - Loop-Invariant Code Motion (LICM)                         |
|  - Loop Unrolling                                            |
|  - Loop Vectorization                                        |
|  - Loop Interchange                                          |
|  - Loop Rotation                                             |
|  - Loop Fusion / Loop Fission                                |
|                                                             |
|  Control flow optimizations:                                 |
|  - Jump Threading                                            |
|  - Tail Merging                                              |
|  - Correlated Propagation                                    |
|  - SimplifyCFG                                               |
|                                                             |
|  Interprocedural Optimizations (IPO / LTO):                  |
|  - Interprocedural inlining                                  |
|  - Devirtualization                                          |
|  - Interprocedural constant propagation                      |
|  - Global dead code elimination                              |
|                                                             |
|  Memory optimizations:                                       |
|  - Alias Analysis / TBAA                                     |
|  - Memory dependence analysis                                |
|  - MemCpyOpt                                                 |
|  - ScalarEvolution (loop index analysis)                     |
+------------------------------+-------------------------------+
                               | Optimized IR
                               v
+-------------------------------------------------------------+
| Backend                                                      |
|                                                             |
|  - Instruction Selection (SelectionDAG / GlobalISel / FastISel)|
|  - Instruction Scheduling                                    |
|  - Register Allocation                                       |
|  - Peephole Optimization                                     |
|  - Tail Call Optimization                                    |
|  - Target-specific optimizations (SSE/AVX/NEON vectorization)|
+------------------------------+-------------------------------+
                               |
                               v
                Machine Code (.o / .obj)
```

---

## I. Frontend Optimizations: C++-Specific Compile-Time Transforms

### 1.1 Copy Elision (RVO / NRVO)

The C++ standard **permits** (and since C++17 mandates in certain cases) the compiler to eliminate copies/moves of temporary objects:

```
Source:
  std::string make() {
      std::string s = "hello";
      return s;  // NRVO: construct directly in the caller's stack frame
  }
  auto x = make();

Without optimization:
  Construct s in make() -> copy/move to temporary -> copy/move to x -> destroy temporary -> destroy s

After RVO:
  Construct directly at x's address -> 0 copies, 0 moves

  Caller stack frame:
  +--------------+
  | address of x | <-- make() return value written directly here
  +--------------+
```

**C++17 Guaranteed Copy Elision**:
```cpp
auto p = Widget{};  // Since C++17: compiles even if Widget's move constructor is deleted
// The compiler constructs Widget directly at p's address; no "temporary" exists
```

### 1.2 constexpr / consteval Evaluation

```
Source:
  constexpr int factorial(int n) {
      return n <= 1 ? 1 : n * factorial(n - 1);
  }
  int arr[factorial(5)];  // Compile-time evaluation -> arr[120]

Compiler behavior:
  +--------------------------------------+
  | constexpr interpreter (built-in VM)  |
  |                                      |
  | Input:  factorial(5)                 |
  | Execute: 5 * 4 * 3 * 2 * 1 = 120   |
  | Output:  constant 120               |
  +--------------------------------------+
  
  Final IR: int arr[120];  // Zero runtime overhead
```

### 1.3 Template Instantiation Deduplication

```
When the same template specialization is instantiated in different translation units,
the linker keeps only one copy:

  // a.cpp                          // b.cpp
  vector<int> v1;                    vector<int> v2;

  Both translation units instantiate vector<int>
  -> Merged into a single instance at link time (COMDAT folding)
  -> No increase in binary size
```

---

## II. Inlining: The Single Most Important Optimization

Inlining is the **cornerstone** of compiler optimization — it copies the function body to the call site, eliminates function call overhead, and opens the door for subsequent optimizations.

### 2.1 Why Inlining Is the Most Important

```
Before inlining:              After inlining:
+-------------+              +-------------------------+
| call foo(x) |              | y = x * 2 + 1;          | <-- function body expanded
|             |              | if (y > 0) return y;    |
| foo:        |              | else return -y;         |
|   y = x * 2 |              |                         |
|   y += 1    |    ---->     | Subsequent opts can:     |
|   if y>0    |              | - Constant propagation   |
|     ret y   |              |   (if x is known)        |
|   else      |              | - Dead code elimination  |
|     ret -y  |              | - Loop unrolling         |
+-------------+              | - Vectorization          |
                             +-------------------------+
```

Subsequent optimizations unlocked by inlining:
- **Constant propagation**: If the argument is a constant, the entire function body can fold to a constant after inlining
- **Dead code elimination**: The untaken branch of `if (constexpr_condition)` is eliminated
- **Loop optimizations**: The loop body grows larger after inlining, potentially exposing vectorization opportunities
- **Alias analysis**: After inlining, the compiler can see more pointer relationships

### 2.2 Inlining Decisions

```
LLVM's inlining heuristics (simplified):

  Inlining benefit = eliminated call overhead + benefit from subsequent optimizations
  Inlining cost    = code bloat -> I-cache pressure + compile time

  +-------------------------------------------+
  | Tends to inline:                           |
  | - Function body is small (< a few instrs)  |
  | - Only one call site (no bloat)            |
  | - Argument is a compile-time constant      |
  |   (unlocks constant folding)               |
  | - Marked with __attribute__((always_inline))|
  | - [[gnu::flatten]] (recursively inline     |
  |   entire call chain)                        |
  |                                            |
  | Tends NOT to inline:                       |
  | - Function body is too large               |
  | - Recursive function (generally not inlined,|
  |   unless tail-recursive)                    |
  | - Virtual function (cannot resolve statically)|
  | - -Os (stricter threshold for size)        |
  +-------------------------------------------+
```

### 2.3 Practical Example

```cpp
// Source
int compute(int x) {
    return x * x + 2 * x + 1;
}
int result = compute(42);

// After inlining + constant folding:
// compute(42) -> 42*42 + 2*42 + 1 -> 1764 + 84 + 1 = 1849
// Final IR: int result = 1849;  // Zero runtime computation
```

---

## III. SROA: Scalar Replacement of Aggregates

SROA (Scalar Replacement of Aggregates) is one of the most important optimizations in LLVM — it decomposes structs into independent scalar variables.

```
Source:
  struct Point { int x, y; };
  Point p;
  p.x = 10;
  p.y = 20;
  return p.x + p.y;

Before SROA (p is an alloca in IR):
  %p = alloca { i32, i32 }
  store { i32, i32 } { i32 10, i32 20 }, ptr %p
  %x = load i32, ptr getelementptr(%p, 0, 0)
  %y = load i32, ptr getelementptr(%p, 0, 1)
  %sum = add i32 %x, %y

After SROA (decomposed into independent variables):
  ; p is completely eliminated; x and y become SSA registers
  %sum = add i32 10, 20    ; constant propagation further folds to 30
```

**SROA is a generalization of `mem2reg`**: `mem2reg` only handles alloca-to-SSA conversion for simple variables; SROA handles decomposition of structs, arrays, and other composite types.

---

## IV. Loop Optimizations

### 4.1 Loop-Invariant Code Motion (LICM)

```
Source:
  for (int i = 0; i < n; ++i) {
      a[i] = x * y + i;  // x * y is invariant across the loop
  }

After LICM:
  int tmp = x * y;       // Hoisted outside the loop
  for (int i = 0; i < n; ++i) {
      a[i] = tmp + i;
  }
```

### 4.2 Loop Unrolling

```
Source:
  for (int i = 0; i < 4; ++i) a[i] = 0;

After full unrolling:
  a[0] = 0; a[1] = 0; a[2] = 0; a[3] = 0;
  // Eliminates branches, eliminates loop variable increment

Partial unrolling (4-way):
  for (int i = 0; i < n; i += 4) {
      a[i]   = 0;
      a[i+1] = 0;
      a[i+2] = 0;
      a[i+3] = 0;
  }
  // Branch count reduced by 4x, creates conditions for vectorization
```

### 4.3 Loop Vectorization

```
Source:
  for (int i = 0; i < n; ++i)
      c[i] = a[i] + b[i];

After SSE2 vectorization (128-bit, 4 floats):
  for (i = 0; i < n; i += 4) {
      va = _mm_load_ps(&a[i]);      // Load 4 floats
      vb = _mm_load_ps(&b[i]);      // Load 4 floats
      vc = _mm_add_ps(va, vb);      // One instruction completes 4 additions
      _mm_store_ps(&c[i], vc);      // Store 4 results
  }

AVX2 vectorization (256-bit, 8 floats):
  // Same logic, but processes 8 floats at a time -> throughput doubles

AVX-512 vectorization (512-bit, 16 floats):
  // Processes 16 floats at a time -> throughput doubles again
```

**Conditions for auto-vectorization**:
```
The compiler must prove:
  1. The trip count is known at compile time, or there is explicit tail handling
  2. No loop-carried dependencies (a[i] does not depend on the result of a[i-1])
  3. Memory access is contiguous (no indirect accesses with stride > 1)
  4. No alias conflicts (a and b do not overlap)

  __restrict__ tells the compiler "pointers do not alias" -> satisfies condition 4
```

### 4.4 Loop-Invariant Condition Hoisting + Loop Rotation

```
Source:
  while (x > 0) {
      if (cond) { x -= a; }
      else      { x -= b; }
  }

After Loop Rotation:
  if (x > 0) {
      do {
          if (cond) { x -= a; }
          else      { x -= b; }
      } while (x > 0);
  }
  // Eliminates one conditional branch (loop bottom jumps directly back to loop body)
```

### 4.5 Loop Fusion / Loop Fission

```
Loop Fusion:
  // Source: two independent loops
  for (i) a[i] = f(i);
  for (i) b[i] = g(a[i]);
  
  // After fusion: single traversal
  for (i) { a[i] = f(i); b[i] = g(a[i]); }
  // Reduces loop overhead, improves cache locality

Loop Fission:
  // Source: loop body is too large
  for (i) { a[i] = f(i); b[i] = g(i); c[i] = h(i); }
  
  // After fission: each loop is more compact, may vectorize separately
  for (i) a[i] = f(i);
  for (i) b[i] = g(i);
  for (i) c[i] = h(i);
```

---

## V. Control Flow Optimizations

### 5.1 Jump Threading

```
Source:
  if (x > 0) {
      // ... some code ...
      if (x > 0) {  // Redundant condition (x unchanged in between)
          do_something();
      }
  }

After Jump Threading:
  if (x > 0) {
      // ... some code ...
      do_something();  // Skips redundant condition check
  }
```

### 5.2 Tail Merging

```
Source:
  switch (type) {
      case A: process_a(); cleanup(); return;
      case B: process_b(); cleanup(); return;
      case C: process_c(); cleanup(); return;
  }

After Tail Merging:
  switch (type) {
      case A: process_a(); goto common;
      case B: process_b(); goto common;
      case C: process_c(); goto common;
  }
  common: cleanup(); return;
  // Three returns merged into one -> reduces code size
```

### 5.3 Correlated Propagation

```
Source:
  if (x > 0) {
      // ... 20 lines of code ...
      if (x > 0) {  // x not modified inside the if block -> condition always true
          do_work();
      }
  }

After Correlated Propagation:
  if (x > 0) {
      // ... 20 lines of code ...
      do_work();  // Condition propagated and eliminated
  }
```

---

## VI. Alias Analysis and TBAA

### 6.1 Alias Analysis

```
Problem: The compiler needs to know whether two pointers point to the same memory

  void foo(int* a, int* b, int* c) {
      *a = 1;
      *b = 2;
      *c = *a;  // Can c equal a? Can it equal b?
  }

  // If the compiler can prove c != b, then *c = *a = 1
  // If c might equal b, then *c = 2 (because *b = 2 overwrote *a = 1)
```

### 6.2 TBAA (Type-Based Alias Analysis)

```
C/C++ strict aliasing rule: pointers of different types generally do not alias

  int* pi = ...;
  float* pf = ...;
  *pi = 42;
  *pf = 3.14f;
  printf("%d", *pi);  // Compiler may assume *pi is still 42
                       // because int* and float* do not alias under TBAA

TBAA metadata (LLVM IR):
  store i32 42, ptr %pi, !tbaa !{!"int", !"any pointer"}
  store float 3.14, ptr %pf, !tbaa !{!"float", !"any pointer"}
  ; Two !tbaa tags have different types -> compiler assumes no aliasing
```

### 6.3 `__restrict__` Tells the Compiler

```cpp
// Without restrict -> compiler must assume a and b may overlap
void add(float* a, float* b, int n) {
    for (int i = 0; i < n; ++i) a[i] += b[i];
    // Compiler cannot vectorize (a[i] write may affect subsequent b[i] reads)
}

// With restrict -> compiler knows a and b do not overlap -> can safely vectorize
void add(float* __restrict__ a, float* __restrict__ b, int n) {
    for (int i = 0; i < n; ++i) a[i] += b[i];
    // Compiler can vectorize: load all b[i] first, then batch-add to a[i]
}
```

---

## VII. Devirtualization

### 7.1 Performance Cost of Virtual Functions

```
Virtual function call:
  obj->virtual_method(args);
  
  Machine code:
  mov rax, [obj]           ; Load vtable pointer
  mov rax, [rax + offset]  ; Load function pointer from vtable
  call rax                  ; Indirect call

  Problems:
  - Indirect call -> CPU branch predictor has difficulty predicting the target
  - Cannot inline (actual type unknown at compile time)
  - Blocks all subsequent optimizations
```

### 7.2 `final` Triggers Devirtualization

```cpp
class Base {
    virtual void foo();
};
class Derived final : public Base {
    void foo() override;
};

void call(Derived* d) {
    d->foo();  // Derived is final -> compiler knows the actual type
    // Calls Derived::foo() directly, no vtable lookup needed
    // Can be inlined!
}
```

### 7.3 Automatic Devirtualization by the Compiler

```cpp
// The compiler can track the actual type of an object
Derived d;
Base* b = &d;
b->foo();  // Compiler knows b points to Derived -> calls Derived::foo() directly

// Consecutive virtual calls on the same object -> compiler can cache the vtable lookup
obj->a();  // Load vtable
obj->b();  // Reuse already-loaded vtable (if compiler can prove obj's type hasn't changed)
```

---

## VIII. Dead Code Elimination (DCE) and Dead Store Elimination (DSE)

### 8.1 DCE: Eliminating Code That Does Not Affect Program Output

```
Source:
  int x = compute();  // If x is never used
  int y = other();    // If y is only used in a subsequent computation that is also unused
  return 42;

After DCE:
  return 42;  // The computations of x and y are completely eliminated
```

### 8.2 DSE: Eliminating Overwritten Stores

```
Source:
  *p = 1;
  // ... some code that does not read *p ...
  *p = 2;     // The first write is overwritten -> can be eliminated

After DSE:
  // *p = 1;  eliminated
  *p = 2;
```

### 8.3 Aggressive DSE

```
Source:
  void foo(int* p) {
      *p = 42;
  }  // After the function returns, *p is local state -> the write can be eliminated

  If the compiler can prove that the memory pointed to by p will not be read
  after the function returns
  -> The entire write operation is eliminated
```

---

## IX. Interprocedural Optimizations (IPO / LTO)

### 9.1 Link-Time Optimization (LTO)

```
Traditional compilation:
  a.cpp -> a.o (optimized)
  b.cpp -> b.o (optimized)
  Link: a.o + b.o -> executable
  // Each .o is optimized independently; cross-file information is lost

LTO compilation:
  a.cpp -> a.o (with IR metadata)
  b.cpp -> b.o (with IR metadata)
  Link: merge IR -> global optimization -> generate machine code
  // Cross-file inlining, cross-file constant propagation, global dead code elimination
```

### 9.2 Actual Benefits of LTO

```
Typical benefits of enabling LTO:
  - Cross-file inlining (most important)
  - Cross-file constant propagation
  - Global dead code elimination (unused cross-file functions)
  - Cross-file type hierarchy analysis (devirtualization)
  - Cross-file memory optimizations

Costs:
  - Significantly increased link time
  - Increased memory usage (all IR in memory simultaneously)
  - Poor incremental compilation support (changing one file requires full relinking)
```

### 9.3 ThinLTO (Lightweight LTO)

```
ThinLTO (LLVM's default LTO mode):
  - Only merges function summaries at link time, not complete IR
  - Optimizes each module in parallel (summaries guide cross-module inlining decisions)
  - Much faster than full LTO, but with slightly less benefit
  
  Typical scenario: large projects use ThinLTO; small critical libraries use Full LTO
```

---

## X. Profile-Guided Optimization (PGO)

### 10.1 PGO Workflow

```
Step 1: Instrumented Build
  Source -> instrumented executable (records which branches are taken, how many
  times loops execute, function call frequencies)

Step 2: Collect Profile
  Run the instrumented build with a representative workload -> generates .profraw file

Step 3: Optimized Build
  Source + .profraw -> optimized executable
  
  The compiler leverages profile data:
  - Hot functions are prioritized for inlining
  - Cold paths (exception handling, error checking) moved to the end of the code section
  - Branch weight annotation (likely/unlikely determined by actual data, not guessing)
  - Loop unroll counts determined by actual iteration counts
  - Basic block layout (hot paths placed contiguously, reducing I-cache misses)
```

### 10.2 Actual Benefits of PGO

```
Google's internal practice:
  - Chromium: PGO delivers ~10-15% performance improvement
  - Internal C++ services: average ~15-20% throughput improvement
  
  Main sources of benefit:
  - More accurate inlining decisions (40%+ of the benefit)
  - Better code layout (30%+)
  - Better branch prediction (20%+)
```

### 10.3 BOLT (Binary Optimization and Layout Tool)

```
BOLT operates on the post-link binary:
  1. Collect runtime profile (via perf or LBR)
  2. Reorder basic blocks and functions (hot functions grouped, cold functions separated)
  3. Optimize branch layout
  
  Additional benefit: 5-10% (on top of PGO)
  Facebook uses BOLT on its services and achieved significant gains
```

---

## XI. Optimization Barriers: What Prevents the Compiler from Optimizing

### 11.1 `volatile`

```
volatile tells the compiler: "every access must actually read/write memory"

  volatile int x = 0;
  x = 1;    // Must write to memory
  x = 2;    // Must write to memory (even though the previous line just wrote)
  int y = x; // Must read from memory (even though the compiler knows 2 was just written)

  The compiler cannot:
  - Eliminate reads or writes to volatile variables
  - Reorder the relative sequence of volatile operations
  - Cache a volatile value in a register
```

### 11.2 Atomics and Memory Ordering

```
std::atomic<int> x;
x.store(1, std::memory_order_seq_cst);  // Total ordering guarantee
x.load(std::memory_order_acquire);       // Acquire semantics

  The compiler cannot:
  - Reorder operations after an acquire to before the acquire
  - Reorder operations before a release to after the release
  - Eliminate accesses to atomic variables (unless it can prove no other thread
    can observe them)
```

### 11.3 Inline Assembly (`asm volatile`)

```
  asm volatile("cpuid" : "=a"(eax) : "a"(0) : "ebx", "ecx", "edx");
  
  The compiler must:
  - Emit this instruction at the specified location
  - Cannot move it (volatile guarantee)
  - Cannot assume it does not modify "unlisted" registers (clobber list)
```

### 11.4 External Function Calls

```
  foo();      // The compiler does not know what foo does
  x = *p;    // foo may have modified *p -> must reload
  
  // If foo is inlined, the compiler can see it does not modify *p
  // -> x = *p can reuse the previously cached value
```

---

## XII. Practical Tips: How to Observe and Control Optimizations

### 12.1 Godbolt Compiler Explorer

```
https://godbolt.org/ — View compiler output online

How to use:
  1. Enter C++ source code
  2. Select compiler and optimization level
  3. View the generated assembly code
  
  Key tips:
  - Use -O2 (not -O0) to observe real optimization effects
  - Use -S -emit-llvm to view LLVM IR (more readable than assembly)
  - Use -Rpass=inline to see which functions were inlined
  - Use -Rpass=loop-vectorize to view vectorization decisions
```

### 12.2 Common Compiler Flags

```
-O0: No optimization (for debugging, fastest compilation)
-O1: Basic optimization (eliminates obvious dead code, simple inlining)
-O2: Recommended production optimization level (balances compile time and runtime performance)
-O3: Aggressive optimization (more aggressive inlining, vectorization, loop transformations)
-Os: Optimize for size (suitable for embedded systems, reduces I-cache pressure)
-Oz: Extreme size optimization (further reduces code size)

-Ofast: -O3 + IEEE 754 non-compliant floating-point optimizations (may change computation results)

Clang-specific:
  -Rpass=.*              : Report all optimization decisions
  -Rpass-missed=.*       : Report missed optimization opportunities
  -Rpass-analysis=.*     : Report optimization analysis results
  -fsave-optimization-record : Generate YAML optimization record

GCC-specific:
  -fopt-info             : Report optimization decisions
  -fopt-info-optimized   : Report only successful optimizations
  -fopt-info-missed      : Report only missed optimizations
```

### 12.3 LLVM IR Viewing

```bash
# Generate LLVM IR (human-readable format)
clang++ -S -emit-llvm -O2 source.cpp -o source.ll

# View the effect of optimization passes
clang++ -S -emit-llvm -O2 -mllvm -print-after-all source.cpp 2>&1 | less

# View IR only after a specific pass
clang++ -S -emit-llvm -O2 -mllvm -print-after=inline source.cpp
```

---

## XIII. C++-Specific Optimization Interactions

### 13.1 `noexcept` Impact on Optimization

```cpp
// Without noexcept -> compiler must generate exception handling code
void process(std::vector<int>& v) {
    v.push_back(42);  // May throw -> compiler generates unwind tables
}

// With noexcept (or compiler deduces noexcept) -> no unwind tables needed
// vector::reserve uses move_if_noexcept to exploit this:
//   noexcept move -> use move (fast)
//   potentially throwing move -> use copy (slow but safe)
```

### 13.2 `constexpr` Impact on Optimization

```cpp
constexpr auto table = generate_lut();  // Computed at compile time -> embedded in .rodata section
// Zero runtime overhead; read directly from the read-only data section

// C++20 constexpr in more scenarios:
constexpr std::vector<int> v = {1, 2, 3};  // Compile-time vector (since C++20)
// But note: the heap memory of a constexpr vector is freed after compilation finishes
// What is used at runtime is a constant copy embedded by the compiler
```

### 13.3 `[[no_unique_address]]` Impact on Code Size

```cpp
// Empty allocator takes up space
struct Bad {
    int data;
    std::allocator<int> alloc;  // Usually 1 byte, but after alignment may occupy 4-8 bytes
};

// Use [[no_unique_address]] to eliminate
struct Good {
    int data;
    [[no_unique_address]] std::allocator<int> alloc;  // 0 bytes
};
// sizeof(Good) == sizeof(int) -> 4 bytes
// Smaller object -> better cache utilization -> faster traversal
```

### 13.4 Trivially Relocatable: The Future

```
Current (before C++26): vector::reserve must move + destroy element by element
  for (each element) {
      new (dst) T(std::move(*src));  // move construct
      src->~T();                      // destroy
  }

With trivially relocatable (P2786):
  if constexpr (std::trivially_relocatable<T>) {
      memcpy(dst, src, n * sizeof(T));  // single memcpy
  }
  // For simple types like unique_ptr, shared_ptr, string
  // Performance improvement can reach 5-10x (on the reserve hot path)
```

---

## Summary: Optimization Decision Tree

```
Compiler optimization awareness when writing C++ code:

  1. Should the function be inlined?
     - Small function (< 10 lines) -> compiler usually inlines automatically
     - Large function on hot path -> consider __attribute__((always_inline))
     - Virtual function -> use final to help devirtualization

  2. Is the data structure cache-friendly?
     - Contiguous memory (vector, array) -> fast traversal
     - Node-based (list, map) -> slower traversal but faster insertion/deletion
     - SoA vs AoS -> choose based on access pattern

  3. Can the loop be optimized?
     - No loop-carried dependencies -> may be vectorized
     - __restrict__ -> helps alias analysis
     - Invariants inside the loop body -> hoist outside the loop

  4. Can compile-time computation be leveraged?
     - constexpr function -> evaluated at compile time
     - Template metaprogramming -> generate code at compile time
     - consteval -> forces compile-time evaluation

  5. Does the compiler have enough information?
     - noexcept -> eliminates exception handling overhead
     - __restrict__ -> eliminates aliasing assumptions
     - [[likely]]/[[unlikely]] -> optimizes branch layout
     - alignas -> ensures SIMD alignment

Core principles:
  - Enable the compiler to optimize first (provide information), then optimize manually
  - Compile with -O2 and inspect the assembly to confirm optimizations are applied
  - Do not guess about performance — measure
  - Understand the boundaries of optimization (what can be optimized, what cannot)
