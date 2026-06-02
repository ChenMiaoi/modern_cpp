---
title: "SROA: Scalar Replacement of Aggregates"
topic: topics
feature: compiler-opt-sroa
standard: C++
status_checked_at: 2026-06-02
---

# SROA: Scalar Replacement of Aggregates

> SROA is a top-three critical pass in the LLVM optimization pipeline. It decomposes aggregate types (structs, arrays) from `alloca` into independent scalar SSA registers, serving as the bridge between "value semantics in source code" and "efficient register-level code."

---

## What SROA Does

SROA's core task: **decompose a struct in memory into a set of independent scalar variables so they can be promoted to SSA registers by `mem2reg`**.

```
Before optimization (LLVM IR from frontend):
  %p = alloca { i32, i32 }
  store { i32, i32 } { i32 10, i32 20 }, ptr %p
  %x_ptr = getelementptr inbounds { i32, i32 }, ptr %p, i32 0, i32 0
  %y_ptr = getelementptr inbounds { i32, i32 }, ptr %p, i32 0, i32 1
  %x = load i32, ptr %x_ptr
  %y = load i32, ptr %y_ptr
  %sum = add i32 %x, %y

After SROA + mem2reg:
  ; %p's alloca is completely eliminated
  ; x, y become pure SSA values, constant propagation further folds
  ret i32 30
```

SROA is a generalization of `mem2reg`:
- `mem2reg`: handles individual scalar alloca → SSA
- SROA: first decomposes aggregates into multiple scalar allocas, then hands them to `mem2reg`

---

## When SROA Triggers

SROA activates when all of the following conditions are met:

```
┌─────────────────────────────────────────────────────────┐
│ SROA Prerequisites                                      │
│                                                         │
│  1. The alloca type is an aggregate (struct/array/      │
│     vector)                                             │
│  2. Access patterns to the aggregate are "analyzable"   │
│     · GEP offsets are constants                         │
│     · No cross-field load/store                         │
│  3. The alloca's address does not escape to:            │
│     · Functions that cannot be inlined                  │
│     · Global variables                                  │
│     · Inline assembly operands                          │
│     · volatile load/store accesses                      │
└─────────────────────────────────────────────────────────┘
```

### Simple Case

```cpp
struct Point { int x, y; };

int sum(Point p) {
    return p.x + p.y;
}
```

View IR before and after SROA:

```bash
# Generate unoptimized IR
clang++ -O0 -Xclang -disable-O0-optnone -emit-llvm -S sroa.cpp -o sroa_before.ll

# Run only the SROA pass
opt -passes=sroa sroa_before.ll -S -o sroa_after.ll

# GCC equivalent: view FRE/PRE optimization effect
g++ -O1 -fdump-tree-fre sroa.cpp
```

In the IR before SROA, `Point p` is an `alloca` of type `{ i32, i32 }`, and all accesses to `p.x` and `p.y` go through GEP + load/store. After SROA, the `alloca` is split into two independent `i32` allocas, and then `mem2reg` promotes them to SSA registers.

---

## How SROA Works

SROA's algorithm has three steps:

```
Step 1: Analyze
  ┌─────────────────────────────────────────────┐
  │ Traverse all uses of the alloca             │
  │ · GEP access → record offset and size       │
  │ · Whole-block memcpy → record source/target  │
  │ · If unanalyzable access found → mark as    │
  │   non-optimizable                           │
  └─────────────────────────────────────────────┘
          │
          ▼
Step 2: Partition / Slice
  ┌─────────────────────────────────────────────┐
  │ Split the aggregate into multiple "slices"  │
  │ based on access patterns                    │
  │ · { i32, i32 } → slice [0,4) and [4,8)     │
  │ · If a field is never accessed → mark as    │
  │   dead slice                                │
  └─────────────────────────────────────────────┘
          │
          ▼
Step 3: Rewrite
  ┌─────────────────────────────────────────────┐
  │ Replace each slice with an independent      │
  │ alloca                                      │
  │ · GEP + load → load from new alloca         │
  │ · GEP + store → store to new alloca         │
  │ · Delete original alloca                    │
  │ · Delegate to mem2reg to promote new alloca │
  │   to SSA                                    │
  └─────────────────────────────────────────────┘
```

### Partial Decomposition

SROA does not require all fields to be analyzable. Even if some field is accessed indirectly (e.g., through a pointer), as long as other fields meet the conditions, SROA will perform partial decomposition:

```cpp
struct Mixed {
    int simple;     // Only accessed via p.simple
    int complex;    // Accessed indirectly through int* pointer
};

int partial(Mixed m) {
    int* ptr = &m.complex;
    *ptr = 42;
    return m.simple + m.complex;
}
```

SROA will decompose the `simple` field into an independent alloca, but `complex` remains in the original alloca because its address escapes to `ptr`.

---

## SROA and NRVO Interaction

NRVO (Named Return Value Optimization) eliminates return value copies at the frontend level, and SROA further handles residual aggregates at the subsequent IR level:

```cpp
struct Result { int code; double value; };

Result compute() {
    Result r;
    r.code = 0;
    r.value = 3.14;
    return r;
}
```

```
When NRVO takes effect:
  Caller provides return address → compute() directly constructs r at that address
  → SROA has nothing to do (no alloca)

When NRVO does not take effect (e.g., multiple return paths):
  compute() contains %r = alloca { i32, double }
  → SROA decomposes it into two independent allocas
  → Each promoted to SSA registers
  → Finally stored to the return address via store
```

**Key point**: NRVO eliminates copies across function boundaries, while SROA eliminates aggregate memory accesses within a function. They are complementary, not conflicting.

---

## Scenarios Where SROA Fails

### Address Escape

```cpp
struct Config { int timeout; int retries; };

void init(Config* cfg);  // External function, invisible to compiler

Config make_config() {
    Config c;
    c.timeout = 30;
    c.retries = 3;
    init(&c);       // c's address escapes → SROA cannot decompose
    return c;
}
```

If `init` is inlined and the address no longer escapes after inlining, SROA can take effect again after inlining. This is why SROA runs again after inlining in the pass ordering.

### Volatile Access

```cpp
struct HW { uint32_t status; uint32_t data; };
volatile HW* regs = /* MMIO address */;

uint32_t read_data() {
    return regs->data;  // volatile → compiler cannot decompose or eliminate access
}
```

Volatile guarantees access ordering and count. SROA cannot alter the semantics of volatile load/store.

### Inline Assembly Constraints

```cpp
struct Vec { float x, y, z, w; };

void asm_use(Vec v) {
    asm volatile("" : : "x"(v.x), "y"(v.y));  // Register constraints bind fields
    // Inline assembly references struct fields through constraints
    // SROA may not be able to determine whether the assembly accesses
    // the entire struct through other means
}
```

Inline assembly is a black box to the compiler. If assembly constraints reference the aggregate's address (rather than its value), SROA must conservatively preserve the original alloca.

### Cross-Field memcpy

```cpp
struct Big { char data[4096]; };

void copy(Big* dst, const Big* src) {
    *dst = *src;  // Whole-block assignment → compiler generates memcpy
}
```

SROA generally does not decompose whole-block memcpy of large structs — decomposition would produce thousands of individual load/store instructions, which is slower than a single memcpy.

---

## Equivalent Optimization in GCC

GCC does not have a pass named SROA, but has functionally equivalent optimization chains:

```
GCC optimization chain:
  FRE (Full Redundancy Elimination)
    → Eliminates redundant loads
  PRE (Partial Redundancy Elimination)
    → Eliminates partially redundant memory accesses
  DSE (Dead Store Elimination)
    → Eliminates dead stores
  phiprop
    → PHI node propagation

These passes collaboratively accomplish work similar to SROA, but are not as thorough as LLVM SROA.
```

```bash
# View GCC optimization effects
g++ -O2 -fdump-tree-all sroa.cpp
# Focus on .fre, .pre, .dse files
```

---

## Advanced Topic: SROA and ABI

SROA's decomposition affects the function's ABI representation. When a struct is fully decomposed into scalars, the backend may choose register passing instead of stack passing:

```
Before SROA (ABI perspective):
  define void @foo(ptr %out) {
    %tmp = alloca { i32, i32 }
    ; Entire struct is in memory
  }

After SROA + scalar propagation:
  define i32 @bar() {
    ret i32 30
    ; No memory accesses at all, result returned via register
  }
```

The performance impact is significant: a function call changes from "push to stack → call → read from stack → return" to "pass via register → return directly."

---

## Viewing SROA at Work

```bash
# LLVM: print IR before and after SROA
clang++ -O2 -mllvm -print-after=sroa -mllvm -print-module-scope sroa.cpp -c 2>&1

# LLVM: use opt tool for single-step execution
opt -passes='sroa,mem2reg' input.ll -S

# GCC: view struct scalarization
g++ -O2 -fdump-tree-optimized sroa.cpp
# View .optimized file to confirm whether struct accesses were eliminated

# Compiler remarks: view SROA decisions
clang++ -O2 -Rpass=sroa sroa.cpp -c
# or
clang++ -O2 -Rpass-missed=sroa sroa.cpp -c
```

---

## Performance Impact

SROA's performance impact is mainly reflected in:

```
┌─────────────────────────────────────────────────────────┐
│ Direct Benefits                                         │
│  · Eliminates load/store → reduces memory accesses      │
│  · SSA registers → enables downstream constant          │
│    propagation, CSE, dead code elimination               │
│  · Register passing → reduces stack operations          │
│                                                         │
│ Indirect Benefits (downstream optimizations enabled by   │
│ SROA)                                                   │
│  · Global Value Numbering (GVN) can merge identical     │
│    scalar computations                                  │
│  · Loop optimizations can analyze register-only loop    │
│    dependencies                                         │
│  · Instruction scheduling has more freedom              │
│                                                         │
│ Potential Negative Impact                               │
│  · Decomposing very large structs may increase register │
│    pressure                                             │
│  · Partial decomposition may cause additional           │
│    spill/fill                                           │
└─────────────────────────────────────────────────────────┘
```

In actual benchmarks, SROA almost always produces positive or neutral results. Disabling SROA (`-mllvm -enable-sroa=false`) typically causes 5-20% performance regression.

---

## Further Reading

- [Inlining](/topics/compiler-optimizations/inlining) — Inlining creates opportunities for SROA
- [Alias Analysis](/topics/compiler-optimizations/alias-analysis) — Alias information helps SROA determine address escape
- [LTO](/topics/compiler-optimizations/lto) — Cross-module SROA
- [C++ Compiler Optimization Panorama](/topics/compiler-optimizations) — Overall optimization pipeline
- [Value Categories Deep Dive](/topics/value-categories-deep-dive) — Value semantics and SROA relationship
- [Performance Optimization](/topics/performance) — SROA's role in practical performance tuning
- LLVM official documentation: [SROA Pass](https://llvm.org/docs/Passes.html#sroa-scalar-replacement-of-aggregates)
