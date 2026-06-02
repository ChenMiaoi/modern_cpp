---
title: "伪共享（False Sharing）"
topic: topics
feature: memory-model-false-sharing
standard: C++
status_checked_at: 2026-06-02
---

# 伪共享（False Sharing）

> 伪共享是多核系统中最隐蔽的性能杀手之一。两个线程各自访问不同的变量，却因为它们恰好在同一条缓存行上而相互拖慢。理解伪共享的成因和修复方法是编写高性能并发代码的基本功。

---

## 1. 缓存行模型

### 1.1 缓存层次结构

```
现代 CPU 的缓存层次（典型配置）：

  ┌────────────────────────────────────────────────────────┐
  │  核心 0          核心 1          核心 2          核心 3│
  │  ┌──────┐       ┌──────┐       ┌──────┐       ┌──────┐│
  │  │L1:32K│       │L1:32K│       │L1:32K│       │L1:32K││
  │  │8-way │       │8-way │       │8-way │       │8-way ││
  │  ├──────┤       ├──────┤       ├──────┤       ├──────┤│
  │  │L2:256K│      │L2:256K│      │L2:256K│      │L2:256K││
  │  │8-way │       │8-way │       │8-way │       │8-way ││
  │  └──┬───┘       └──┬───┘       └──┬───┘       └──┬───┘│
  │     └──────────────┼──────────────┼──────────────┘    │
  │              ┌─────┴─────────────┴─────┐              │
  │              │    L3: 16-32MB 共享      │              │
  │              └──────────┬──────────────┘              │
  │                    ┌────┴────┐                        │
  │                    │  主内存  │                        │
  │                    └─────────┘                        │
  └────────────────────────────────────────────────────────┘

  关键参数：
  · L1 cache line = 64 字节（x86-64, ARM64 均为 64 字节）
  · 缓存一致性粒度 = 64 字节（整条缓存行一起失效）
  · 缓存行是 CPU 在缓存和主存之间传输的最小单位
```

### 1.2 MESI 协议

```
每条缓存行有一个 MESI 状态：

  Modified (M)  — 本核独占且已修改，其他核 Invalid
  Exclusive (E) — 本核独占但未修改，其他核 Invalid
  Shared (S)    — 多核共享，内容一致
  Invalid (I)   — 无效，不包含有效数据

  写入操作的状态转换：
  ┌─────────────────────────────────────────────────────────┐
  │ 核心 0 写入缓存行 L：                                   │
  │   1. 如果 L 在 E 或 M 状态 → 直接写入，转为 M           │
  │   2. 如果 L 在 S 状态 → 发送 Invalidate 广播            │
  │      · 其他核的 L 转为 I                                │
  │      · 本核的 L 转为 M                                  │
  │   3. 如果 L 在 I 状态 → 发送 Read-For-Ownership         │
  │      · 其他核的 L 转为 I（如果在 M 状态需先写回）       │
  │      · 本核获取 L，转为 M                               │
  │                                                         │
  │ 代价：每次 invalidate 广播 ≈ 30-100 个 CPU 周期         │
  │       缓存行在核间"乒乓"是性能灾难                     │
  └─────────────────────────────────────────────────────────┘
```

---

## 2. 伪共享的定义与症状

### 2.1 定义

```
伪共享（False Sharing）：
  当两个或多个核心各自独立地读写不同的变量，
  但这些变量恰好位于同一条缓存行上，
  导致缓存一致性协议频繁地在核间传送整条缓存行。

  ┌─────────────────────────────────────────────────────┐
  │ 缓存行（64 字节）                                   │
  │ ┌──────────┬──────────┬──────────┬──────────┐       │
  │ │ counter_A│ counter_B│  pad     │  pad     │       │
  │ │ 核心 0 写│ 核心 1 写│          │          │       │
  │ └──────────┴──────────┴──────────┴──────────┘       │
  │                                                     │
  │ 核心 0 写 counter_A → 整条缓存行对核心 1 失效      │
  │ 核心 1 写 counter_B → 整条缓存行对核心 0 失效      │
  │ → 两个核心不断相互使对方的缓存行失效               │
  │ → 乒乓效应，严重降低性能                           │
  └─────────────────────────────────────────────────────┘
```

### 2.2 症状

```
伪共享的症状：

  1. 线程数增加但性能不升反降
  2. 每个线程的 CPU 利用率低（大部分时间在等缓存）
  3. perf stat 显示大量 cache-misses 和 L1-dcache-load-misses
  4. perf c2c 显示特定缓存行在多核间高争用
  5. 将不相关变量分开到不同缓存行后性能急剧提升
```

### 2.3 经典复现

```cpp
#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>

// ❌ 伪共享：两个计数器紧挨在一起
struct bad_counters {
    std::atomic<int> a{0};
    std::atomic<int> b{0};
}; // a 和 b 极可能在同一缓存行上

// ✅ 修复后：padding 隔离
struct good_counters {
    alignas(64) std::atomic<int> a{0};
    alignas(64) std::atomic<int> b{0};
}; // a 和 b 必然在不同缓存行上

template <typename Counter>
void benchmark() {
    Counter c;
    constexpr int N = 100'000'000;

    auto start = std::chrono::steady_clock::now();

    std::thread t1([&] {
        for (int i = 0; i < N; ++i)
            c.a.fetch_add(1, std::memory_order_relaxed);
    });
    std::thread t2([&] {
        for (int i = 0; i < N; ++i)
            c.b.fetch_add(1, std::memory_order_relaxed);
    });
    t1.join();
    t2.join();

    auto elapsed = std::chrono::steady_clock::now() - start;
    std::cout << std::chrono::duration<double>(elapsed).count() << "s\n";
}

// 典型结果（4 核 x86-64）：
// bad_counters: ~8 秒（乒乓效应）
// good_counters: ~1.5 秒（无干扰）
// 差距：5-6 倍
```

---

## 3. 修复方法

### 3.1 alignas 对齐

```cpp
// 方法 1：alignas(64) — 推荐
struct thread_data {
    alignas(64) std::atomic<int> counter{0};
    alignas(64) int local_state = 0;
    // 每个字段独占至少一条缓存行
};

// C++17 起支持在声明中使用 alignas
```

### 3.2 手动填充（Padding）

```cpp
// 方法 2：手动 padding
struct padded_atomic {
    std::atomic<int> value{0};
    char padding[64 - sizeof(std::atomic<int>)]; // 填充到 64 字节
};

// 或更安全的方式：
struct padded_atomic_v2 {
    union {
        std::atomic<int> value{0};
        char cacheline[64];
    };
};
static_assert(sizeof(padded_atomic_v2) >= 64);

// 注意：padding 浪费内存，只在确认有伪共享问题时使用
```

### 3.3 `std::hardware_destructive_interference_size`

```cpp
// C++17 引入的标准常量
// 定义在 <new> 中
static_assert(std::hardware_destructive_interference_size >= 64);

struct proper_counters {
    alignas(std::hardware_destructive_interference_size)
        std::atomic<int> a{0};
    alignas(std::hardware_destructive_interference_size)
        std::atomic<int> b{0};
};

// ⚠ 但是！截至 2024 年：
// · GCC 已实现此常量（通常为 64）
// · Clang/LLVM 已实现（通常为 128，包含了 destructive 的极端情况）
// · MSVC 已实现（通常为 64）
// · 标准允许编译器在不同翻译单元返回不同值（理论上）
// · 实践中，直接用 64 更可靠
```

### 3.4 结构体布局优化

```cpp
// 将只由一个线程访问的字段放在一起
// 将多个线程共享的字段用 padding 隔开

struct server_state {
    // 只由主线程访问 — 可以放在一起
    int request_count = 0;
    int error_count = 0;
    double avg_latency = 0.0;

    // 每个工作线程独占的计数器 — 用 alignas 隔离
    alignas(64) std::atomic<int> worker_counts[16];

    // 多线程共享的标志 — 单独缓存行
    alignas(64) std::atomic<bool> shutdown{false};
};
```

---

## 4. 原子变量的伪共享

### 4.1 相邻原子变量的特殊问题

```cpp
// 即使是单个原子变量，如果与其他变量在同一缓存行上
// 也会造成伪共享

struct bad_layout {
    std::atomic<int> hot_counter{0};  // 频繁更新
    int cold_config = 0;              // 很少读取
};
// 每次 hot_counter 更新 → cold_config 所在的缓存行也失效
// 读取 cold_config 时需要从 L3 甚至主存重新加载

// ✅ 修复
struct good_layout {
    alignas(64) std::atomic<int> hot_counter{0};
    alignas(64) int cold_config = 0;
};
```

### 4.2 原子操作的缓存行争用

```cpp
// 多个线程 CAS 同一个 atomic<int>
std::atomic<int> shared{0};

// 每次 CAS：
// 1. 核心将缓存行转为 M 状态（RFO — Read-For-Ownership）
// 2. 其他核心的缓存行转为 I 状态
// 3. CAS 执行
// 4. 下一个核心需要重新获取缓存行

// 即使没有伪共享，多线程 CAS 同一变量也有争用
// 但伪共享会让情况更糟——不相关的变量也被波及

// 优化：分片（sharding）
alignas(64) std::atomic<int> shards[16]; // 每个核心一个分片
// 线程 i 更新 shards[i % 16]
// 读取时汇总所有分片
```

---

## 5. 检测工具

### 5.1 perf c2c（Linux）

```bash
# perf c2c 是检测伪共享的首选工具
# 记录缓存行争用数据
perf c2c record -a -- sleep 10

# 分析结果
perf c2c report --stdio

# 输出示例：
#   Shared Data Cache Line Table
#   ────────────────────────────
#   Total entries: 1234
#   ...
#   #  -----Hitm-----  ───Store───  ────────Source───────  ──Symbol──
#   #  Rmt  Lcl  Tot   L1  FB  SB   File:Line              Name
#     1  456  123  579   23   4   1   counter.cpp:42        worker_fn
#
# Hitm = Hit-Modified：缓存行在其他核的 L1 中处于 M 状态
# Rmt = Remote Hitm：来自远端 NUMA 节点（最昂贵）
# Lcl = Local Hitm：来自本地 NUMA 节点
#
# 高 Hitm 数 = 伪共享热点
```

### 5.2 Intel VTune

```
Intel VTune 的 Memory Access Analysis：

  · 可以精确到源代码行级别的缓存行争用
  · 显示 NUMA 节点间的远程访问比例
  · 提供优化建议（如"将变量对齐到缓存行边界"）
  · 适用于复杂的应用场景分析
```

### 5.3 ThreadSanitizer (TSan)

```bash
# TSan 可以检测数据竞争（不是伪共享）
# 但伪共享经常伴随数据竞争，所以 TSan 也能提供线索
g++ -fsanitize=thread -g -o app app.cpp

# 注意：TSan 本身开销大（5-15x），不适用于性能分析
# 它是正确性工具，不是性能工具
```

### 5.4 自制检测

```cpp
// 简单的伪共享检测器（概念验证）
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

template <int Stride>
void measure_contention() {
    struct alignas(64) { std::atomic<int> x{0}; } arr[2];

    constexpr int N = 50'000'000;
    auto start = std::chrono::steady_clock::now();

    std::thread t1([&] {
        for (int i = 0; i < N; ++i)
            arr[0].x.fetch_add(1, std::memory_order_relaxed);
    });
    std::thread t2([&] {
        for (int i = 0; i < N; ++i)
            arr[Stride].x.fetch_add(1, std::memory_order_relaxed);
    });
    t1.join();
    t2.join();

    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - start).count();
    std::cout << "Stride=" << Stride << ": " << us << " us\n";
}

// Stride=0（同一缓存行）vs Stride=1（不同缓存行）
// 性能差异就是伪共享的度量
```

---

## 6. 编译器对缓存行大小的支持

```cpp
// 获取缓存行大小的各种方法

// 方法 1：C++17 标准
#include <new>
constexpr std::size_t line = std::hardware_destructive_interference_size;
// ⚠ 编译期常量，但值因编译器/平台而异

// 方法 2：运行时查询（Linux）
#include <unistd.h>
long line = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);

// 方法 3：运行时查询（Windows）
#include <windows.h>
DWORD line_size;
SYSTEM_LOGICAL_PROCESSOR_INFORMATION info[64];
DWORD len = sizeof(info);
GetLogicalProcessorInformation(info, &len);
// 遍历 info 找 RelationCache，Level == 1，CacheLineSize

// 方法 4：cpuid 指令（x86）
// CPUID leaf 0x80000006 返回 L2 cache line size
// CPUID leaf 0x01 返回 CLFLUSH line size

// 方法 5：直接假设 64 字节
// x86-64 和 ARM64 上 64 字节是行业标准
// 除非有特殊硬件，直接用 64 是最实用的方案
```

---

## 7. 进阶：NUMA 与伪共享

```
NUMA 系统中的伪共享更严重：

  ┌─────────────────────┐    ┌─────────────────────┐
  │ NUMA Node 0         │    │ NUMA Node 1         │
  │ ┌─────┐ ┌─────┐    │    │ ┌─────┐ ┌─────┐    │
  │ │Core0│ │Core1│    │    │ │Core2│ │Core3│    │
  │ └──┬──┘ └──┬──┘    │    │ └──┬──┘ └──┬──┘    │
  │    └───┬───┘       │    │    └───┬───┘       │
  │    ┌───┴───┐       │    │    ┌───┴───┐       │
  │    │ 本地内存│       │    │    │ 本地内存│       │
  │    └───────┘       │    │    └───────┘       │
  └─────────┬──────────┘    └──────────┬──────────┘
            │         QPI/Infinity     │
            └──────────────────────────┘

  跨 NUMA 节点的缓存行传送：
  · 延迟：本地 ≈ 80ns，远程 ≈ 150ns（2 倍）
  · 带宽：远程可能只有本地的 60-70%

  如果两个核心在不同 NUMA 节点上产生伪共享
  → 每次失效需要跨节点传输 → 性能更差

  优化：用线程亲和性（affinity）将相关线程绑定到同一 NUMA 节点
```

---

## 8. 特殊场景

### 8.1 读多写少场景

```cpp
// 伪共享在读多写少场景下也很有害
// 写操作会使其他核的缓存行失效
// 即使其他核只是在读取

struct read_heavy {
    alignas(64) std::atomic<int> writer_data{0};   // 一个核频繁写
    alignas(64) std::atomic<int> reader_data{0};   // 多个核只读
};
// 不对齐时：writer 的每次写入都会使 reader 的缓存行失效
// 对齐后：writer 不影响 reader
```

### 8.2 标准库容器

```cpp
// std::vector 或 std::array 的元素如果跨缓存行边界
// 也可能产生伪共享

// 例如：两个工作线程分别处理 vec[i] 和 vec[i+1]
// 如果 sizeof(vec[0]) < 64，它们可能在同一条缓存行上

// 解决方案：
// 1. 让每个线程处理足够大的块（> 64 字节）
// 2. 用 padding 让每个元素独占缓存行
// 3. 将数据重新组织为 SoA（Structure of Arrays）
```

### 8.3 SoA vs AoS

```cpp
// AoS (Array of Structures) — 容易产生伪共享
struct particle_aos {
    float x, y, z;       // 位置
    float vx, vy, vz;    // 速度
    float mass;           // 质量
};
std::vector<particle_aos> particles;

// SoA (Structure of Arrays) — 更缓存友好
struct particles_soa {
    std::vector<float> x, y, z;
    std::vector<float> vx, vy, vz;
    std::vector<float> mass;
};
// 当线程只访问位置时，不需要加载速度数据到缓存行
// 减少了伪共享的可能性
```

---

## 9. 性能影响量化

```
伪共享的性能影响（典型场景）：

  场景                        无伪共享    有伪共享    差距
  ─────────────────────────────────────────────────────────
  双线程计数器                1.5s       8s          5.3x
  4 线程生产者-消费者         2.0s       12s         6x
  8 线程无锁队列              3.0s       25s         8.3x
  NUMA 远端伪共享             2.0s       40s         20x

  影响因素：
  · 争用的核心数（越多越差）
  · 争用频率（每秒操作数越高越差）
  · 缓存行大小（64 字节标准）
  · NUMA 拓扑（跨节点更差）
```

---

## 10. 总结

```
伪共享速查：
┌─────────────────────────────────────────────────────────────────┐
│ 定义：不同线程访问不同变量，但它们在同一缓存行上              │
│ 症状：线程数增加但性能不升反降，高 cache miss                  │
│ 检测：perf c2c, Intel VTune                                    │
│ 修复：alignas(64), std::hardware_destructive_interference_size │
│ 预防：SoA 优于 AoS，分片优于共享，隔离优于混合                │
│ 注意：不要过度优化——只有确认有伪共享问题时才对齐              │
└─────────────────────────────────────────────────────────────────┘
```
