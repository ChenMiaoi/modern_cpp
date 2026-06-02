---
title: "Link-Time Optimization (LTO)"
topic: topics
feature: compiler-opt-lto
standard: C++
status_checked_at: 2026-06-02
---

# Link-Time Optimization (LTO)

> LTO breaks the boundaries of translation units, allowing the compiler to see the entire program's IR at link time and perform global optimizations such as cross-module inlining, devirtualization, and dead code elimination. It is a standard tool in modern C++ performance engineering.

---

## Why LTO Is Needed

Limitations of the traditional compilation model:

```
Traditional compilation (without LTO):
  a.cpp ──compile──→ a.o (machine code)  ──┐
                                             ├── Link → app
  b.cpp ──compile──→ b.o (machine code)  ──┘

  Problems:
  · Compiler can only see code within a single translation unit
  · Functions in a.cpp cannot be inlined into b.cpp
  · Dead code in a.o is not eliminated (linker only operates at symbol level)
  · Cross-module type information is lost → cannot devirtualize

LTO compilation:
  a.cpp ──compile──→ a.o (LLVM IR / GIMPLE)  ──┐
                                                  ├── LTO → global optimization → app
  b.cpp ──compile──→ b.o (LLVM IR / GIMPLE)  ──┘

  Advantages:
  · Cross-module inlining
  · Cross-module devirtualization
  · Global dead code elimination
  · Cross-module constant propagation
```

---

## Full LTO vs ThinLTO

### Full LTO

```
Full LTO workflow:
  ┌─────────────────────────────────────────────────┐
  │ 1. All .o files (containing IR) are merged into │
  │    one large module                              │
  │    LLVM: llvm-link a.o b.o → merged.bc          │
  │    GCC: merged by lto1 at link time              │
  │                                                  │
  │ 2. Run the full optimization pipeline on the     │
  │    merged module                                 │
  │    · Global inlining                            │
  │    · Global dead code elimination               │
  │    · Global constant propagation                │
  │                                                  │
  │ 3. Generate final machine code                  │
  └─────────────────────────────────────────────────┘

  Pros: Best optimization results (sees all code)
  Cons:
    · Long compile times (O(N) module size)
    · High memory usage (all IR resides in memory)
    · Cannot optimize in parallel
```

### ThinLTO

```
ThinLTO workflow:
  ┌─────────────────────────────────────────────────┐
  │ Phase 1: Module Summary / Summary Index         │
  │  · Each .o retains only summary information     │
  │    (function signatures, call graph, type info)  │
  │  · Does not retain full IR                      │
  │  · Summary is much smaller than full IR         │
  │    (typically < 10%)                            │
  └──────────────────────┬──────────────────────────┘
                         │ Summary index
                         ▼
  ┌─────────────────────────────────────────────────┐
  │ Phase 2: Global Decisions (Import / Cross-Module│
  │          Analysis)                               │
  │  · Compute cross-module inline decisions based   │
  │    on summary                                   │
  │  · Determine which functions need to be imported │
  │    from other modules                           │
  │  · Determine dead code and symbol visibility    │
  └──────────────────────┬──────────────────────────┘
                         │ Import list
                         ▼
  ┌─────────────────────────────────────────────────┐
  │ Phase 3: Parallel Backend                        │
  │  · Each module is optimized independently        │
  │  · Only imports needed functions (not entire     │
  │    modules)                                      │
  │  · Fully parallelizable (utilizes multiple cores)│
  └──────────────────────┬──────────────────────────┘
                         │
                         ▼
  ┌─────────────────────────────────────────────────┐
  │ Phase 4: Link                                   │
  │  · Link optimized .o files into final executable│
  └─────────────────────────────────────────────────┘

  Pros:
    · Fast compile times (parallel optimization, O(1) per-module overhead)
    · Low memory usage (each module processed independently)
    · Incremental compilation friendly (only re-optimizes modified modules)

  Cons:
    · Cross-module information is less complete than Full LTO
    · Some cross-module optimizations (e.g., global constant propagation)
      are slightly less effective
```

---

## Compilation Commands

### Clang / LLVM

```bash
# ThinLTO (recommended)
clang++ -flto=thin -O2 a.cpp b.cpp -o app

# Full LTO
clang++ -flto=full -O2 a.cpp b.cpp -o app

# Step-by-step compilation (compile IR first, then link-optimize)
clang++ -flto=thin -O2 -c a.cpp -o a.o    # a.o contains LLVM bitcode
clang++ -flto=thin -O2 -c b.cpp -o b.o    # b.o contains LLVM bitcode
clang++ -flto=thin -O2 a.o b.o -o app     # LTO link

# View ThinLTO import decisions
clang++ -flto=thin -O2 -Wl,--plugin-opt=thinlto-index-only a.o b.o -o app
# Only generates index file, doesn't actually link

# Control ThinLTO import limits
clang++ -flto=thin -O2 -mllvm -thinlto-import-instr-limit=100 a.o b.o -o app
# Upper limit on imported instructions (default 100)
```

### GCC

```bash
# Full LTO
g++ -flto -O2 a.cpp b.cpp -o app

# With fat LTO objects (contains both IR and machine code)
g++ -ffat-lto-objects -flto -O2 -c a.cpp -o a.o
# a.o contains both LTO IR and machine code, can use regular linker

# ThinLTO (GCC 10+)
g++ -flto=auto -O2 a.cpp b.cpp -o app
# Automatically selects thread count

# Control LTO parallelism
g++ -flto=4 -O2 a.cpp b.cpp -o app   # Use 4 threads
```

---

## ThinLTO Import Mechanism

ThinLTO's core is **on-demand importing** — only importing functions needed for inline decisions:

```
Module A (a.cpp):                   Module B (b.cpp):
  void process() {                  inline int helper(int x) {
      int r = helper(42);               return x * 2 + 1;
      ...                            }
  }

ThinLTO decision:
  1. Analyze summary: A calls B::helper()
  2. Evaluate: helper is small and frequently called → worth inlining
  3. Import: import helper's IR from B into A
  4. Inline: inline helper into process() in A
  5. Subsequent optimization: constant propagation → helper(42) = 85
```

```
Import limits:
  ┌─────────────────────────────────────────────────┐
  │ thinlto-import-instr-limit (default 100)         │
  │  · Each module imports at most 100 instructions  │
  │    worth of functions                            │
  │  · Prevents importing large functions which      │
  │    would cause compile time explosion            │
  │                                                  │
  │ Adjustable:                                      │
  │  · Raise limit → better cross-module optimization│
  │    but slower compilation                        │
  │  · Lower limit → faster compilation, slightly    │
  │    less effective optimization                   │
  │                                                  │
  │ PGO data assists:                                │
  │  · Hot functions automatically get higher import │
  │    limits                                        │
  │  · Cold functions are not imported               │
  └─────────────────────────────────────────────────┘
```

---

## Cross-Module Inlining

LTO's most direct benefit is cross-module inlining:

```cpp
// utils.cpp
namespace utils {
    inline int fast_hash(int x) {  // Even if marked inline, no cross-module inlining without LTO
        x = ((x >> 16) ^ x) * 0x45d9f3b;
        x = ((x >> 16) ^ x) * 0x45d9f3b;
        x = (x >> 16) ^ x;
        return x;
    }
}

// main.cpp
#include "utils.h"
void process(std::vector<int>& data) {
    for (auto& x : data)
        x = utils::fast_hash(x);  // Can be inlined with LTO
}
```

```bash
# Without LTO: fast_hash is a function call
clang++ -O2 main.cpp utils.cpp -o app_no_lto

# With LTO: fast_hash is inlined, loop may be vectorized
clang++ -flto=thin -O2 main.cpp utils.cpp -o app_lto

# Compare performance
hyperfine ./app_no_lto ./app_lto
```

---

## Dead Code Elimination

LTO can eliminate dead code across modules:

```bash
# View LTO dead code elimination effect
# Use nm to count symbols
nm app_no_lto | wc -l     # Symbol count without LTO
nm app_lto | wc -l         # Symbol count with LTO (typically fewer)

# GCC LTO dead code elimination
g++ -flto -O2 main.cpp unused.cpp -o app
# Uncalled functions in unused.cpp are eliminated during LTO phase
```

---

## String Merging

LTO can merge identical string constants across modules:

```
Without LTO:
  a.o: .rodata: "error: invalid input"  (independent copy)
  b.o: .rodata: "error: invalid input"  (independent copy)
  After linking: two copies of the same string

With LTO:
  Optimizer finds two identical strings → merges into one
  → Saves .rodata section space
```

---

## CMake Integration

```cmake
# CMake LTO enablement

# Method 1: Global enablement
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

# Method 2: Per-target enablement
add_executable(app main.cpp utils.cpp)
set_property(TARGET app PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

# Method 3: Check if compiler supports LTO
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported)
if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# Method 4: Best practice for Ninja + ThinLTO
# CMakeLists.txt
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto=thin")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto=thin")
```

```bash
# Build commands
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
ninja -j$(nproc)  # ThinLTO can optimize in parallel
```

---

## LTO and Debug Information

LTO affects the debugging experience:

```
LTO's impact on debug information:
  ┌─────────────────────────────────────────────────┐
  │ Problems:                                       │
  │  · After cross-module inlining, stack frames    │
  │    may "disappear"                              │
  │  · LTO may delete unused functions → breakpoints│
  │    become ineffective                           │
  │  · Type information may be incomplete after     │
  │    import                                       │
  │                                                  │
  │ Solutions:                                      │
  │  · Use -flto=thin -g (preserve debug info)      │
  │  · ThinLTO preserves each module's independent  │
  │    debug info                                   │
  │  · Full LTO merges all debug info → larger but  │
  │    more complete                                │
  │  · Use -fno-lto to preserve debug build         │
  └─────────────────────────────────────────────────┘
```

```bash
# LTO + debug information
clang++ -flto=thin -O2 -g main.cpp utils.cpp -o app_debug

# Split debug information
clang++ -flto=thin -O2 -g -gsplit-dwarf main.cpp utils.cpp -o app
# .dwo files contain debug info, reducing executable size
```

---

## Code Size Impact

LTO's impact on code size goes both ways:

```
LTO size impact:
  ┌─────────────────────────────────────────────────┐
  │ Size reduction:                                  │
  │  · Cross-module dead code elimination            │
  │  · String constant merging                       │
  │  · Global inlining followed by constant          │
  │    propagation → eliminates more code            │
  │                                                  │
  │ Size increase:                                   │
  │  · Cross-module inlining → function body copies  │
  │  · More aggressive inline decisions              │
  │                                                  │
  │ Typical net effect: -O2 + LTO size is close to   │
  │ -O2 without LTO                                 │
  │ -Os + LTO typically reduces size                │
  └─────────────────────────────────────────────────┘
```

```bash
# Compare code size
clang++ -O2 main.cpp utils.cpp -o app_no_lto && size app_no_lto
clang++ -flto=thin -O2 main.cpp utils.cpp -o app_lto && size app_lto

# View specific section sizes
llvm-size --totals app_lto
```

---

## LTO Build Time Overhead

```
Compile time comparison (typical project, ~100 translation units):

  Mode             Compile Time   Link Time   Total
  ─────────────────────────────────────────────
  -O2 no LTO      30s            2s          32s
  -O2 ThinLTO     32s            8s          40s   (+25%)
  -O2 FullLTO     32s            45s         77s   (+140%)

  ThinLTO link time increase comes from:
  · Module summary generation
  · Import decision computation
  · Parallel backend optimization

  Full LTO link time increase comes from:
  · All IR merged into a single module
  · Global optimization (single-threaded)
```

---

## Further Reading

- [Inlining](/topics/compiler-optimizations/inlining) — LTO's core benefit is cross-module inlining
- [Devirtualization](/topics/compiler-optimizations/devirtualization) — LTO enables whole-program devirtualization
- [PGO](/topics/compiler-optimizations/pgo) — PGO + LTO = best performance combination
- [C++ Compiler Optimization Panorama](/topics/compiler-optimizations) — Overall optimization pipeline
- [Toolchain and Ecosystem](/topics/toolchain) — Build system configuration
- LLVM ThinLTO design document: https://clang.llvm.org/docs/ThinLTO.html
