# 性能优化

## 移动语义与避免拷贝

移动语义允许将资源所有权从一个对象转移到另一个，避免昂贵的深拷贝：

```cpp
std::vector<int> create_large_vector() {
    std::vector<int> v(1000000);
    std::iota(v.begin(), v.end(), 0);
    return v;  // NRVO 或移动，O(1)
}

// 按值接收 — 调用方可选择移动或拷贝
void process(std::vector<int> data) { /* ... */ }
process(create_large_vector());        // 移动
process(std::move(my_vec));            // 移动

// 完美转发 — 保持值类别
template <typename T>
void wrapper(T&& arg) { target(std::forward<T>(arg)); }

// ⚠️ 不要在返回值上用 std::move — 会阻碍 NRVO
std::string make_string() {
    std::string s = "hello";
    return s; // ✅ 允许编译器优化（NRVO）
}
```

## 小字符串优化（SSO）

大多数标准库对 `std::string` 使用 SSO：短字符串（通常 ≤ 15-22 字节）直接存储在对象内部，避免堆分配。

```cpp
std::string short_str = "hello";  // 零堆分配（SSO 缓冲区）
std::string long_str(100, 'x');   // 堆分配一次
// GCC: empty capacity = 15, libc++: 22
```

SSO 影响：短字符串操作快（无堆分配），但 `sizeof(std::string)` 较大（libc++ 为 24 字节，libstdc++/MSVC 为 32 字节），且移动短字符串可能需要拷贝（数据内联）。

## 缓存友好的数据结构

CPU 缓存对性能的影响往往远超算法复杂度。O(n) 遍历连续内存通常比 O(log n) 遍历分散节点快得多。

```cpp
// 缓存不友好 — 链表节点分散在堆上
struct ListNode { int value; ListNode* next; };

// SoA（Structure of Arrays）— SIMD 和缓存预取最友好
struct Particles { std::vector<float> x, y, z; };
// 只需要 x 时填满整个缓存行；AoS 浪费 2/3 空间

// C++23: flat_map — 排序 vector 替代红黑树
#include <flat_map>
std::flat_map<int, std::string> fm; // 小数据集快数倍

// 对象池 — 避免碎片化分配
template <typename T, std::size_t BlockSize = 4096>
class ObjectPool {
    struct Block { alignas(T) char data[sizeof(T) * BlockSize]; };
    std::vector<std::unique_ptr<Block>> blocks_;
    std::size_t used_ = 0;
public:
    template <typename... Args>
    T* allocate(Args&&... args) {
        if (used_ >= BlockSize * blocks_.size())
            blocks_.push_back(std::make_unique<Block>());
        auto* block = blocks_.back().get();
        auto offset = used_ % BlockSize;
        ++used_;
        return new (block->data + offset * sizeof(T)) T(std::forward<Args>(args)...);
    }
};
```

## 分支预测提示

C++20 的 `[[likely]]`/`[[unlikely]]` 帮助编译器优化分支布局：

```cpp
int process(int value) {
    if (value >= 0) [[likely]] { return value * 2; }
    else [[unlikely]] { throw std::out_of_range("negative"); }
}

// GCC/Clang 传统方式
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
```

影响代码布局（热/冷路径分离），不控制条件移动指令生成。

## SIMD 向量化（实验性 TS / C++26 草案）

> **注意**：`std::simd` 已被纳入 C++26 草案（P1928），但截至 2024 年主流编译器仍以实验性 TS 形式提供（`<experimental/simd>`）。下方示例使用 TS 命名空间；待编译器正式支持后可切换为 `std::simd`。

```cpp
#include <experimental/simd>
namespace stdx = std::experimental;

void vector_add(const float* a, const float* b, float* out, std::size_t n) {
    using V = stdx::native_simd<float>; // 4(SSE)/8(AVX)/16(AVX-512)
    constexpr auto w = V::size();
    std::size_t i = 0;
    for (; i + w <= n; i += w) {
        // ⚠️ vector_aligned 要求指针满足 native_simd 的对齐要求
        // 实际使用时需确保 a/b/out 按 V::alignment() 对齐分配
        V va(a + i, stdx::vector_aligned);
        V vb(b + i, stdx::vector_aligned);
        (va + vb).copy_to(out + i, stdx::vector_aligned);
    }
    for (; i < n; ++i) out[i] = a[i] + b[i]; // 标量尾部处理
}
```

编译示例（GCC 14+）：`g++ -std=c++26 -O3 -march=native -I/path/to/experimental/simd`

## 分配器感知容器

```cpp
// C++17 PMR 分配器
#include <memory_resource>
char buffer[4096];
std::pmr::monotonic_buffer_resource pool{buffer, sizeof(buffer)};
std::pmr::vector<int> vec{&pool};  // 零系统调用

// 线程局部复用避免热路径分配
thread_local std::string buf;
void hot_path(const char* input) {
    buf.clear(); buf.append(input);
}
```

## 基准测试工具

```cpp
// Google Benchmark — 业界标准
#include <benchmark/benchmark.h>
static void BM_PushBack(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<int> v;
        for (int i = 0; i < state.range(0); ++i) v.push_back(i);
        benchmark::DoNotOptimize(v.data());
    }
}
BENCHMARK(BM_PushBack)->Range(8, 1 << 20);
```

- **quick-bench.com**：在线对比基准测试
- **perf**（Linux）：`perf stat -e cache-misses,branch-misses ./bench`
- **Tracy Profiler**：实时帧级分析，跨平台

## Agner Fog 指导原则

```cpp
struct alignas(64) CacheLineAligned { float data[16]; }; // 缓存行对齐

// __restrict__ 提示编译器可安全向量化
void add(float* __restrict__ a, const float* __restrict__ b, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) a[i] += b[i];
}
```

**核心纪律：先测量，后优化。** 没有基准测试数据，任何优化都是猜测。
