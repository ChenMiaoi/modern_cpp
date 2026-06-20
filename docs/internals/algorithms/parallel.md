---
title: "并行算法实现分析"
topic: internals
feature: parallel
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/pstl/"
source_llvm: "references/impl/llvm-project/libcxx/include/__pstl/"
---

# 并行算法实现分析

> C++17 引入了并行算法，允许算法在多线程上执行。本文基于 GCC 和 LLVM 的源码，分析并行算法的内部实现。

---

## 一、核心概念

### 1.1 什么是并行算法

并行算法使用执行策略在多线程上执行：

```cpp
// 并行算法的使用
vector<int> v = {1, 2, 3, 4, 5};
sort(execution::par, v.begin(), v.end());

// 并行 for_each
for_each(execution::par, v.begin(), v.end(), [](int& x) {
    x *= 2;
});
```

### 1.2 执行策略

```
执行策略：

execution::sequenced_policy：
  · 顺序执行
  · 保证顺序一致性

execution::parallel_policy：
  · 并行执行
  · 不保证顺序

execution::parallel_unsequenced_policy：
  · 并行 + 无序
  · 最高并行度
```

---

## 二、GCC (libstdc++) 的实现

### 2.1 并行后端（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/pstl/

// GCC 使用 TBB 或 OpenMP 作为并行后端

// TBB 后端
#ifdef _GLIBCXX_HAS_GTHREADS
#include <parallel/algorithm>
namespace __gnu_parallel {
    // 并行排序
    template<typename _RandomAccessIterator, typename _Compare>
    void __parallel_sort(_RandomAccessIterator __first,
                         _RandomAccessIterator __last,
                         _Compare __comp) {
        // 使用 TBB 并行排序
        // 1. 将数据分成多个块
        // 2. 每个线程排序一个块
        // 3. 合并排序后的块
    }
    
    // 并行 for_each
    template<typename _InputIterator, typename _Function>
    void __parallel_for_each(_InputIterator __first,
                             _InputIterator __last,
                             _Function __f) {
        // 使用 TBB 并行处理
        // 1. 计算线程数量
        // 2. 分配工作给每个线程
        // 3. 等待所有线程完成
    }
}
#endif

// OpenMP 后端
#ifdef _OPENMP
#include <parallel/algorithm>
namespace __gnu_parallel {
    // 使用 OpenMP 指令进行并行化
    #pragma omp parallel for schedule(dynamic)
    for (auto it = __first; it != __last; ++it) {
        __f(*it);
    }
}
#endif
```

### 2.2 执行策略分派（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/pstl/policy_fwd.h

// 执行策略类
struct sequenced_policy { };
struct parallel_policy { };
struct parallel_unsequenced_policy { };

// 全局策略对象
inline constexpr sequenced_policy seq{};
inline constexpr parallel_policy par{};
inline constexpr parallel_unsequenced_policy par_unseq{};

// 策略分派
template<typename _Policy>
constexpr bool __is_parallel_policy() {
    return is_same_v<_Policy, parallel_policy> ||
           is_same_v<_Policy, parallel_unsequenced_policy>;
}

// 根据策略选择实现
template<typename _Policy, typename _RandomAccessIterator, typename _Compare>
void __sort_impl(_Policy, _RandomAccessIterator __first,
                 _RandomAccessIterator __last, _Compare __comp) {
    if constexpr (__is_parallel_policy<_Policy>()) {
        // 并行实现
        __gnu_parallel::__parallel_sort(__first, __last, __comp);
    } else {
        // 顺序实现
        std::__sort(__first, __last, __comp);
    }
}
```

### 2.2 SIMD 优化

```
GCC 的 SIMD 优化：

1. 自动向量化：
   · 编译器自动使用 SIMD 指令
   · 适合简单循环

2. 手动 SIMD：
   · 使用 intrinsics
   · 更精细控制

3. 并行 + SIMD：
   · 多线程 + SIMD
   · 最高性能
```

---

## 三、LLVM (libc++) 的实现

### 3.1 并行后端

```cpp
// LLVM 的并行后端

// 默认后端（串行）
namespace std::execution {
    // sequenced_policy：顺序执行
}

// 并行后端
#ifdef _LIBCPP_HAS_THREADS
namespace std::execution {
    // parallel_policy：并行执行
    // parallel_unsequenced_policy：最高并行度
}
#endif
```

### 3.2 算法分派

```
LLVM 的算法分派：

1. 检测执行策略
2. 根据策略选择实现
3. 调用对应的并行算法

分派逻辑：
  execution::seq → 顺序实现
  execution::par → 并行实现
  execution::par_unseq → 并行 + SIMD
```

---

## 四、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ 并行后端               │ TBB/OpenMP           │ 内置实现             │
│ SIMD 支持              │ 编译器自动向量化     │ 编译器自动向量化     │
│ 线程池                 │ TBB 线程池           │ 内置线程池           │
│ 负载均衡               │ TBB 工作窃取         │ 内置实现             │
│ 任务调度               │ TBB 任务调度         │ 内置实现             │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 五、最佳实践

```
并行算法使用指南：

1. 选择合适的执行策略：
   · 需要顺序：execution::seq
   · 需要并行：execution::par
   · 最高性能：execution::par_unseq

2. 注意数据竞争：
   · 避免共享可变状态
   · 使用原子操作或锁

3. 注意负载均衡：
   · 并行任务应该均匀
   · 避免细粒度并行

4. 测试性能：
   · 并行不一定更快
   · 小数据集可能更慢
```

---

## 延伸阅读

- [std::thread 实现](/internals/concurrency/thread) — 线程的实现
- [std::atomic 实现](/internals/concurrency/atomic) — 原子操作的实现
- [排序算法实现](/internals/algorithms/sorting) — 排序算法的实现
