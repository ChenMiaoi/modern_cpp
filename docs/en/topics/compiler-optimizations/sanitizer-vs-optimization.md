---
title: "Sanitizers and Optimization Interaction"
topic: topics
feature: compiler-opt-sanitizer-vs-optimization
standard: C++
status_checked_at: 2026-06-02
---

# Sanitizers and Optimization Interaction

> Sanitizers are the guardians of C++ safety, but they have complex interactions with compiler optimizations. Understanding these interactions is key to using sanitizers correctly — wrong optimization levels can cause false positives or false negatives, and sanitizer instrumentation itself changes the compiler's optimization behavior.

---

## How Sanitizers Work

### ASAN (AddressSanitizer)

```
ASAN memory layout (64-bit Linux):

  Application memory (8 TB)       Shadow memory (16 TB)
  ┌──────────────────┐         ┌──────────────────┐
  │ 0x7fff...        │         │ shadow: 1 byte   │
  │                  │  Maps   │ describes 8 bytes │
  │ Every 8 bytes of │ ──────→ │ of app memory:    │
  │ application      │         │ 0 = all accessible│
  │ memory maps to   │         │ 1-7 = first k     │
  │ 1 byte of shadow │         │   accessible      │
  │ memory           │         │ negative = errors │
  └──────────────────┘         └──────────────────┘

  Red Zone layout:
  ┌──────────┬──────────────────┬──────────┐
  │ red zone │  User-allocated  │ red zone │
  │ (16-128B)│  object          │ (16-128B)│
  └──────────┴──────────────────┴──────────┘
  Out-of-bounds access hits red zone → ASAN detects error
```

```bash
# ASAN compilation command
clang++ -fsanitize=address -g -O1 test.cpp -o test_asan
# ASAN recommends -O1: balance between debuggability and performance

# ASAN runtime options
ASAN_OPTIONS="detect_leaks=1:check_initialization_order=1:strict_init_order=1" ./test_asan

# GCC ASAN
g++ -fsanitize=address -g -O1 test.cpp -o test_asan
```

### TSAN (ThreadSanitizer)

```
How TSAN works:
  ┌─────────────────────────────────────────────────┐
  │ Each memory address is associated with a shadow  │
  │ memory location                                  │
  │ Shadow records the thread that last accessed it, │
  │ access type, and happens-before clock            │
  │                                                  │
  │ On every load/store:                             │
  │ 1. Read the shadow record for that address       │
  │ 2. Compare happens-before relationship with      │
  │    current operation                             │
  │ 3. If data race detected → report                │
  │ 4. Update shadow record                          │
  │                                                  │
  │ Overhead: ~5-15x slower per memory access        │
  └─────────────────────────────────────────────────┘
```

```bash
# TSAN compilation (recommend -O2 or -O1)
clang++ -fsanitize=thread -g -O2 test.cpp -o test_tsan

# TSAN runtime options
TSAN_OPTIONS="history_size=7:second_deadlock_stack=1" ./test_tsan
```

### MSAN (MemorySanitizer)

```
How MSAN works:
  Tracks the "is initialized" state of every bit (origin tracking):

  Every 8-byte application value has an 8-byte shadow:
  shadow[i] = 0 → corresponding 8 bytes are initialized
  shadow[i] ≠ 0 → some bits are uninitialized

  Any operation using an uninitialized value → MSAN reports error
  And traces the source of the uninitialized value (origin chain)
```

```bash
# MSAN compilation (must use -O1 or -O2, cannot use -O0)
clang++ -fsanitize=memory -g -O2 test.cpp -o test_msan

# MSAN requires all dependency libraries to also be compiled with MSAN
# Use msan-instrumented libc++:
clang++ -fsanitize=memory -g -O2 \
        -stdlib=libc++ \
        -L/path/to/msan-libcxx/lib \
        test.cpp -o test_msan
```

### UBSAN (UndefinedBehaviorSanitizer)

```bash
# UBSAN compilation (lowest overhead, can be used in production)
clang++ -fsanitize=undefined -g -O2 test.cpp -o test_ubsan

# Fine-grained control over checks
clang++ -fsanitize=signed-integer-overflow,null,alignment \
        -g -O2 test.cpp -o test_ubsan

# UBSAN can trap instead of printing report (for production use)
clang++ -fsanitize=undefined -fsanitize-trap=all \
        -O2 test.cpp -o test_ubsan
# UB detected → direct SIGABRT, no runtime library needed
```

---

## Recommended Optimization Levels

```
Recommended optimization levels per sanitizer:

  ┌──────────────┬──────────────┬──────────────────────────┐
  │ Sanitizer    │ Recommended  │ Reason                   │
  │              │ -O level     │                          │
  ├──────────────┼──────────────┼──────────────────────────┤
  │ ASAN         │ -O1          │ -O0 is too slow and      │
  │              │              │ ASAN instrumentation may  │
  │              │              │ have false positives at   │
  │              │              │ -O2                       │
  │              │              │ -O1 is the best balance   │
  ├──────────────┼──────────────┼──────────────────────────┤
  │ TSAN         │ -O1 or -O2   │ TSAN tracks memory       │
  │              │              │ access patterns;          │
  │              │              │ optimization doesn't      │
  │              │              │ affect correctness        │
  │              │              │ -O2 reduces TSAN overhead │
  ├──────────────┼──────────────┼──────────────────────────┤
  │ MSAN         │ -O1 or -O2   │ MSAN needs to track      │
  │              │              │ initialization state of   │
  │              │              │ all values                │
  │              │              │ -O0 has more variables →  │
  │              │              │ false positives           │
  ├──────────────┼──────────────┼──────────────────────────┤
  │ UBSAN        │ -O2          │ UBSAN checks UB          │
  │              │              │ semantics; optimization   │
  │              │              │ level doesn't affect      │
  │              │              │ detection capability      │
  │              │              │ -O2 + UBSAN is production │
  │              │              │ ready                     │
  └──────────────┴──────────────┴──────────────────────────┘
```

### Why ASAN Recommends -O1

```
Problems with ASAN and -O2:

  1. Instruction merging:
     -O2 may merge two independent loads into one larger load
     → ASAN shadow checks are skipped
     → False negatives (missed detections)

  2. Dead code elimination:
     -O2 may eliminate checks that ASAN marks as "redundant"
     → Some out-of-bounds accesses go undetected

  3. Loop optimizations:
     -O2's loop unrolling may change memory access patterns
     → ASAN's red zone boundaries may be crossed

  Practical impact:
  · -O1 + ASAN: highest detection rate, fewest false positives
  · -O2 + ASAN: works in most cases, but has edge cases
  · -O0 + ASAN: complete detection, but extremely slow (10-20x)
```

---

## Sanitizer Impact on Inlining

```
How ASAN instrumentation affects inlining:

  Original code:
    void foo(int* p) {
        *p = 42;    // 1 store instruction
    }

  After ASAN instrumentation:
    void foo(int* p) {
        // Check if p is in a red zone
        shadow_addr = (addr >> 3) + offset;
        shadow_val = load shadow_addr;
        if (shadow_val != 0) {
            // Check if specific byte is accessible
            report_error(addr);
        }
        *p = 42;    // Actual store
    }
  // Function body grows from 1 instruction to ~10 instructions
  // → Inline threshold may no longer be met → some functions won't be inlined

  Impact chain:
  ASAN instrumentation → function body bloat → less inlining → fewer optimization opportunities
```

```bash
# Force ASAN + inlining
clang++ -fsanitize=address -O1 -mllvm -inline-threshold=500 test.cpp
# Increase inline threshold to compensate for ASAN instrumentation overhead

# View ASAN's impact on inline decisions
clang++ -fsanitize=address -O1 -Rpass=inline test.cpp -c
clang++ -fsanitize=address -O1 -Rpass-missed=inline test.cpp -c
```

---

## Sanitizer Impact on Vectorization

```
Why ASAN instrumentation blocks vectorization:

  Original loop:
    for (int i = 0; i < n; ++i)
        c[i] = a[i] + b[i];

  After ASAN instrumentation (pseudocode):
    for (int i = 0; i < n; ++i) {
        check_shadow(&a[i]);     // ASAN check
        check_shadow(&b[i]);     // ASAN check
        check_shadow(&c[i]);     // ASAN check
        c[i] = a[i] + b[i];
    }

  Problems:
  · ASAN checks contain conditional branches → loop body becomes complex
  · check_shadow is not vectorizable
  · Vectorizer sees complex loop body → gives up vectorization

  TSAN is worse:
  · Every load/store must update shadow memory
  · Contains atomic operations (thread-safe shadow updates)
  · Completely blocks auto-vectorization
```

```bash
# ASAN + vectorization (generally not recommended, but can try)
clang++ -fsanitize=address -O2 -mllvm -force-vector-width=4 test.cpp
# Force vectorization, ASAN checks execute outside vectorized loop

# Check if vectorization is blocked by ASAN
clang++ -fsanitize=address -O2 -Rpass-missed=loop-vectorize test.cpp -c
```

---

## Debug vs Release Sanitizer Usage

### Debug Configuration

```bash
# Debug + all sanitizers
clang++ -O0 -g -fsanitize=address,undefined \
        -fno-omit-frame-pointer \
        test.cpp -o test_debug

# Debug + TSAN (cannot be used simultaneously with ASAN)
clang++ -O1 -g -fsanitize=thread \
        -fno-omit-frame-pointer \
        test.cpp -o test_tsan_debug

# Framework:
# ┌──────────────────────────────────────────────┐
# │ Unit tests → ASAN + UBSAN                     │
# │ Integration tests → ASAN + UBSAN +            │
# │                     LeakSanitizer             │
# │ Concurrency tests → TSAN                      │
# │ Initialization tests → MSAN (requires full    │
# │                         toolchain MSAN build)  │
# └──────────────────────────────────────────────┘
```

### Release Configuration

```bash
# Release + UBSAN (low overhead, usable in production)
clang++ -O2 -fsanitize=undefined -fsanitize-trap=all \
        -fno-omit-frame-pointer \
        test.cpp -o test_release_ubsan

# Release + ASAN (for staging environment)
clang++ -O1 -fsanitize=address \
        -fno-omit-frame-pointer \
        test.cpp -o test_release_asan

# Do not use TSAN/MSAN in production (too much overhead)
# UBSAN with -fsanitize-trap=all is the only production-ready option
```

---

## Sanitizer Combination Rules

```
Can be used simultaneously:        Cannot be used simultaneously:
  ASAN + UBSAN ✅                    ASAN + TSAN ❌
  ASAN + LSan  ✅                    ASAN + MSAN ❌
  TSAN + UBSAN ✅                    TSAN + MSAN ❌
  MSAN + UBSAN ✅

  LSan (LeakSanitizer):
  · ASAN includes LSan by default
  · Standalone use: -fsanitize=leak
  · Can be disabled at runtime: LSAN_OPTIONS="detect_leaks=0"
```

```bash
# ASAN + UBSAN (most common combination)
clang++ -O1 -g -fsanitize=address,undefined \
        -fno-omit-frame-pointer \
        test.cpp -o test_both

# Configure both runtime options simultaneously
ASAN_OPTIONS="detect_leaks=1:abort_on_error=1" \
UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1" \
./test_both
```

---

## Sanitizers and LTO

```
Sanitizer + LTO considerations:

  1. ASAN + LTO:
     · ASAN instrumentation happens in the frontend → LTO still works
     · But ASAN's shadow memory accesses cannot be optimized away by LTO
     · Recommendation: use -O1 with ASAN; LTO benefits are limited in ASAN builds

  2. TSAN + LTO:
     · TSAN's happens-before tracking crosses modules
     · LTO's cross-module inlining may change the access patterns TSAN sees
     · Generally safe, but verify TSAN reports are consistent with and without LTO

  3. UBSAN + LTO:
     · UBSAN with -fsanitize-trap=all has almost zero additional overhead
     · Fully compatible with LTO
     · Recommended for production builds
```

```bash
# ASAN + ThinLTO (for debugging)
clang++ -flto=thin -fsanitize=address -O1 -g test.cpp -o test_asan_lto

# UBSAN + LTO (for production)
clang++ -flto=thin -fsanitize=undefined -fsanitize-trap=all \
        -O2 test.cpp -o test_ubsan_lto
```

---

## Sanitizer Impact on Compile Time

```
Sanitizer compile time overhead:

  Configuration                      Compile Time (relative)
  ────────────────────────────────────────────
  -O2 (baseline)                    1.0x
  -O2 -fsanitize=undefined          1.1x
  -O2 -fsanitize=address            1.3x
  -O1 -fsanitize=address            1.0x (because -O1 itself is faster)
  -O2 -fsanitize=thread             1.2x
  -O2 -fsanitize=memory             1.3x
```

---

## Practical: Sanitizer CI Configuration

```yaml
# CI matrix example (conceptual)
# Build matrix:
#   Release:     -O2
#   Debug+ASAN:  -O1 -g -fsanitize=address,undefined
#   Debug+TSAN:  -O1 -g -fsanitize=thread
#   Release+UBSAN: -O2 -fsanitize=undefined -fsanitize-trap=all

# CMake integration
# cmake -DCMAKE_BUILD_TYPE=ASAN ..
# In CMakeLists.txt:
#   set(CMAKE_C_FLAGS_ASAN "-O1 -g -fsanitize=address -fno-omit-frame-pointer")
#   set(CMAKE_CXX_FLAGS_ASAN "-O1 -g -fsanitize=address -fno-omit-frame-pointer")
```

```cmake
# CMake sanitizer support
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

if(ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()

if(ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
endif()
```

---

## Further Reading

- [Inlining](/topics/compiler-optimizations/inlining) — Sanitizer instrumentation affects inline decisions
- [Vectorization](/topics/compiler-optimizations/vectorization) — Sanitizers block loop vectorization
- [LTO](/topics/compiler-optimizations/lto) — Sanitizer + LTO considerations
- [C++ Compiler Optimization Panorama](/topics/compiler-optimizations) — Overall optimization pipeline
- [Toolchain and Ecosystem](/topics/toolchain) — Sanitizer usage in the toolchain
- [Performance Optimization](/topics/performance) — Quantifying sanitizer overhead
- LLVM Sanitizer documentation: https://github.com/google/sanitizers/wiki
- ASAN documentation: https://clang.llvm.org/docs/AddressSanitizer.html
