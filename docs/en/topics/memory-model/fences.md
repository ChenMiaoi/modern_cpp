---
title: "Memory Fences"
topic: topics
feature: memory-model-fences
standard: C++
status_checked_at: 2026-06-02
---

# Memory Fences

> Standalone memory fences (`atomic_thread_fence`) are the other half of synchronization primitives in the C++ memory model — unlike embedded ordering in atomic operations, fences provide ordering constraints **without a specific atomic operation**. Understanding the equivalences and differences between fences and atomic operation ordering is a prerequisite for writing correct lock-free code.

---

## 1. Two Synchronization Paradigms

```
Paradigm A: Operation-embedded ordering
  x.store(1, std::memory_order_release);
  int v = x.load(std::memory_order_acquire);

Paradigm B: Standalone fence + relaxed operations
  x.store(1, std::memory_order_relaxed);
  std::atomic_thread_fence(std::memory_order_release);
  // ...
  int v = x.load(std::memory_order_relaxed);
  std::atomic_thread_fence(std::memory_order_acquire);

The two paradigms are not fully equivalent — the fence version has stricter synchronization conditions (see Section 4)
```

---

## 2. Fence Types

### 2.1 Acquire Fence

```cpp
std::atomic_thread_fence(std::memory_order_acquire);

// Semantics: all relaxed loads before the fence cannot be reordered after the fence
// Hardware mapping:
//   x86:  no instruction (TSO naturally guarantees load-load order), compiler barrier suffices
//   ARM:  DMB ISHLD (data memory barrier, load direction)
//   POWER: lwsync
```

### 2.2 Release Fence

```cpp
std::atomic_thread_fence(std::memory_order_release);

// Semantics: all relaxed stores after the fence cannot be reordered before the fence
// Hardware mapping:
//   x86:  no instruction (TSO naturally guarantees store-store order), compiler barrier
//   ARM:  DMB ISH (full-direction data memory barrier)
//   POWER: lwsync
```

### 2.3 Acq_rel Fence

```cpp
std::atomic_thread_fence(std::memory_order_acq_rel);

// Semantics: simultaneously has acquire and release effects
// Hardware mapping:
//   x86:  no extra instruction (TSO compatible)
//   ARM:  DMB ISH
//   POWER: lwsync
```

### 2.4 Seq_cst Fence

```cpp
std::atomic_thread_fence(std::memory_order_seq_cst);

// Semantics: strongest — all seq_cst operations and fences form a global total order
// Hardware mapping:
//   x86:  MFENCE (or LOCK-prefixed instruction)
//   ARM:  DMB ISH (plus additional global ordering constraints)
//   POWER: sync (stronger barrier than lwsync)
//
// ⚠ seq_cst fence has significant overhead on all architectures
```

---

## 3. x86-TSO vs ARM Weak Ordering Model

### 3.1 x86-TSO (Total Store Order)

```
x86 hardware guarantees:
  ✅ load-load ordering
  ✅ load-store ordering
  ✅ store-store ordering
  ❌ store-load not ordered (only weakness, implemented via store buffer)

  Implications:
  · acquire fence  → no hardware instruction (TSO naturally satisfies), only compiler barrier needed
  · release fence  → no hardware instruction, only compiler barrier
  · acq_rel fence  → same as above
  · seq_cst fence  → MFENCE (to resolve store-load reordering)

  This is why most fences on x86 have nearly zero overhead —
  the hardware already does the ordering, the fence only needs to prevent compiler reordering.
```

```
x86 store buffer's store-load loophole:

  CPU Core                    Store Buffer        Cache/L3
  ┌──────────┐               ┌──────────┐       ┌──────────┐
  │ store x=1│ ──────────→   │ x: 1     │ ──→   │ x: 1     │
  │          │               │ (uncommitted)│     │          │
  │ load y   │ ←──────────── │          │       │ y: 0     │
  │ returns 0│               └──────────┘       └──────────┘
  └──────────┘

  Store enters the store buffer but is not yet visible to other cores
  But this core's load can read from the store buffer (store-to-load forwarding)
  For load y, directly reads the old value from cache
  → This is the hardware root cause of store-load reordering
```

### 3.2 ARM (AArch64 / ARMv8)

```
ARM hardware guarantees:
  ❌ load-load can be reordered
  ❌ load-store can be reordered
  ❌ store-load can be reordered
  ❌ store-store can be reordered

  All fences require actual hardware instructions:

  DMB ISHLD   → acquire fence (load-direction barrier)
  DMB ISH     → release fence (full-direction barrier)
  DMB ISH     → acq_rel fence
  DMB ISH     → seq_cst fence (some implementations need DSB + ISB)

  Implications:
  · Every fence on ARM has real hardware cost
  · Fence placement must be more careful
  · Performance advantage of relaxed operations is more pronounced (avoid unnecessary fences)
```

### 3.3 POWER (IBM)

```
POWER is one of the weakest mainstream architectures:

  · All four reordering types can occur
  · Requires lwsync (lightweight sync barrier) or sync (heavyweight sync)
  · Even lwsync has tens of cycles overhead
  · sync may exceed 100 cycles on some implementations

  Programming insights on POWER:
  · Use relaxed + precise fences whenever possible instead of blanket seq_cst
  · But correctness requirements are higher because more reordering is possible
```

---

## 4. Fence Placement Rules

### 4.1 Release Fence + Acquire Fence Pairing

```cpp
// Correct release-acquire fence pairing
std::atomic<int> x{0};
int data = 0;

// Thread A
data = 42;                                              // ① Normal write
std::atomic_thread_fence(std::memory_order_release);    // ② Release fence
x.store(1, std::memory_order_relaxed);                  // ③ Relaxed store

// Thread B
while (x.load(std::memory_order_relaxed) != 1) {}      // ④ Relaxed load
std::atomic_thread_fence(std::memory_order_acquire);    // ⑤ Acquire fence
int v = data;                                           // ⑥ Read data

// Synchronization condition: ④ reads the value written by ③ (i.e., x == 1)
//   → ② release fence synchronizes-with ⑤ acquire fence
//   → ① happens-before ⑥
//   → v == 42 ✓
```

### 4.2 Fences Don't Guarantee Specific Operation Ordering

```cpp
// ✅ Correct usage: if B's while exits (y == 1), then v == 1
std::atomic<int> x{0}, y{0};

// Thread A
x.store(1, std::memory_order_relaxed);
std::atomic_thread_fence(std::memory_order_release);
y.store(1, std::memory_order_relaxed);

// Thread B
while (y.load(std::memory_order_relaxed) != 1) {}
std::atomic_thread_fence(std::memory_order_acquire);
int v = x.load(std::memory_order_relaxed); // v == 1

// ⚠ But if B's load(y) doesn't read the store(y) (still in the loop),
//    then the fence establishes no synchronization relationship
//    Fence synchronization is conditional — must be activated through
//    the atomic operation's rf relationship
```

### 4.3 One-Sided Fences Are Ineffective

```cpp
// ❌ Wrong: only release fence, no acquire fence
std::atomic<int> x{0};
int data = 0;

// Thread A
data = 42;
std::atomic_thread_fence(std::memory_order_release);
x.store(1, std::memory_order_relaxed);

// Thread B
while (x.load(std::memory_order_relaxed) != 1) {}
// No acquire fence!
int v = data; // ❌ data may not be 42

// Release fence only constrains ordering on A's side
// B has no acquire constraint, compiler/CPU can reorder load(data) before load(x)
```

---

## 5. Standalone Fence vs Operation-Embedded Ordering

### 5.1 Semantic Differences

```cpp
// Approach 1: embedded ordering
x.store(1, std::memory_order_release);
// Only constrains this store's release semantics

// Approach 2: standalone fence
x.store(1, std::memory_order_relaxed);
std::atomic_thread_fence(std::memory_order_release);
// Constrains all stores before this fence (not just x) with all operations after

// Key difference:
// Release fence constrains ALL preceding write operations, not just the immediately following one
// While store(release) only constrains that specific store
```

```cpp
// Concrete example illustrating the difference
int a = 0, b = 0;
std::atomic<int> flag{0};

// Approach A: store(release) — only guarantees operations before flag don't reorder past flag
a = 1;
b = 2;
flag.store(1, std::memory_order_release);
// ✓ a=1, b=2 won't be reordered after flag.store

// Approach B: relaxed store + release fence
a = 1;
b = 2;
std::atomic_thread_fence(std::memory_order_release);
flag.store(1, std::memory_order_relaxed);
// ✓ Same effect: all writes before the fence (a=1, b=2) won't reorder past the fence
//   And flag.store is after the fence, so a=1, b=2 won't reorder past flag.store
```

### 5.2 When to Use Standalone Fences

```
Standalone fence applicable scenarios:

  1. Multiple variables need synchronization, but only one is atomic
     int data1, data2, data3;
     std::atomic<int> flag{0};

     // Write side
     data1 = ...; data2 = ...; data3 = ...;
     std::atomic_thread_fence(std::memory_order_release);
     flag.store(1, std::memory_order_relaxed);

  2. Multiple atomic variables need unified fence constraints
     x.store(1, std::memory_order_relaxed);
     y.store(2, std::memory_order_relaxed);
     std::atomic_thread_fence(std::memory_order_release);
     // Both x and y stores are constrained by the same fence

  3. Relaxed operations already exist, need to add ordering without modifying the operations themselves
     (common during codebase refactoring)
```

### 5.3 When to Use Embedded Ordering

```
Embedded ordering applicable scenarios:

  1. Simple synchronization of a single atomic variable (most common pattern)
     x.store(val, std::memory_order_release);
     auto v = x.load(std::memory_order_acquire);

  2. CAS operations — fences cannot precisely simulate CAS success/failure ordering
     x.compare_exchange_weak(expected, desired,
         std::memory_order_acq_rel,  // success
         std::memory_order_acquire); // failure

  3. Performance-sensitive — fences may be stronger than necessary
     (e.g., on ARM, release fence uses DMB ISH full-direction barrier,
       while store(release) only needs DMB ISH but can be further optimized by compiler)
```

---

## 6. Common Fence Patterns

### 6.1 Store-Load Fence (SB Elimination)

```cpp
// Store-Buffering elimination: most classic seq_cst fence usage
std::atomic<int> x{0}, y{0};

// Thread A
x.store(1, std::memory_order_relaxed);
std::atomic_thread_fence(std::memory_order_seq_cst); // Key!
int r1 = y.load(std::memory_order_relaxed);

// Thread B
y.store(1, std::memory_order_relaxed);
std::atomic_thread_fence(std::memory_order_seq_cst); // Key!
int r2 = x.load(std::memory_order_relaxed);

// Impossible for r1 == 0 && r2 == 0
// seq_cst fence guarantees the two fences are ordered in a global total order

// ⚠ Using only release/acquire fences cannot guarantee SB elimination
//    seq_cst fence or seq_cst operations are required
```

### 6.2 IRIW (Independent Reads of Independent Writes)

```cpp
// Four-thread litmus test: IRIW
std::atomic<int> x{0}, y{0};

// Thread A: x.store(1, relaxed)
// Thread B: y.store(1, relaxed)
// Thread C: r1=x.load(relaxed); r2=y.load(relaxed); // sees x=1, y=0
// Thread D: r3=y.load(relaxed); r4=x.load(relaxed); // sees y=1, x=0

// seq_cst fence can eliminate this inconsistent result:

// Thread C                          // Thread D
r1 = x.load(relaxed);             r3 = y.load(relaxed);
std::atomic_thread_fence(         std::atomic_thread_fence(
    std::memory_order_seq_cst);       std::memory_order_seq_cst);
r2 = y.load(relaxed);             r4 = x.load(relaxed);

// seq_cst fence guarantees C's fence and D's fence are ordered in a global total order
// → impossible to see inconsistent store ordering
```

### 6.3 Multi-Variable Synchronization

```cpp
// Synchronize multiple data items with a single fence
int a, b, c, d;
std::atomic<int> guard{0};

// Write side
a = 1; b = 2; c = 3; d = 4;
// Only one release fence needed, not four store(release)
std::atomic_thread_fence(std::memory_order_release);
guard.store(1, std::memory_order_relaxed);

// Read side
while (guard.load(std::memory_order_relaxed) != 1) {}
std::atomic_thread_fence(std::memory_order_acquire);
// Values of a, b, c, d are all guaranteed visible
```

---

## 7. Compiler Implementation of Fences

### 7.1 Compiler Barriers

```cpp
// At the compiler level, fence implementation is typically a compiler barrier

// GCC/Clang implementation (simplified):
#if defined(__x86_64__)
  // acquire fence → compiler barrier (no hardware instruction)
  #define ACQUIRE_FENCE() asm volatile("" ::: "memory")

  // release fence → compiler barrier
  #define RELEASE_FENCE() asm volatile("" ::: "memory")

  // seq_cst fence → MFENCE
  #define SEQ_CST_FENCE() asm volatile("mfence" ::: "memory")
#elif defined(__aarch64__)
  #define ACQUIRE_FENCE() asm volatile("dmb ishld" ::: "memory")
  #define RELEASE_FENCE() asm volatile("dmb ish" ::: "memory")
  #define SEQ_CST_FENCE() asm volatile("dmb ish" ::: "memory")
#endif

// "memory" clobber tells the compiler: do not reorder any memory operations across this barrier
```

### 7.2 Fence in LLVM IR

```llvm
; acquire fence
fence acquire

; release fence
fence release

; acq_rel fence
fence acq_rel

; seq_cst fence
fence seq_cst

; The LLVM backend translates these into target-architecture-specific instructions
```

---

## 8. Fence and Release Sequence Interaction

```cpp
// C++20 P0735 standardizes the interaction between fences and release sequences
std::atomic<int> x{0};
int data = 0;

// Thread A: release fence + relaxed store
data = 42;
std::atomic_thread_fence(std::memory_order_release);
x.store(1, std::memory_order_relaxed);

// Thread B: RMW (in the release sequence)
x.fetch_add(1, std::memory_order_relaxed);

// Thread C: acquire fence (reads value from the sequence)
int v = x.load(std::memory_order_relaxed); // reads 2
std::atomic_thread_fence(std::memory_order_acquire);

// C++20 guarantees:
// release fence's effects can propagate through the release sequence
// → data == 42 is visible to Thread C
// C++17 had insufficient clarity in this scenario; P0735 fixed it
```

---

## 9. Performance Considerations

```
Fence overhead on each architecture (typical values):

  Operation                       x86-64    AArch64    POWER
  ─────────────────────────────────────────────────────────
  store(release)                  ~0 cycles  ~10 cycles  ~30 cycles
  load(acquire)                   ~0 cycles  ~10 cycles  ~30 cycles
  release fence                   ~0 cycles  ~10 cycles  ~30 cycles
  acquire fence                   ~0 cycles  ~10 cycles  ~30 cycles
  seq_cst fence                   ~30 cycles ~10 cycles  ~100 cycles
  seq_cst store                   ~30 cycles ~10 cycles  ~100 cycles
  ─────────────────────────────────────────────────────────

  Key observations:
  · On x86, release/acquire fences are nearly free (compiler barrier suffices)
  · On x86, seq_cst fence is expensive (needs MFENCE)
  · On ARM, all fences have cost (DMB instruction)
  · On POWER, all fences are expensive (lwsync/sync)
  · Relaxed operations have zero additional overhead on all architectures
```

---

## 10. Summary

```
Fence usage quick reference:
┌───────────────────────────────────────────────────────────────┐
│ Scenario                        Recommended approach          │
│ ──────────────────────────────────────────────────────────── │
│ Single variable release-acquire store(release) + load(acquire)│
│ Multi-variable release-acquire  fence(release) + fence(acquire)│
│ Need SB elimination (store-load) seq_cst fence or seq_cst ops │
│ CAS operations                  Operation-embedded ordering   │
│ Performance-sensitive ARM code  Use relaxed + precise fences  │
│ Uncertain                       seq_cst (safest default)      │
└───────────────────────────────────────────────────────────────┘

Core principles:
· Fences must pair with atomic operations to establish synchronizes-with
· One-sided fences produce no synchronization effect
· seq_cst fence is the only fence that guarantees global total order
· On x86, release/acquire fences are nearly free — use them freely
· On ARM/POWER, every fence has a cost — use them judiciously
```
