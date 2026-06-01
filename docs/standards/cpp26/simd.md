# C++26 std::simd

## 概述

`std::simd` 是 C++26 的可移植 SIMD 类型库（`<simd>`），提供固定/原生宽度向量类型，使向量化代码可跨平台复用，无需平台特定 intrinsics。

**提案状态：** P1928R15 已被 C++26 接受。

## 基本类型

```cpp
#include <simd>
#include <print>

// 固定宽度
using float4 = std::simd<float, std::simd_abi::fixed_size<4>>;
// 原生宽度（硬件最优）
using native_float = std::simd<float>;

int main() {
    float4 a{1.0f, 2.0f, 3.0f, 4.0f};
    std::println("size = {}", a.size());  // 4
    for (std::size_t i = 0; i < a.size(); ++i)
        std::print("{} ", a[i]);          // 1 2 3 4
}
```

| ABI 类型 | 说明 |
|----------|------|
| `native` | 硬件最优（默认） |
| `fixed_size<N>` | 固定 N 个元素 |
| `compatible` | 所有目标均可用 |

## 加载与存储

```cpp
std::vector<float> data{1,2,3,4,5,6,7,8};
using float4 = std::simd<float, std::simd_abi::fixed_size<4>>;

float4 a(&data[0], std::simd_flag_aligned);   // 从内存加载
float4 c(3.0f);                                // 广播标量

alignas(float4) float result[4];
a.copy_to(&result[0], std::simd_flag_aligned); // 存储
```

## 算术运算

所有运算按元素并行执行：

```cpp
using float4 = std::simd<float, std::simd_abi::fixed_size<4>>;
float4 a{1.0f, 2.0f, 3.0f, 4.0f};
float4 b{4.0f, 3.0f, 2.0f, 1.0f};

float4 sum  = a + b;          // {5, 5, 5, 5}
float4 prod = a * b;          // {4, 6, 6, 4}
float4 mins = std::min(a, b); // {1, 2, 2, 1}

float total = std::reduce(sum);  // 归约: 20.0
auto mask = a > b;               // simd_mask: {F,F,T,T}
```

## 掩码操作

```cpp
using float4 = std::simd<float, std::simd_abi::fixed_size<4>>;
float4 a{1.0f, -2.0f, 3.0f, -4.0f};

auto negative = a < float4(0.0f);   // {F,T,F,T}

float4 abs_a = where(negative, -a, a);   // {1, 2, 3, 4}
float4 clamped = where(a > float4(0.0f), a, float4(0.0f)); // {1, 0, 3, 0}
```

## 向量化循环

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

### 数组求和

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

## 与 Intrinsics 对比

```cpp
// SSE intrinsics — 平台绑定
__m128 acc = _mm_setzero_ps();
for (int i = 0; i + 4 <= n; i += 4)
    acc = _mm_add_ps(acc, _mm_loadu_ps(data + i));

// std::simd — 可移植
float_v acc(0.0f);
for (std::size_t i = 0; i + float_v::size() <= n; i += float_v::size())
    acc += float_v(data + i, std::simd_flag_unaligned);
```

| 维度 | Intrinsics | `std::simd` |
|------|-----------|-------------|
| 可移植性 | 平台绑定 | 统一抽象 |
| 向量宽度 | 硬编码 | 自适应 |
| 掩码操作 | 手动管理 | `where()` 原生 |

## 实现状态

| 编译器 | 状态 |
|--------|------|
| GCC | 跟进中，`std::experimental::simd` 在 libstdc++ 可用 |
| Clang / MSVC | 跟进中 |

生产环境可先用 `std::experimental::simd` 过渡。

## 总结

`std::simd` 提供标准化可移植 SIMD 抽象，通过 `simd`/`simd_mask`/`where`/归约操作覆盖常见向量化场景，比平台 intrinsics 更具可移植性和可读性。
