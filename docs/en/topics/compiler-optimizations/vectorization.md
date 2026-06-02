---
title: "Auto-Vectorization"
topic: topics
feature: compiler-opt-vectorization
standard: C++
status_checked_at: 2026-06-02
---

# Auto-Vectorization

> Vectorization is the technique of replacing scalar operations with SIMD (Single Instruction, Multiple Data) instructions. A single SIMD instruction can process 4 floats (SSE), 8 floats (AVX2), or 16 floats (AVX-512) simultaneously — it is a key lever for modern CPU performance.

---

## Loop Vectorization

The most classic scenario for auto-vectorization is loops: packing element-wise operations into SIMD operations.

```
Source code:
  for (int i = 0; i < n; ++i)
      c[i] = a[i] + b[i];

Scalar execution (1 addition per iteration):
  iter 0: c[0] = a[0] + b[0]
  iter 1: c[1] = a[1] + b[1]
  iter 2: c[2] = a[2] + b[2]
  iter 3: c[3] = a[3] + b[3]
  → 4 loads, 4 adds, 4 stores = 12 instructions

SSE vectorization (4 additions per iteration, 128-bit):
  iter 0-3: [c[0],c[1],c[2],c[3]] = [a[0..3]] + [b[0..3]]
  → 2 loads, 1 addps, 1 store = 4 instructions

AVX2 vectorization (8 additions per iteration, 256-bit):
  iter 0-7: [c[0..7]] = [a[0..7]] + [b[0..7]]
  → 2 loads, 1 vaddps, 1 store = 4 instructions, processing 8 elements
```

### Viewing Vectorization Effects

```bash
# Clang: enable vectorization and view report
clang++ -O2 -Rpass=loop-vectorize test.cpp -c
# test.cpp:5:5: remark: vectorized loop (vectorization width: 4, interleaved count: 2)

# View reasons for vectorization failure
clang++ -O2 -Rpass-missed=loop-vectorize test.cpp -c
# test.cpp:5:5: remark: loop not vectorized: cannot prove it is safe to reorder memory operations

# View vectorization analysis details
clang++ -O2 -Rpass-analysis=loop-vectorize test.cpp -c

# GCC: view vectorization decisions
g++ -O2 -ftree-vectorize -fdump-tree-vect-details test.cpp

# Manually control vectorization width (LLVM)
clang++ -O2 -mllvm -force-vector-width=8 test.cpp
# Force 8-way vectorization (even if cost model advises against it)
```

---

## SLP Vectorization

SLP (Superword Level Parallelism) vectorization does not depend on loops; instead it packs **independent scalar operations within the same basic block**:

```cpp
// Classic SLP vectorization scenario: AOS → SOA transform
struct RGBA { float r, g, b, a; };

void convert(RGBA* pixels, float* r_out, float* g_out,
             float* b_out, float* a_out, int n) {
    for (int i = 0; i < n; ++i) {
        r_out[i] = pixels[i].r;  // These four loads can be SLP-vectorized
        g_out[i] = pixels[i].g;
        b_out[i] = pixels[i].b;
        a_out[i] = pixels[i].a;
    }
}
```

```
Before SLP vectorization (4 independent loads in loop body):
  %r = load float, ptr %p_r
  %g = load float, ptr %p_g
  %b = load float, ptr %p_b
  %a = load float, ptr %p_a

After SLP vectorization (packed into 1 SIMD load):
  %vec = load <4 x float>, ptr %p_rgba
  ; One instruction loads 4 floats
```

```bash
# View SLP vectorization
clang++ -O2 -Rpass=slp-vectorize test.cpp -c
# test.cpp:6:9: remark: SLP vectorized with cost -3 and tree size 4

# Control SLP vectorization
clang++ -O2 -mllvm -slp-vectorize-hor=false test.cpp  # Disable horizontal vectorization
clang++ -O2 -mllvm -slp-max-tree-size=10 test.cpp     # Limit SLP tree size
```

---

## Vectorization Cost Model

The compiler's vectorization decisions are based on a cost model:

```
Vectorization benefit = Scalar cost - Vectorization cost

Scalar cost:
  = N × (load_cost + compute_cost + store_cost)
  where N = iteration count

Vectorization cost:
  = (N/VF) × (vector_load + vector_compute + vector_store + extract_cost)
  + scalar tail handling cost
  where VF = vectorization factor (vector width)

  ┌──────────────────────────────────────────────┐
  │ VF selection factors:                        │
  │  · Target architecture's SIMD width          │
  │  · Data type size                            │
  │  · Operation types in loop body              │
  │  · Loop iteration count (SCEV analysis)      │
  │  · Whether tail handling is needed            │
  │                                              │
  │ Typical VF values:                           │
  │  float + SSE    → VF=4                       │
  │  float + AVX2   → VF=8                       │
  │  float + AVX512 → VF=16                      │
  │  int32 + SSE    → VF=4                       │
  │  int8 + AVX2    → VF=32                      │
  └──────────────────────────────────────────────┘
```

---

## Common Causes of Vectorization Failure

### 1. Data Dependencies

```cpp
// Vectorization failure: loop-carried dependency
for (int i = 1; i < n; ++i) {
    a[i] = a[i-1] + 1;  // Iteration i depends on result of iteration i-1
}
// Error: loop not vectorized: value that could not be identified as reduction
//        is used outside the loop
```

### 2. Aliasing Issues

```cpp
void add(float* a, float* b, float* c, int n) {
    for (int i = 0; i < n; ++i)
        c[i] = a[i] + b[i];
    // Compiler cannot prove a,b,c don't overlap → conservatively does not vectorize
}
```

Solutions:

```cpp
// Solution 1: Use __restrict__ to tell compiler pointers don't alias
void add(float* __restrict__ a, float* __restrict__ b,
         float* __restrict__ c, int n) {
    for (int i = 0; i < n; ++i)
        c[i] = a[i] + b[i];
    // Compiler confirms no aliasing → safe to vectorize
}

// Solution 2: Use #pragma clang loop vectorize(assume_safety)
void add2(float* a, float* b, float* c, int n) {
    #pragma clang loop vectorize(assume_safety)
    for (int i = 0; i < n; ++i)
        c[i] = a[i] + b[i];
}

// Solution 3: OpenMP SIMD hint
#include <omp.h>
void add3(float* a, float* b, float* c, int n) {
    #pragma omp simd
    for (int i = 0; i < n; ++i)
        c[i] = a[i] + b[i];
}
```

### 3. Non-Contiguous Memory Access

```cpp
// Stride != 1 → low vectorization efficiency or failure
void strided(float* a, float* b, int stride, int n) {
    for (int i = 0; i < n; ++i)
        b[i] = a[i * stride];  // Gather operation, high cost
}
```

### 4. Complex Control Flow

```cpp
// Loop with branches: requires masked operations
for (int i = 0; i < n; ++i) {
    if (a[i] > 0)
        b[i] = a[i] * 2;
    else
        b[i] = 0;
}
// AVX-512 supports masked operations → can vectorize
// SSE/AVX2 requires blend + extra cost → cost model may reject
```

### 5. Function Calls

```cpp
// Calling external function in loop body → usually cannot vectorize
for (int i = 0; i < n; ++i)
    a[i] = sin(b[i]);  // sin() is not vectorizable (unless using SVML or similar vector math library)

// Solution: use vector math library
// Clang: -fveclib=SVML (Intel) or -fveclib=LIBMVEC (glibc)
// Or manually use Intel SVML intrinsics
```

---

## SIMD Intrinsics vs Auto-Vectorization

When auto-vectorization is insufficient, compiler intrinsics can be used:

```cpp
#include <immintrin.h>

// Manual SIMD (SSE): full control
void add_manual(float* a, float* b, float* c, int n) {
    for (int i = 0; i < n; i += 4) {
        __m128 va = _mm_loadu_ps(&a[i]);
        __m128 vb = _mm_loadu_ps(&b[i]);
        __m128 vc = _mm_add_ps(va, vb);
        _mm_storeu_ps(&c[i], vc);
    }
}

// AVX2 version
void add_avx2(float* a, float* b, float* c, int n) {
    for (int i = 0; i < n; i += 8) {
        __m256 va = _mm256_loadu_ps(&a[i]);
        __m256 vb = _mm256_loadu_ps(&b[i]);
        __m256 vc = _mm256_add_ps(va, vb);
        _mm256_storeu_ps(&c[i], vc);
    }
}
```

```
Auto-Vectorization vs Manual SIMD:

  ┌───────────────────────────────────────────────────────┐
  │ Auto-Vectorization            Manual SIMD             │
  │                                                       │
  │ ✅ Portable (compiler picks   ❌ Tied to specific ISA │
  │    instruction set)                                   │
  │ ✅ Zero maintenance cost      ❌ Requires manual      │
  │                               maintenance             │
  │ ✅ Compiler handles tail      ❌ Manual tail handling  │
  │    automatically                                      │
  │ ❌ Depends on cost model      ✅ Full control          │
  │    correctness                                        │
  │ ❌ May miss opportunities     ✅ Guaranteed            │
  │                               vectorization           │
  │ ❌ Limited by provability     ✅ Can use special       │
  │                               instructions            │
  │                                                       │
  │ Recommendation: prefer auto-vectorization; only use   │
  │ intrinsics when a confirmed bottleneck exists and     │
  │ auto-vectorization has failed                        │
  └───────────────────────────────────────────────────────┘
```

---

## Target Architecture Control

```bash
# Specify target CPU (affects vector register width and available instruction sets)
clang++ -O2 -march=skylake test.cpp      # AVX2 + BMI2
clang++ -O2 -march=znver3 test.cpp       # AMD Zen3: AVX2
clang++ -O2 -march=sapphirerapids test.cpp  # AVX-512 + AMX

# Generate for all supported ISA versions (multi-version dispatch)
clang++ -O2 -march=x86-64-v3 test.cpp   # Requires AVX2
clang++ -O2 -march=x86-64-v4 test.cpp   # Requires AVX-512

# GCC equivalent
g++ -O2 -march=native test.cpp          # Use all instruction sets of current CPU
g++ -O2 -mavx2 -mfma test.cpp           # Manually enable specific instruction sets

# Runtime dispatch (function multi-versioning)
__attribute__((target_clones("avx2","avx512f","default")))
float dot(float* a, float* b, int n);
// Compiler generates multiple versions, runtime selects optimal version for CPU
```

---

## Vectorization Debugging in Practice

```bash
# Step 1: Compile and view vectorization report
clang++ -O2 -Rpass=loop-vectorize -Rpass-missed=loop-vectorize \
        -Rpass-analysis=loop-vectorize test.cpp -c

# Step 2: View generated assembly
clang++ -O2 -S -masm=intel test.cpp -o test.s
# Search for vmovups, vaddps, vmulpd and other SIMD instructions

# Step 3: Disable vectorization as comparison
clang++ -O2 -fno-vectorize test.cpp -c
# Compare performance difference

# Step 4: View LLVM optimization log
clang++ -O2 -mllvm -debug-only=loop-vectorize test.cpp -c 2>&1
# Outputs detailed vectorization decision process (only in debug builds of opt/clang)

# GCC equivalent
g++ -O2 -ftree-vectorize -fdump-tree-vect-details test.cpp
# .vect file contains detailed vectorization decision log
```

---

## Vectorization and Other Optimization Interactions

```
Optimization ordering and interactions:

  Inlining → Loop unrolling → Loop vectorization → SLP vectorization

  Inlining → exposes loop body → vectorizer sees larger loop body
  Loop unrolling → reduces branch overhead → creates conditions for vectorization
  LICM → hoists loop invariants → reduces complexity of vectorized loop body

  ┌─────────────────────────────────────────────────┐
  │ Common optimization chain:                      │
  │                                                  │
  │ Source:                                          │
  │   for (int i = 0; i < n; ++i)                   │
  │       c[i] = a[i] * scale + offset;             │
  │                                                  │
  │ 1. LICM: hoist scale and offset                 │
  │ 2. Inlining: if a/b/c are accessed via functions│
  │ 3. SROA: eliminate temporary structs             │
  │ 4. Vectorization:                               │
  │    va = load <8 x float> (a + i)                │
  │    vb = va * broadcast(scale)                    │
  │    vc = vb + broadcast(offset)                   │
  │    store <8 x float> (c + i), vc                │
  └─────────────────────────────────────────────────┘
```

---

## Further Reading

- [SROA](/topics/compiler-optimizations/sroa) — SROA eliminates memory accesses for vectorization
- [Inlining](/topics/compiler-optimizations/inlining) — Inlining exposes more vectorization opportunities
- [Alias Analysis](/topics/compiler-optimizations/alias-analysis) — `__restrict__` and vectorization safety
- [PGO](/topics/compiler-optimizations/pgo) — Profile helps vectorization cost model
- [C++ Compiler Optimization Panorama](/topics/compiler-optimizations) — Overall optimization pipeline
- [Performance Optimization](/topics/performance) — SIMD and cache-friendly design
- Intel Intrinsics Guide: https://www.intel.com/content/www/us/en/docs/intrinsics-guide/
