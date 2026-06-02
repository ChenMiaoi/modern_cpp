---
title: "Function Inlining"
topic: topics
feature: compiler-opt-inlining
standard: C++
status_checked_at: 2026-06-02
---

# Function Inlining

> Inlining is the **most important single optimization** in the compiler optimization pipeline. It not only eliminates function call overhead, but more importantly exposes the callee's code to the caller's context, unlocking downstream optimizations such as constant propagation, dead code elimination, and loop vectorization.

---

## The Essence of Inlining

```
Before inlining:                   After inlining:
┌──────────────────┐             ┌──────────────────────────┐
│ caller:          │             │ caller:                  │
│   ...            │             │   ...                    │
│   call foo(a, b) │   ──────→   │   ; foo's code expanded: │
│   ...            │             │   tmp = a + b;            │
│                  │             │   if (tmp > 0) ret tmp;   │
│ foo(x, y):       │             │   else ret -tmp;          │
│   z = x + y      │             │   ...                     │
│   if (z > 0)     │             │                           │
│     ret z        │             │ Subsequent optimizations   │
│   else           │             │ can now:                  │
│     ret -z       │             │ · If a=5, b=3 → constant  │
└──────────────────┘             │   folding                 │
                                 │ · Dead branch elimination │
                                 └──────────────────────────┘
```

Overhead eliminated by inlining:
- Argument push/pass instructions
- `call`/`ret` instruction pair (branch prediction pressure)
- Stack frame setup/teardown
- Register save/restore

---

## Inline Thresholds and Cost Model

### LLVM Inline Cost Model

LLVM's inline decisions are made by the `InlineCost` analysis:

```
InlineCost = Σ(instruction_weight) - Threshold_bonus

Decision rule:
  if InlineCost ≤ Threshold → inline
  else → do not inline

Threshold is influenced by the following factors:
  ┌───────────────────────────────────────────────────┐
  │ Base threshold (determined by optimization level): │
  │   -O0:  0    (almost never inlines)                │
  │   -O1:  75                                          │
  │   -O2:  225                                         │
  │   -O3:  275                                         │
  │   -Os:  50   (lower bar when optimizing for size)  │
  │   -Oz:  25                                          │
  │                                                     │
  │ Bonus factors (reduce inline cost):                │
  │   · Call site is on hot path (+15000)              │
  │   · Arguments are constants (+2000 per constant    │
  │     arg)                                            │
  │   · Single call site (+1500)                       │
  │   · Function body is very small (< 5 instructions, │
  │     always inline)                                  │
  │                                                     │
  │ Penalty factors (increase inline cost):            │
  │   · Large function body                            │
  │   · Multiple call sites (code bloat)               │
  │   · Contains loops                                 │
  │   · Contains function calls (uncertain overhead)   │
  └───────────────────────────────────────────────────┘
```

### GCC Inline Parameters

```bash
# GCC inline-related --param options
g++ -O2 \
    --param inline-unit-growth=30 \         # Allow 30% growth per translation unit
    --param large-function-growth=200 \     # Allow 200% growth per function
    --param inline-insns-auto=15 \          # Max instructions for auto inlining
    --param inline-insns-single=40 \        # Max instructions for single call site
    --param max-inline-insns-size=500 \     # Hard limit
    test.cpp

# View GCC's inline decisions
g++ -O2 -fdump-ipa-inline test.cpp
# Outputs .inline file with reasoning for each inline decision
```

### LLVM Inline Threshold Control

```bash
# Manually set inline threshold
clang++ -O2 -mllvm -inline-threshold=300 test.cpp

# View inline decisions
clang++ -O2 -Rpass=inline test.cpp
# Output: test.cpp:5:5: remark: 'foo' inlined into 'bar'

# View reasons for not inlining
clang++ -O2 -Rpass-missed=inline test.cpp
# Output: test.cpp:5:5: remark: 'foo' not inlined: cost=500 threshold=225
```

---

## Forced Inlining and Inlining Prohibition

### Forced Inlining

```cpp
// GCC / Clang
__attribute__((always_inline))
inline void fast_path(int x) {
    // Compiler guarantees inlining, even if threshold is not met
}

// MSVC
__forceinline void fast_path(int x) {
    // MSVC forced inlining
}

// C++20 has no standard attribute, but GCC/Clang support:
[[gnu::always_inline]]       // GCC 10+, Clang 13+
[[gnu::flatten]]              // Recursively inline the entire call tree
void aggressive(int x) { }
```

### Inlining Prohibition

```cpp
// GCC / Clang
__attribute__((noinline))
void debug_dump(const Widget& w) {
    // Never inline — used for debug functions, performance-critical isolation points
}

// MSVC
__declspec(noinline) void debug_dump(const Widget& w) { }

// Used for controlling code layout:
__attribute__((noinline, cold))
void error_handler(int code) {
    // Not inline + marked as cold path → compiler places it in .text.unlikely section
}
```

### Practical Usage Patterns

```cpp
// Pattern 1: Force inline on hot path
[[gnu::always_inline]] [[gnu::hot]]
inline uint64_t hash_mix(uint64_t x) {
    x ^= x >> 23;
    x *= 0x2127599bf4325c37ULL;
    x ^= x >> 47;
    return x;
}

// Pattern 2: Mark cold path as noinline
[[gnu::noinline]] [[gnu::cold]]
void slow_path(const char* msg) {
    fprintf(stderr, "ERROR: %s\n", msg);
    abort();
}

// Pattern 3: Profile-guided conditional inlining
// PGO data automatically adjusts thresholds: hot path threshold is raised
// (more aggressive inlining), cold path threshold is lowered (avoid inlining)
```

---

## Recursive Inlining

Compilers have strict limits on inlining recursive functions:

```
Recursive inlining strategy:
  ┌─────────────────────────────────────────────────┐
  │ LLVM default behavior:                          │
  │  · Does not inline recursive calls               │
  │  · But can inline a recursive function into its  │
  │    caller (one level of unrolling, no recursive  │
  │    expansion)                                     │
  │                                                  │
  │ GCC default behavior:                            │
  │  · Same as above                                 │
  │  · -foptimize-sibling-calls optimizes tail       │
  │    recursion into loops                          │
  │                                                  │
  │ Tail recursion optimization:                     │
  │  · Tail call → equivalent to goto                │
  │  · Does not increase stack frame                 │
  │  · Not technically "inlining", but similar effect│
  └─────────────────────────────────────────────────┘
```

```cpp
// Tail recursion → compiler optimizes to loop
int factorial(int n, int acc = 1) {
    if (n <= 1) return acc;
    return factorial(n - 1, n * acc);  // Tail position
}

// Non-tail recursion → cannot optimize to loop
int tree_depth(Node* node) {
    if (!node) return 0;
    return 1 + std::max(tree_depth(node->left),   // Not tail position
                        tree_depth(node->right));
}
```

---

## LTO and Cross-Module Inlining

LTO (Link-Time Optimization) enables inlining across translation unit boundaries:

```
Traditional compilation (without LTO):
  a.cpp → a.o (foo inlined)     ──┐
                                   ├── Link → a.out
  b.cpp → b.o (bar calls foo)   ──┘
  Problem: the call to foo() in bar() cannot be inlined (foo is defined in a.cpp)

LTO compilation:
  a.cpp → a.o (contains LLVM IR)  ──┐
                                      ├── LTO link → cross-module inlining → a.out
  b.cpp → b.o (contains LLVM IR)  ──┘
  With LTO, foo() can be inlined into bar()
```

```bash
# Clang LTO inlining
clang++ -flto=thin -O2 a.cpp b.cpp -o app
# ThinLTO automatically inlines hot-path functions across modules

# GCC LTO inlining
g++ -flto -O2 a.cpp b.cpp -o app
# Full LTO: all modules merged, then unified inline decisions
```

---

## Devirtualization Through Inlining

Inlining is a prerequisite for devirtualization:

```cpp
class Base {
public:
    virtual int compute(int x) = 0;
};

class Derived : public Base {
public:
    int compute(int x) override { return x * 2; }
};

void process(Base& obj) {
    int r = obj.compute(42);  // Virtual call
}
```

```
Before inlining (virtual call):
  %vtable = load ptr, ptr %obj        ; Load vtable pointer
  %vfn = getelementptr ptr, %vtable, 1 ; Get compute's slot
  %fn = load ptr, ptr %vfn             ; Load function pointer
  call %fn(ptr %obj, i32 42)           ; Indirect call

After inlining + devirtualization (if compiler can infer type is Derived):
  ; Virtual call replaced with direct call
  ; Direct call then inlined
  ret i32 84                           ; 42 * 2 = 84
```

---

## Impact of Inlining on Debugging

Inlining breaks the call stack in debug information:

```
-O0 + -g:
  (gdb) bt
  #0  foo(x=42) at test.cpp:5
  #1  bar() at test.cpp:10
  #2  main() at test.cpp:15

-O2 + -g (foo inlined into bar):
  (gdb) bt
  #0  bar() at test.cpp:5    ← foo's line number, but stack frame is bar
  #1  main() at test.cpp:15
  # foo's stack frame "disappeared"
```

```bash
# Debug build recommendations: disable inlining or reduce inline level
clang++ -O1 -g -fno-inline test.cpp     # Disable inlining
clang++ -O2 -g -finline-hint-functions test.cpp  # Only inline functions marked inline

# Release + debug symbols: for profiling (no requirement for exact stack frames)
clang++ -O2 -g -fno-omit-frame-pointer test.cpp

# GCC debug-friendly options
g++ -O2 -g -finline-limit=0 test.cpp    # Disable automatic inlining
g++ -O2 -g -fno-inline test.cpp          # Fully disable
```

---

## Inlining and Code Size

Inlining is a major source of code bloat:

```
Inline bloat illustration:
  foo() 20 instructions, called 10 times
  bar() 30 instructions, called 5 times

  Before inlining: total code = 20 + 30 + call overhead ≈ 60 instructions
  After full inlining: total code = 20×10 + 30×5 = 350 instructions ← 5.8x bloat

  I-Cache impact:
  ┌──────────────────────────────────────────────┐
  │  L1 I-Cache typical size: 32KB (~8K instrs)  │
  │  Working set after bloat may exceed L1 →     │
  │  frequent I-cache misses → execution speed   │
  │  actually decreases                          │
  └──────────────────────────────────────────────┘
```

```bash
# Size-optimizing inline strategies
clang++ -Os test.cpp   # Aggressively optimize for size, very low inline threshold
clang++ -Oz test.cpp   # Extreme size optimization

# View code size changes
size a.out  # Check text section size change

# GCC: control per-translation-unit size growth
g++ -O2 --param inline-unit-growth=10 test.cpp
# Limit code bloat after inlining to 10% (default 30%)
```

---

## Compiler Inline Remarks

```bash
# View all inline decisions
clang++ -O2 -Rpass=inline test.cpp -c
# Example output:
# test.cpp:10:5: remark: 'fast_hash' inlined into 'process' with (cost=0, threshold=225) [-Rpass=inline]

# View reasons for not inlining
clang++ -O2 -Rpass-missed=inline test.cpp -c
# Example output:
# test.cpp:15:5: remark: 'complex_fn' not inlined into 'caller' because too costly to inline (cost=1200, threshold=225) [-Rpass-missed=inline]

# View code changes after inlining
clang++ -O2 -Rpass-analysis=inline test.cpp -c

# GCC equivalent: IPA inline dump
g++ -O2 -fdump-ipa-inline-details test.cpp
```

---

## Inlining Best Practices

```
┌─────────────────────────────────────────────────────────┐
│ DO (Recommended Practices)                              │
│                                                         │
│  · Define small functions in header files (implicit     │
│    inline)                                              │
│  · Use __attribute__((always_inline)) for performance-  │
│    critical small functions (e.g., SIMD intrinsic       │
│    wrappers)                                            │
│  · Use __attribute__((noinline)) for error handling     │
│    paths                                                │
│  · Use -Rpass=inline / -Rpass-missed=inline to audit   │
│    inline decisions                                     │
│  · Use LTO to break translation unit boundaries         │
│  · PGO data helps the compiler distinguish hot/cold     │
│    paths                                                │
│                                                         │
│ DON'T (Practices to Avoid)                              │
│                                                         │
│  · Do not force-inline large functions (> 100           │
│    instructions)                                        │
│  · Do not expect inlining on recursive functions        │
│  · Do not ignore inline fallback under -Os              │
│  · Do not mark virtual functions with always_inline     │
│    (virtual functions are not inlined)                  │
│    unless combined with final/devirtualization          │
└─────────────────────────────────────────────────────────┘
```

---

## Further Reading

- [SROA](/topics/compiler-optimizations/sroa) — Inlining exposes more SROA opportunities
- [Devirtualization](/topics/compiler-optimizations/devirtualization) — Inlining is a prerequisite for devirtualization
- [LTO](/topics/compiler-optimizations/lto) — Cross-module inlining
- [PGO](/topics/compiler-optimizations/pgo) — Profile-guided inline decisions
- [C++ Compiler Optimization Panorama](/topics/compiler-optimizations) — Overall optimization pipeline
- [RAII and Resource Management](/topics/raii) — Inline characteristics of small-object RAII
- [Value Categories Deep Dive](/topics/value-categories-deep-dive) — Rvalue references and inlining interaction
