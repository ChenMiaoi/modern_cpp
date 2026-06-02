---
title: "自动向量化"
topic: topics
feature: compiler-opt-vectorization
standard: C++
status_checked_at: 2026-06-02
---

# 自动向量化（Auto-Vectorization）

> 向量化是将标量操作替换为 SIMD（Single Instruction, Multiple Data）指令的技术。一条 SIMD 指令可以同时处理 4 个 float（SSE）、8 个 float（AVX2）或 16 个 float（AVX-512），是现代 CPU 性能的关键杠杆。

---

## 循环向量化

自动向量化最经典的场景是循环：将逐元素操作打包为 SIMD 操作。

```
源码：
  for (int i = 0; i < n; ++i)
      c[i] = a[i] + b[i];

标量执行（每次 1 个加法）：
  iter 0: c[0] = a[0] + b[0]
  iter 1: c[1] = a[1] + b[1]
  iter 2: c[2] = a[2] + b[2]
  iter 3: c[3] = a[3] + b[3]
  → 4 次 load, 4 次 add, 4 次 store = 12 条指令

SSE 向量化（每次 4 个加法，128-bit）：
  iter 0-3: [c[0],c[1],c[2],c[3]] = [a[0..3]] + [b[0..3]]
  → 2 次 load, 1 次 addps, 1 次 store = 4 条指令

AVX2 向量化（每次 8 个加法，256-bit）：
  iter 0-7: [c[0..7]] = [a[0..7]] + [b[0..7]]
  → 2 次 load, 1 次 vaddps, 1 次 store = 4 条指令，处理 8 个元素
```

### 查看向量化效果

```bash
# Clang：启用向量化并查看报告
clang++ -O2 -Rpass=loop-vectorize test.cpp -c
# test.cpp:5:5: remark: vectorized loop (vectorization width: 4, interleaved count: 2)

# 查看向量化失败的原因
clang++ -O2 -Rpass-missed=loop-vectorize test.cpp -c
# test.cpp:5:5: remark: loop not vectorized: cannot prove it is safe to reorder memory operations

# 查看向量化分析详情
clang++ -O2 -Rpass-analysis=loop-vectorize test.cpp -c

# GCC：查看向量化决策
g++ -O2 -ftree-vectorize -fdump-tree-vect-details test.cpp

# 手动控制向量化宽度（LLVM）
clang++ -O2 -mllvm -force-vector-width=8 test.cpp
# 强制使用 8 路向量化（即使代价模型不建议）
```

---

## SLP 向量化

SLP（Superword Level Parallelism）向量化不依赖循环，而是对**同一基本块中的独立标量操作**进行打包：

```cpp
// SLP 向量化的经典场景：AOS → SOA 变换
struct RGBA { float r, g, b, a; };

void convert(RGBA* pixels, float* r_out, float* g_out,
             float* b_out, float* a_out, int n) {
    for (int i = 0; i < n; ++i) {
        r_out[i] = pixels[i].r;  // 这四个 load 可以 SLP 向量化
        g_out[i] = pixels[i].g;
        b_out[i] = pixels[i].b;
        a_out[i] = pixels[i].a;
    }
}
```

```
SLP 向量化前（循环体内的 4 个独立 load）：
  %r = load float, ptr %p_r
  %g = load float, ptr %p_g
  %b = load float, ptr %p_b
  %a = load float, ptr %p_a

SLP 向量化后（打包为 1 个 SIMD load）：
  %vec = load <4 x float>, ptr %p_rgba
  ; 一条指令加载 4 个 float
```

```bash
# 查看 SLP 向量化
clang++ -O2 -Rpass=slp-vectorize test.cpp -c
# test.cpp:6:9: remark: SLP vectorized with cost -3 and tree size 4

# 控制 SLP 向量化
clang++ -O2 -mllvm -slp-vectorize-hor=false test.cpp  # 禁止水平向量化
clang++ -O2 -mllvm -slp-max-tree-size=10 test.cpp     # 限制 SLP 树大小
```

---

## 向量化代价模型

编译器的向量化决策基于代价模型（Cost Model）：

```
向量化收益 = 标量代价 - 向量化代价

标量代价：
  = N × (load_cost + compute_cost + store_cost)
  其中 N = 迭代次数

向量化代价：
  = (N/VF) × (vector_load + vector_compute + vector_store + extract_cost)
  + 标量尾部处理代价
  其中 VF = vectorization factor（向量宽度）

  ┌──────────────────────────────────────────────┐
  │ VF 选择因素：                                 │
  │  · 目标架构的 SIMD 宽度                       │
  │  · 数据类型大小                               │
  │  · 循环体中的操作类型                         │
  │  · 循环迭代次数（SCEV 分析）                 │
  │  · 是否需要尾部处理                           │
  │                                               │
  │ 典型 VF 值：                                  │
  │  float + SSE  → VF=4                          │
  │  float + AVX2 → VF=8                          │
  │  float + AVX512 → VF=16                       │
  │  int32 + SSE → VF=4                           │
  │  int8 + AVX2 → VF=32                          │
  └──────────────────────────────────────────────┘
```

---

## 向量化失败的常见原因

### 1. 数据依赖

```cpp
// 向量化失败：循环携带依赖
for (int i = 1; i < n; ++i) {
    a[i] = a[i-1] + 1;  // 第 i 次迭代依赖第 i-1 次的结果
}
// 错误信息：loop not vectorized: value that could not be identified as reduction is used outside the loop
```

### 2. 别名问题

```cpp
void add(float* a, float* b, float* c, int n) {
    for (int i = 0; i < n; ++i)
        c[i] = a[i] + b[i];
    // 编译器无法证明 a,b,c 不重叠 → 保守地不向量化
}
```

解决方案：

```cpp
// 方案 1：使用 __restrict__ 告诉编译器指针不别名
void add(float* __restrict__ a, float* __restrict__ b,
         float* __restrict__ c, int n) {
    for (int i = 0; i < n; ++i)
        c[i] = a[i] + b[i];
    // 编译器确认无别名 → 安全向量化
}

// 方案 2：使用 #pragma clang loop vectorize(assume_safety)
void add2(float* a, float* b, float* c, int n) {
    #pragma clang loop vectorize(assume_safety)
    for (int i = 0; i < n; ++i)
        c[i] = a[i] + b[i];
}

// 方案 3：OpenMP SIMD hint
#include <omp.h>
void add3(float* a, float* b, float* c, int n) {
    #pragma omp simd
    for (int i = 0; i < n; ++i)
        c[i] = a[i] + b[i];
}
```

### 3. 非连续内存访问

```cpp
// 步长不为 1 → 向量化效率低或失败
void strided(float* a, float* b, int stride, int n) {
    for (int i = 0; i < n; ++i)
        b[i] = a[i * stride];  // gather 操作，代价高
}
```

### 4. 控制流复杂

```cpp
// 带分支的循环：需要 masked 操作
for (int i = 0; i < n; ++i) {
    if (a[i] > 0)
        b[i] = a[i] * 2;
    else
        b[i] = 0;
}
// AVX-512 支持 masked 操作 → 可以向量化
// SSE/AVX2 需要 blend + 额外开销 → 代价模型可能拒绝
```

### 5. 函数调用

```cpp
// 循环体中调用外部函数 → 通常无法向量化
for (int i = 0; i < n; ++i)
    a[i] = sin(b[i]);  // sin() 不可向量化（除非用 SVML 等向量数学库）

// 解决：使用向量数学库
// Clang: -fveclib=SVML (Intel) 或 -fveclib=LIBMVEC (glibc)
// 或手动使用 Intel SVML intrinsics
```

---

## SIMD 内建函数 vs 自动向量化

当自动向量化不够时，可以使用编译器内建函数（intrinsics）：

```cpp
#include <immintrin.h>

// 手动 SIMD（SSE）：完全控制
void add_manual(float* a, float* b, float* c, int n) {
    for (int i = 0; i < n; i += 4) {
        __m128 va = _mm_loadu_ps(&a[i]);
        __m128 vb = _mm_loadu_ps(&b[i]);
        __m128 vc = _mm_add_ps(va, vb);
        _mm_storeu_ps(&c[i], vc);
    }
}

// AVX2 版本
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
自动向量化 vs 手动 SIMD：

  ┌───────────────────────────────────────────────────────┐
  │ 自动向量化                      手动 SIMD             │
  │                                                       │
  │ ✅ 可移植（编译器选择指令集）  ❌ 绑定特定 ISA        │
  │ ✅ 零维护成本                  ❌ 需要手动维护        │
  │ ✅ 编译器自动处理尾部          ❌ 需要手动处理尾部    │
  │ ❌ 依赖代价模型正确性          ✅ 完全控制            │
  │ ❌ 可能漏掉机会                ✅ 保证向量化          │
  │ ❌ 受限于可证明性              ✅ 可以利用特殊指令    │
  │                                                       │
  │ 建议：优先让编译器自动向量化；只在确认瓶颈且          │
  │       自动向量化失败时才使用 intrinsics              │
  └───────────────────────────────────────────────────────┘
```

---

## 目标架构控制

```bash
# 指定目标 CPU（影响向量寄存器宽度和可用指令集）
clang++ -O2 -march=skylake test.cpp      # AVX2 + BMI2
clang++ -O2 -march=znver3 test.cpp       # AMD Zen3: AVX2
clang++ -O2 -march=sapphirerapids test.cpp  # AVX-512 + AMX

# 生成所有支持的 ISA 版本（多版本分发）
clang++ -O2 -march=x86-64-v3 test.cpp   # 要求 AVX2
clang++ -O2 -march=x86-64-v4 test.cpp   # 要求 AVX-512

# GCC 等价
g++ -O2 -march=native test.cpp          # 使用当前 CPU 的全部指令集
g++ -O2 -mavx2 -mfma test.cpp           # 手动启用特定指令集

# 运行时分发（函数多版本化）
__attribute__((target_clones("avx2","avx512f","default")))
float dot(float* a, float* b, int n);
// 编译器生成多个版本，运行时根据 CPU 选择最优版本
```

---

## 向量化调试实战

```bash
# 第 1 步：编译并查看向量化报告
clang++ -O2 -Rpass=loop-vectorize -Rpass-missed=loop-vectorize \
        -Rpass-analysis=loop-vectorize test.cpp -c

# 第 2 步：查看生成的汇编
clang++ -O2 -S -masm=intel test.cpp -o test.s
# 搜索 vmovups, vaddps, vmulpd 等 SIMD 指令

# 第 3 步：禁用向量化作为对比
clang++ -O2 -fno-vectorize test.cpp -c
# 比较性能差异

# 第 4 步：查看 LLVM 优化日志
clang++ -O2 -mllvm -debug-only=loop-vectorize test.cpp -c 2>&1
# 输出向量化的详细决策过程（仅 debug build 的 opt/clang）

# GCC 等价
g++ -O2 -ftree-vectorize -fdump-tree-vect-details test.cpp
# .vect 文件包含详细的向量化决策日志
```

---

## 向量化与其他优化的交互

```
优化顺序与交互：

  内联 → 循环展开 → 循环向量化 → SLP 向量化

  内联 → 暴露循环体 → 向量化器看到更大的循环体
  循环展开 → 减少分支开销 → 为向量化创造条件
  LICM → 循环不变量外提 → 减少向量化循环体的复杂度

  ┌─────────────────────────────────────────────────┐
  │ 常见优化链：                                     │
  │                                                  │
  │ 源码：                                           │
  │   for (int i = 0; i < n; ++i)                   │
  │       c[i] = a[i] * scale + offset;             │
  │                                                  │
  │ 1. LICM：scale 和 offset 外提                   │
  │ 2. 内联：若 a/b/c 通过函数访问                  │
  │ 3. SROA：消除临时结构体                          │
  │ 4. 向量化：                                       │
  │    va = load <8 x float> (a + i)                │
  │    vb = va * broadcast(scale)                    │
  │    vc = vb + broadcast(offset)                   │
  │    store <8 x float> (c + i), vc                │
  └─────────────────────────────────────────────────┘
```

---

## 延伸阅读

- [SROA](/topics/compiler-optimizations/sroa) — SROA 为向量化消除内存访问
- [内联](/topics/compiler-optimizations/inlining) — 内联暴露更多向量化机会
- [别名分析](/topics/compiler-optimizations/alias-analysis) — `__restrict__` 与向量化安全
- [PGO](/topics/compiler-optimizations/pgo) — Profile 帮助向量化代价模型
- [C++ 编译器优化全景](/topics/compiler-optimizations) — 整体优化管线
- [性能优化](/topics/performance) — SIMD 与缓存友好设计
- Intel Intrinsics Guide：https://www.intel.com/content/www/us/en/docs/intrinsics-guide/
