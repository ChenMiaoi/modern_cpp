---
title: "Concurrency & Memory Model Terminology"
topic: unknown
feature: concurrency-terms
standard: N/A
status_checked_at: 2026-06-02
---
# Concurrency & Memory Model Terminology

## Data Race vs Race Condition

**Data Race**: Two threads access the same memory location simultaneously, at least one is a write, and there is no synchronization. This is **UB**.

**Race Condition**: The program result depends on the relative execution order of threads. Not UB, but a bug.

```cpp
// Data Race (UB!)
int counter = 0;
std::thread t1([&]{ counter++; });
std::thread t2([&]{ counter++; });

// Race Condition (logic bug, not UB)
bool ready = false;
std::thread producer([&]{ data = 42; ready = true; });  // may be reordered
std::thread consumer([&]{ while(!ready); use(data); }); // may see data=0
```

## Happens-Before

The core relationship in the C++ memory model — if A happens-before B, then A's effects are visible to B:

```
- Within the same thread: A is before B → A sequenced-before B → A happens-before B
- Across threads: A release → B acquire (on the same atomic variable) → A happens-before B
- Transitivity: A happens-before B and B happens-before C → A happens-before C
```

## Memory Order

### relaxed

Guarantees only atomicity, not ordering:

```cpp
std::atomic<int> counter{0};
counter.fetch_add(1, std::memory_order_relaxed);
// guarantees atomic increment, but does not guarantee other threads see the result immediately
```

### acquire / release

Used in pairs to establish a happens-before relationship:

```cpp
// Thread 1: write data + release
data = 42;
flag.store(true, std::memory_order_release);

// Thread 2: acquire + read data
if (flag.load(std::memory_order_acquire)) {
  use(data);  // guaranteed to see data = 42
}
```

### seq_cst (default)

The strongest ordering — all threads see the same operation order. Worst performance but safest.

## Lock-Free

A data structure is lock-free if at least one thread can complete its operation in a finite number of steps (even if other threads are suspended):

```cpp
std::atomic<int> counter;
// lock-free: fetch_add can always complete in finite steps
```

## ABA Problem

A classic pitfall in lock-free algorithms:

```
Thread 1: reads A → computes new value → CAS(A, B)
Thread 2: before Thread 1's CAS, changes A to B then back to A
Thread 1: CAS succeeds — but the data has been modified!
```

Solution: Use versioned pointers or `std::shared_ptr` atomic operations.

## False Sharing

Two threads access different variables, but those variables happen to be on the same cache line:

```cpp
struct Bad {
  int thread1_data;  // assume on cache line X
  int thread2_data;  // also on cache line X!
  // two threads modify different variables, but cache line invalidation causes performance degradation
};

struct Good {
  alignas(64) int thread1_data;  // occupies its own cache line
  alignas(64) int thread2_data;  // occupies its own cache line
};
```

Cache lines are typically 64 bytes. `alignas(64)` ensures variables are aligned to cache line boundaries.

## Memory Barrier

Tells the CPU not to reorder specific memory operations. The memory orders of `std::atomic` compile down to memory barrier instructions.

```
x86/x64: inherently strong memory model; most reordering is prohibited by hardware
ARM/POWER: weak memory model; explicit barriers required
```

This is why `memory_order_relaxed` appears to "work fine" on x86 but exposes problems on ARM.
