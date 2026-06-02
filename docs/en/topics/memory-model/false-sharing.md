---
title: "False Sharing"
topic: topics
feature: memory-model-false-sharing
standard: C++
status_checked_at: 2026-06-02
---

# False Sharing

> False sharing is one of the most insidious performance killers on multi-core systems. Two threads access different variables independently, yet they slow each other down because those variables happen to reside on the same cache line. Understanding the causes and fixes of false sharing is a fundamental skill for writing high-performance concurrent code.

---

## 1. Cache Line Model

### 1.1 Cache Hierarchy

```
Modern CPU cache hierarchy (typical configuration):

  ┌────────────────────────────────────────────────────────┐
  │  Core 0          Core 1          Core 2          Core 3│
  │  ┌──────┐       ┌──────┐       ┌──────┐       ┌──────┐│
  │  │L1:32K│       │L1:32K│       │L1:32K│       │L1:32K││
  │  │8-way │       │8-way │       │8-way │       │8-way ││
  │  ├──────┤       ├──────┤       ├──────┤       ├──────┤│
  │  │L2:256K│      │L2:256K│      │L2:256K│      │L2:256K││
  │  │8-way │       │8-way │       │8-way │       │8-way ││
  │  └──┬───┘       └──┬───┘       └──┬───┘       └──┬───┘│
  │     └──────────────┼──────────────┼──────────────┘    │
  │              ┌─────┴─────────────┴─────┐              │
  │              │    L3: 16-32MB Shared    │              │
  │              └──────────┬──────────────┘              │
  │                    ┌────┴────┐                        │
  │                    │  Main Memory │                        │
  │                    └─────────┘                        │
  └────────────────────────────────────────────────────────┘

  Key parameters:
  · L1 cache line = 64 bytes (both x86-64 and ARM64 use 64 bytes)
  · Cache coherence granularity = 64 bytes (entire cache line invalidated together)
  · Cache line is the minimum unit of transfer between CPU cache and main memory
```

### 1.2 MESI Protocol

```
Each cache line has a MESI state:

  Modified (M)  — exclusive to this core and modified; other cores Invalid
  Exclusive (E) — exclusive to this core but unmodified; other cores Invalid
  Shared (S)    — shared across cores, contents consistent
  Invalid (I)   — invalid, contains no valid data

  Write operation state transitions:
  ┌─────────────────────────────────────────────────────────┐
  │ Core 0 writes to cache line L:                          │
  │   1. If L is in E or M state → write directly, transition to M │
  │   2. If L is in S state → send Invalidate broadcast     │
  │      · Other cores' L transitions to I                  │
  │      · This core's L transitions to M                   │
  │   3. If L is in I state → send Read-For-Ownership       │
  │      · Other cores' L transitions to I (if M, write back first) │
  │      · This core acquires L, transitions to M           │
  │                                                         │
  │ Cost: each invalidate broadcast ≈ 30-100 CPU cycles     │
  │       Cache line "ping-pong" between cores is a performance disaster │
  └─────────────────────────────────────────────────────────┘
```

---

## 2. Definition and Symptoms of False Sharing

### 2.1 Definition

```
False Sharing:
  When two or more cores independently read and write different variables,
  but those variables happen to reside on the same cache line,
  causing the cache coherence protocol to frequently transfer the entire
  cache line between cores.

  ┌─────────────────────────────────────────────────────┐
  │ Cache line (64 bytes)                               │
  │ ┌──────────┬──────────┬──────────┬──────────┐       │
  │ │ counter_A│ counter_B│  pad     │  pad     │       │
  │ │ Core 0 W │ Core 1 W │          │          │       │
  │ └──────────┴──────────┴──────────┴──────────┘       │
  │                                                     │
  │ Core 0 writes counter_A → entire cache line invalid │
  │                             for Core 1              │
  │ Core 1 writes counter_B → entire cache line invalid │
  │                             for Core 0              │
  │ → The two cores continuously invalidate each other's │
  │   cache lines                                        │
  │ → Ping-pong effect, severely degrades performance    │
  └─────────────────────────────────────────────────────┘
```

### 2.2 Symptoms

```
Symptoms of false sharing:

  1. Thread count increases but performance drops instead of improving
  2. Low per-thread CPU utilization (most time spent waiting for cache)
  3. perf stat shows large numbers of cache-misses and L1-dcache-load-misses
  4. perf c2c shows specific cache lines with high contention across cores
  5. Dramatic performance improvement when separating unrelated variables
     onto different cache lines
```

### 2.3 Classic Reproduction

```cpp
#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>

// ❌ False sharing: two counters adjacent in memory
struct bad_counters {
    std::atomic<int> a{0};
    std::atomic<int> b{0};
}; // a and b are very likely on the same cache line

// ✅ Fixed: padding isolation
struct good_counters {
    alignas(64) std::atomic<int> a{0};
    alignas(64) std::atomic<int> b{0};
}; // a and b are guaranteed to be on different cache lines

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

// Typical results (4-core x86-64):
// bad_counters: ~8 seconds (ping-pong effect)
// good_counters: ~1.5 seconds (no interference)
// Difference: 5-6x
```

---

## 3. Fix Methods

### 3.1 alignas Alignment

```cpp
// Method 1: alignas(64) — recommended
struct thread_data {
    alignas(64) std::atomic<int> counter{0};
    alignas(64) int local_state = 0;
    // Each field exclusively occupies at least one cache line
};

// Supported in declarations since C++17
```

### 3.2 Manual Padding

```cpp
// Method 2: manual padding
struct padded_atomic {
    std::atomic<int> value{0};
    char padding[64 - sizeof(std::atomic<int>)]; // pad to 64 bytes
};

// Or a safer approach:
struct padded_atomic_v2 {
    union {
        std::atomic<int> value{0};
        char cacheline[64];
    };
};
static_assert(sizeof(padded_atomic_v2) >= 64);

// Note: padding wastes memory; use only after confirming false sharing
```

### 3.3 `std::hardware_destructive_interference_size`

```cpp
// Standard constant introduced in C++17
// Defined in <new>
static_assert(std::hardware_destructive_interference_size >= 64);

struct proper_counters {
    alignas(std::hardware_destructive_interference_size)
        std::atomic<int> a{0};
    alignas(std::hardware_destructive_interference_size)
        std::atomic<int> b{0};
};

// ⚠ However! As of 2024:
// · GCC has implemented this constant (typically 64)
// · Clang/LLVM has implemented it (typically 128, including destructive extremes)
// · MSVC has implemented it (typically 64)
// · The standard allows compilers to return different values in different TUs (in theory)
// · In practice, using 64 directly is more reliable
```

### 3.4 Struct Layout Optimization

```cpp
// Place fields accessed by only one thread together
// Separate fields shared by multiple threads with padding

struct server_state {
    // Only accessed by the main thread — can be grouped together
    int request_count = 0;
    int error_count = 0;
    double avg_latency = 0.0;

    // Counters exclusively owned by each worker thread — isolate with alignas
    alignas(64) std::atomic<int> worker_counts[16];

    // Multi-threaded shared flag — separate cache line
    alignas(64) std::atomic<bool> shutdown{false};
};
```

---

## 4. False Sharing with Atomic Variables

### 4.1 Special Issues with Adjacent Atomic Variables

```cpp
// Even a single atomic variable can cause false sharing
// if it shares a cache line with other variables

struct bad_layout {
    std::atomic<int> hot_counter{0};  // frequently updated
    int cold_config = 0;              // rarely read
};
// Every hot_counter update → invalidates the cache line containing cold_config
// Reading cold_config requires reloading from L3 or main memory

// ✅ Fixed
struct good_layout {
    alignas(64) std::atomic<int> hot_counter{0};
    alignas(64) int cold_config = 0;
};
```

### 4.2 Cache Line Contention with Atomic Operations

```cpp
// Multiple threads CAS-ing the same atomic<int>
std::atomic<int> shared{0};

// Each CAS:
// 1. Core transitions cache line to M state (RFO — Read-For-Ownership)
// 2. Other cores' cache lines transition to I state
// 3. CAS executes
// 4. Next core needs to re-acquire the cache line

// Even without false sharing, multi-thread CAS on the same variable has contention
// But false sharing makes it worse — unrelated variables are also affected

// Optimization: sharding
alignas(64) std::atomic<int> shards[16]; // one shard per core
// Thread i updates shards[i % 16]
// Aggregate all shards when reading
```

---

## 5. Detection Tools

### 5.1 perf c2c (Linux)

```bash
# perf c2c is the primary tool for detecting false sharing
# Record cache line contention data
perf c2c record -a -- sleep 10

# Analyze results
perf c2c report --stdio

# Output example:
#   Shared Data Cache Line Table
#   ────────────────────────────
#   Total entries: 1234
#   ...
#   #  -----Hitm-----  ───Store───  ────────Source───────  ──Symbol──
#   #  Rmt  Lcl  Tot   L1  FB  SB   File:Line              Name
#     1  456  123  579   23   4   1   counter.cpp:42        worker_fn
#
# Hitm = Hit-Modified: cache line is in M state in another core's L1
# Rmt = Remote Hitm: from a remote NUMA node (most expensive)
# Lcl = Local Hitm: from the local NUMA node
#
# High Hitm count = false sharing hotspot
```

### 5.2 Intel VTune

```
Intel VTune's Memory Access Analysis:

  · Cache line contention accurate to source code line level
  · Shows remote access ratio between NUMA nodes
  · Provides optimization suggestions (e.g., "align variables to cache line boundaries")
  · Suitable for complex application scenario analysis
```

### 5.3 ThreadSanitizer (TSan)

```bash
# TSan can detect data races (not false sharing)
# But false sharing often accompanies data races, so TSan can provide clues
g++ -fsanitize=thread -g -o app app.cpp

# Note: TSan itself has high overhead (5-15x), not suitable for performance profiling
# It is a correctness tool, not a performance tool
```

### 5.4 DIY Detection

```cpp
// Simple false sharing detector (proof of concept)
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

// Stride=0 (same cache line) vs Stride=1 (different cache line)
// The performance difference is a measure of false sharing
```

---

## 6. Compiler Support for Cache Line Size

```cpp
// Various ways to obtain the cache line size

// Method 1: C++17 standard
#include <new>
constexpr std::size_t line = std::hardware_destructive_interference_size;
// ⚠ Compile-time constant, but value varies by compiler/platform

// Method 2: Runtime query (Linux)
#include <unistd.h>
long line = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);

// Method 3: Runtime query (Windows)
#include <windows.h>
DWORD line_size;
SYSTEM_LOGICAL_PROCESSOR_INFORMATION info[64];
DWORD len = sizeof(info);
GetLogicalProcessorInformation(info, &len);
// Iterate info to find RelationCache, Level == 1, CacheLineSize

// Method 4: cpuid instruction (x86)
// CPUID leaf 0x80000006 returns L2 cache line size
// CPUID leaf 0x01 returns CLFLUSH line size

// Method 5: Just assume 64 bytes
// 64 bytes is the industry standard on x86-64 and ARM64
// Unless there is special hardware, using 64 directly is the most practical approach
```

---

## 7. Advanced: NUMA and False Sharing

```
False sharing is worse on NUMA systems:

  ┌─────────────────────┐    ┌─────────────────────┐
  │ NUMA Node 0         │    │ NUMA Node 1         │
  │ ┌─────┐ ┌─────┐    │    │ ┌─────┐ ┌─────┐    │
  │ │Core0│ │Core1│    │    │ │Core2│ │Core3│    │
  │ └──┬──┘ └──┬──┘    │    │ └──┬──┘ └──┬──┘    │
  │    └───┬───┘       │    │    └───┬───┘       │
  │    ┌───┴───┐       │    │    ┌───┴───┐       │
  │    │ Local Mem│    │    │    │ Local Mem│    │
  │    └───────┘       │    │    └───────┘       │
  └─────────┬──────────┘    └──────────┬──────────┘
            │         QPI/Infinity     │
            └──────────────────────────┘

  Cross-NUMA cache line transfer:
  · Latency: local ≈ 80ns, remote ≈ 150ns (2x)
  · Bandwidth: remote may be only 60-70% of local

  If two cores on different NUMA nodes experience false sharing
  → each invalidation requires cross-node transfer → even worse performance

  Optimization: use thread affinity to bind related threads to the same NUMA node
```

---

## 8. Special Scenarios

### 8.1 Read-Heavy, Write-Light Scenarios

```cpp
// False sharing is also harmful in read-heavy, write-light scenarios
// Writes invalidate other cores' cache lines
// Even if those cores are only reading

struct read_heavy {
    alignas(64) std::atomic<int> writer_data{0};   // one core writes frequently
    alignas(64) std::atomic<int> reader_data{0};   // multiple cores only read
};
// Without alignment: every writer write invalidates the reader's cache line
// With alignment: writer does not affect reader
```

### 8.2 Standard Library Containers

```cpp
// Elements of std::vector or std::array that span cache line boundaries
// can also produce false sharing

// Example: two worker threads process vec[i] and vec[i+1] respectively
// If sizeof(vec[0]) < 64, they may be on the same cache line

// Solutions:
// 1. Have each thread process a sufficiently large chunk (> 64 bytes)
// 2. Use padding so each element exclusively occupies a cache line
// 3. Reorganize data as SoA (Structure of Arrays)
```

### 8.3 SoA vs AoS

```cpp
// AoS (Array of Structures) — prone to false sharing
struct particle_aos {
    float x, y, z;       // position
    float vx, vy, vz;    // velocity
    float mass;           // mass
};
std::vector<particle_aos> particles;

// SoA (Structure of Arrays) — more cache-friendly
struct particles_soa {
    std::vector<float> x, y, z;
    std::vector<float> vx, vy, vz;
    std::vector<float> mass;
};
// When a thread only accesses positions, no need to load velocity data into cache
// Reduces the likelihood of false sharing
```

---

## 9. Quantifying Performance Impact

```
Performance impact of false sharing (typical scenarios):

  Scenario                    No False Share  False Share  Difference
  ─────────────────────────────────────────────────────────
  Two-thread counter          1.5s            8s           5.3x
  4-thread producer-consumer  2.0s            12s          6x
  8-thread lock-free queue    3.0s            25s          8.3x
  NUMA remote false sharing   2.0s            40s          20x

  Influencing factors:
  · Number of contending cores (more is worse)
  · Contention frequency (higher ops/sec is worse)
  · Cache line size (64 bytes standard)
  · NUMA topology (cross-node is worse)
```

---

## 10. Summary

```
False sharing quick reference:
┌─────────────────────────────────────────────────────────────────┐
│ Definition: different threads access different variables,       │
│             but they reside on the same cache line              │
│ Symptoms: thread count increases but performance drops,         │
│           high cache miss rate                                  │
│ Detection: perf c2c, Intel VTune                                │
│ Fix: alignas(64), std::hardware_destructive_interference_size   │
│ Prevention: SoA over AoS, sharding over sharing, isolation over mixing│
│ Note: don't over-optimize — only align after confirming false sharing│
└─────────────────────────────────────────────────────────────────┘
```
