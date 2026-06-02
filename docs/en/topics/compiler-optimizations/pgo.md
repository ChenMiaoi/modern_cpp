---
title: "Profile-Guided Optimization (PGO)"
topic: topics
feature: compiler-opt-pgo
standard: C++
status_checked_at: 2026-06-02
---

# Profile-Guided Optimization (PGO)

> PGO lets the compiler make optimization decisions based on real runtime data — which branches are more likely to execute, which functions are on hot paths, which code is rarely reached. This is far more accurate than static heuristics and typically yields 5-20% performance improvement.

---

## PGO Workflow

```
PGO three-phase workflow:

  Phase 1: Instrumentation Build
  ┌─────────────────────────────────────────────────┐
  │ Source → Compile (-fprofile-generate) →          │
  │ Instrumented executable                          │
  │                                                  │
  │ Instrumented code records at runtime:            │
  │  · Execution count of each branch                │
  │  · Call count of each function                   │
  │  · Execution frequency of each basic block       │
  │  · Indirect call targets (for devirtualization)  │
  └──────────────────────┬──────────────────────────┘
                         │ Run instrumented program
                         ▼
  Phase 2: Collect Profile Data
  ┌─────────────────────────────────────────────────┐
  │ Run instrumented program (with representative    │
  │ workload)                                        │
  │                                                  │
  │ GCC: generates .gcda files                       │
  │ LLVM: generates .profraw files                   │
  │                                                  │
  │ ⚠️ Critical: workload must be representative!     │
  │  · Do not use unit tests as profile input        │
  │  · Should use actual production workload         │
  └──────────────────────┬──────────────────────────┘
                         │ Process profile data
                         ▼
  Phase 3: Optimized Build
  ┌─────────────────────────────────────────────────┐
  │ Source + profile data → Compile (-fprofile-use)  │
  │                                                  │
  │ Compiler uses profile data to:                   │
  │  · Optimize branch layout (compact hot paths)    │
  │  · Guide inline decisions (aggressively inline   │
  │    hot functions)                                │
  │  · Function layout (group hot functions together)│
  │  · Conditional devirtualization                  │
  │  · Function splitting (hot/cold separation)      │
  └─────────────────────────────────────────────────┘
```

---

## LLVM PGO

### Step 1: Instrumentation Build

```bash
# Generate instrumented version
clang++ -fprofile-generate -O2 main.cpp utils.cpp -o app_instrumented

# Instrument to specified directory
clang++ -fprofile-generate=/tmp/pgo_profiles -O2 main.cpp -o app_instrumented

# Instrumented executable size increases ~15-30%
# Runtime speed decreases ~10-30% (depending on branch density)
```

### Step 2: Collect Profile

```bash
# Run instrumented program
./app_instrumented --real-workload

# Generates .profraw files (default in current directory)
# Or in the directory specified by -fprofile-generate=DIR

# If there are multiple .profraw files (e.g., multi-process, test suite)
llvm-profdata merge -output=merged.profdata *.profraw

# View profile contents
llvm-profdata show merged.profdata
# Output: function call counts, branch frequencies, etc.

# View specific function's profile
llvm-profdata show --function=process merged.profdata
```

### Step 3: Optimized Build

```bash
# Compile using profile data
clang++ -fprofile-use=merged.profdata -O2 main.cpp utils.cpp -o app_optimized

# If profile files are at different paths, need remapping
clang++ -fprofile-use=merged.profdata \
        -fprofile-remapping-file=remap.txt \
        -O2 main.cpp -o app_optimized

# View PGO-guided optimization decisions
clang++ -fprofile-use=merged.profdata -O2 \
        -Rpass=pgo-instrumentation \
        -Rpass=inline main.cpp -c
```

---

## GCC PGO

```bash
# Step 1: Instrumentation build
g++ -fprofile-generate -O2 main.cpp utils.cpp -o app_instrumented

# Step 2: Collect profile
./app_instrumented --real-workload
# Generates .gcda files

# Step 3: Optimized build
g++ -fprofile-use -O2 main.cpp utils.cpp -o app_optimized

# Automatic sampling (no instrumented run needed)
g++ -fprofile-generate -fprofile-update=atomic -O2 main.cpp -o app_instrumented
# atomic update mode: thread-safe, with slight additional overhead

# View GCC's PGO optimization effects
g++ -fprofile-use -O2 -fdump-tree-profile_estimate main.cpp
```

---

## Profile Data Format Conversion

```bash
# AutoFDO toolchain: perf → LLVM profile
# Step 1: Sample with perf (no instrumentation needed)
perf record -b -e cycles:u ./app --real-workload
# -b: record branch information (LBR, requires Intel Haswell+)

# Step 2: Convert to LLVM profile format
create_llvm_prof --binary=./app --out=profile.afdo perf.data

# Step 3: Compile with AutoFDO profile
clang++ -fprofile-sample-use=profile.afdo -O2 main.cpp -o app_afdo

# GCC AutoFDO
create_gcov --binary=./app --gcov=profile.gcov --profile=perf.data
g++ -fauto-profile=profile.gcov -O2 main.cpp -o app_afdo
```

---

## Branch Prediction Hints

PGO's most direct optimization is branch layout — placing hot paths in contiguous memory addresses:

```cpp
void process(int input) {
    if (is_normal(input)) {       // PGO: 99% takes this branch
        fast_path(input);
    } else {                      // PGO: 1% takes this branch
        slow_path(input);
    }
}
```

```
Code layout without PGO:
  ┌───────────────────────────────────────────────┐
  │ Code section:                                 │
  │   LBB0: is_normal(input)                      │
  │   LBB1: fast_path(input)   ; default fallthrough│
  │   LBB2: jmp end                               │
  │   LBB3: slow_path(input)   ; jump target      │
  │   LBB4: end                                   │
  └───────────────────────────────────────────────┘

  Code layout with PGO:
  ┌───────────────────────────────────────────────┐
  │ .text.hot (hot section):                      │
  │   LBB0: is_normal(input)                      │
  │   LBB1: fast_path(input)                      │
  │   LBB2: end                                   │
  │                                               │
  │ .text.unlikely (cold section):                │
  │   LBB3: slow_path(input)                      │
  └───────────────────────────────────────────────┘

  Effect:
  · Hot path code is compact → better I-cache utilization
  · Cold path separated → doesn't pollute hot path cache lines
  · Branch prediction more accurate (fallthrough is always faster than taken branch)
```

---

## Function Splitting

PGO can separate a function's hot and cold parts into different sections:

```cpp
void handle_request(Request& req) {
    validate(req);           // Hot path (99%)
    process(req);            // Hot path
    if (unlikely_error()) {  // Cold path (<1%)
        log_error(req);
        retry(req);
        cleanup(req);
    }
    respond(req);            // Hot path
}
```

```
After function splitting:
  .text.hot:
    handle_request.hot:    ; Contains only hot path code
      validate()
      process()
      respond()

  .text.unlikely:
    handle_request.cold:   ; Cold path code placed separately
      log_error()
      retry()
      cleanup()
```

```bash
# LLVM function splitting
clang++ -fprofile-use=merged.profdata -O2 \
        -mllvm -hot-cold-split=true main.cpp

# GCC function splitting (-freorder-blocks-and-partition)
g++ -fprofile-use -O2 -freorder-blocks-and-partition main.cpp
```

---

## Function Layout Optimization

PGO can also optimize the ordering of functions — grouping hot functions together and placing cold functions at the end:

```
Function layout without PGO (in compilation order):
  .text:  main() → validate() → error_handler() → process() → retry()
  Problem: error_handler and retry are cold functions, sandwiched between hot functions

Function layout with PGO:
  .text.hot: main() → process() → validate()   ← hot functions grouped
  .text:     parse_args()                       ← neutral functions
  .text.unlikely: error_handler() → retry()     ← cold functions grouped

  Effect:
  · Hot functions have adjacent addresses → TLB and I-cache friendly
  · Reduces I-cache line waste on cold code
```

```bash
# View function layout
nm --numeric-sort app_optimized | grep ' T '  # Functions sorted by address
# Or use perf tools to view I-cache utilization
perf stat -e L1-icache-load-misses ./app_optimized
```

---

## AutoFDO (Sampling-Based PGO)

AutoFDO does not require instrumentation; instead it uses hardware performance counter sampling:

```
Traditional PGO vs AutoFDO:

  Traditional PGO:
    Instrumented compile → Run → .gcda/.profraw → Optimized compile
    ✅ Precise counts
    ❌ Requires additional instrumented run
    ❌ Instrumentation has ~10-30% performance overhead

  AutoFDO:
    Normal compile → perf sampling → Convert profile → Optimized compile
    ✅ Zero instrumentation overhead (sample directly in production)
    ✅ Uses real production workload
    ❌ Sampling has noise (slightly lower precision)
    ❌ Requires debug info (-g) for source location mapping
```

```bash
# AutoFDO complete workflow
# 1. Compile (needs debug info + optimization)
clang++ -O2 -g main.cpp utils.cpp -o app

# 2. Sample in production environment
perf record -b -e cycles:u -o perf.data -- ./app --production-load
# -b: record branch information (Branch Stack / LBR)
# -e cycles:u: only sample user-mode

# 3. Convert profile
create_llvm_prof \
    --binary=./app \
    --profile=perf.data \
    --out=profile.afdo \
    --format=extbinary

# 4. Recompile with AutoFDO profile
clang++ -fprofile-sample-use=profile.afdo -O2 -g \
        main.cpp utils.cpp -o app_optimized
```

---

## BOLT: Post-Link Optimizer

BOLT (Binary Optimization and Layout Tool) performs layout optimization at the binary level, independent of the compiler:

```
BOLT workflow:
  Normal compile + link → Executable
                         │
                         ▼
  perf record -b → perf.data
                         │
                         ▼
  llvm-bolt app -data perf.data -o app_bolt
                         │
                         ▼
  Optimized executable

  What BOLT does:
  · Function reordering (profile-based hot function grouping)
  · Basic block reordering (hot path contiguous)
  · Cold code separation
  · Indirect call optimization
  · Intra-function basic block reordering
```

```bash
# BOLT complete workflow
# 1. Compile (needs relocations)
clang++ -O2 -Wl,--emit-relocs main.cpp -o app
# or
clang++ -O2 -Wl,-q main.cpp -o app

# 2. Sample
perf record -e cycles:u -j any,u -o perf.data -- ./app

# 3. Convert perf data to BOLT format
perf2bolt -p perf.data -o perf.fdata ./app

# 4. Run BOLT
llvm-bolt ./app -o ./app_bolt \
    -data=perf.fdata \
    -reorder-blocks=ext-tsp \
    -reorder-functions=hfsort+ \
    -split-functions \
    -split-all-cold
```

---

## PGO + LTO Combination

PGO + LTO is the optimal performance combination:

```bash
# LLVM: PGO + ThinLTO
clang++ -flto=thin -fprofile-generate -O2 a.cpp b.cpp -o app_instrumented
./app_instrumented --workload
llvm-profdata merge -output=default.profdata *.profraw
clang++ -flto=thin -fprofile-use=default.profdata -O2 a.cpp b.cpp -o app_final

# GCC: PGO + LTO
g++ -flto -fprofile-generate -O2 a.cpp b.cpp -o app_instrumented
./app_instrumented --workload
g++ -flto -fprofile-use -O2 a.cpp b.cpp -o app_final
```

```
Synergy of PGO + LTO:
  ┌─────────────────────────────────────────────────┐
  │ PGO knows which functions are hot               │
  │ LTO can inline across modules                   │
  │                                                  │
  │ Combined effect:                                │
  │  · Hot path cross-module calls are aggressively │
  │    inlined                                       │
  │  · Cold path function calls are preserved (not   │
  │    inlined, to avoid bloat)                      │
  │  · Whole-program optimization + Profile-guided = │
  │    optimal decisions                             │
  │                                                  │
  │ Typical gains:                                  │
  │  · PGO alone:     +5-15%                        │
  │  · LTO alone:     +3-10%                        │
  │  · PGO + LTO:     +10-25%                       │
  └─────────────────────────────────────────────────┘
```

---

## Further Reading

- [Inlining](/topics/compiler-optimizations/inlining) — PGO data guides inline decisions
- [Devirtualization](/topics/compiler-optimizations/devirtualization) — PGO supports speculative devirtualization
- [LTO](/topics/compiler-optimizations/lto) — PGO + LTO combination
- [Vectorization](/topics/compiler-optimizations/vectorization) — PGO helps vectorization cost model
- [C++ Compiler Optimization Panorama](/topics/compiler-optimizations) — Overall optimization pipeline
- [Performance Optimization](/topics/performance) — PGO in practical projects
- LLVM AutoFDO: https://github.com/google/autofdo
- BOLT: https://github.com/llvm/llvm-project/tree/main/bolt
