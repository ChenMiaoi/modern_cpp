---
title: C++26 std::simd
status_checked_at: 2026-06-01
topic: unknown
feature: simd
standard: N/A
---


# C++26 std::simd

## Overview

`std::simd` is C++26's portable SIMD type library (`<simd>`), providing fixed/native-width vector types that enable vectorized code to be reused across platforms without platform-specific intrinsics.

**Proposal status:** P1928R15 has been accepted for C++26.

## Basic Types

```cpp
#include <simd>
#include <print>

// Fixed width
using float4 = std::simd<float, std::simd_abi::fixed_size<4>>;
// Native width (hardware-optimal)
using native_float = std::simd<float>;

int main() {
    float4 a{1.0f, 2.0f, 3.0f, 4.0f};
    std::println("size = {}", a.size());  // 4
    for (std::size_t i = 0; i < a.size(); ++i)
        std::print("{} ", a[i]);          // 1 2 3 4
}
```

| ABI Type | Description |
|----------|-------------|
| `native` | Hardware-optimal (default) |
| `fixed_size<N>` | Fixed N elements |
| `compatible` | Available on all targets |

## Load and Store

```cpp
std::vector<float> data{1,2,3,4,5,6,7,8};
using float4 = std::simd<float, std::simd_abi::fixed_size<4>>;

float4 a(&data[0], std::simd_flag_aligned);   // Load from memory
float4 c(3.0f);                                // Broadcast scalar

alignas(float4) float result[4];
a.copy_to(&result[0], std::simd_flag_aligned); // Store
```

## Arithmetic Operations

All operations execute element-wise in parallel:

```cpp
using float4 = std::simd<float, std::simd_abi::fixed_size<4>>;
float4 a{1.0f, 2.0f, 3.0f, 4.0f};
float4 b{4.0f, 3.0f, 2.0f, 1.0f};

float4 sum  = a + b;          // {5, 5, 5, 5}
float4 prod = a * b;          // {4, 6, 6, 4}
float4 mins = std::min(a, b); // {1, 2, 2, 1}

float total = std::reduce(sum);  // Reduction: 20.0
auto mask = a > b;               // simd_mask: {F,F,T,T}
```

## Mask Operations

```cpp
using float4 = std::simd<float, std::simd_abi::fixed_size<4>>;
float4 a{1.0f, -2.0f, 3.0f, -4.0f};

auto negative = a < float4(0.0f);   // {F,T,F,T}

float4 abs_a = where(negative, -a, a);   // {1, 2, 3, 4}
float4 clamped = where(a > float4(0.0f), a, float4(0.0f)); // {1, 0, 3, 0}
```

## Vectorized Loops

### SAXPY

```cpp
void saxpy(float a, float const* x, float const* y, float* out, std::size_t n) {
    using float_v = std::simd<float>;
    constexpr std::size_t W = float_v::size();
    std::size_t i = 0;
    for (; i + W <= n; i += W) {
        float_v xv(x + i, std::simd_flag_aligned);
        float_v yv(y + i, std::simd_flag_aligned);
        (a * xv + yv).copy_to(out + i, std::simd_flag_aligned);
    }
    for (; i < n; ++i) out[i] = a * x[i] + y[i];
}
```

### Array Sum

```cpp
float simd_sum(float const* data, std::size_t n) {
    using float_v = std::simd<float>;
    float_v acc(0.0f);
    std::size_t i = 0;
    for (; i + float_v::size() <= n; i += float_v::size())
        acc += float_v(data + i, std::simd_flag_overaligned<alignof(float_v)>);
    float total = std::reduce(acc);
    for (; i < n; ++i) total += data[i];
    return total;
}
```

## Comparison with Intrinsics

```cpp
// SSE intrinsics — platform-bound
__m128 acc = _mm_setzero_ps();
for (int i = 0; i + 4 <= n; i += 4)
    acc = _mm_add_ps(acc, _mm_loadu_ps(data + i));

// std::simd — portable
float_v acc(0.0f);
for (std::size_t i = 0; i + float_v::size() <= n; i += float_v::size())
    acc += float_v(data + i, std::simd_flag_unaligned);
```

| Dimension | Intrinsics | `std::simd` |
|-----------|-----------|-------------|
| Portability | Platform-bound | Unified abstraction |
| Vector width | Hardcoded | Adaptive |
| Mask operations | Manual management | Native `where()` |

## Implementation Status

| Compiler | Status |
|----------|--------|
| GCC | In progress, `std::experimental::simd` available in libstdc++ |
| Clang / MSVC | In progress |

For production environments, `std::experimental::simd` can serve as an interim solution.

## Summary

`std::simd` provides a standardized portable SIMD abstraction, covering common vectorization scenarios through `simd`/`simd_mask`/`where`/reduction operations, offering better portability and readability than platform intrinsics.
