---
title: "Modification Order"
topic: topics
feature: memory-model-modification-order
standard: C++
status_checked_at: 2026-06-02
---

# Modification Order

> Every atomic object has a **modification order** — a total ordering of all write operations to that object. This is one of the most fundamental yet most easily overlooked concepts in the C++ memory model; it constrains the order of writes that different threads can observe.

---

## 1. Basic Definition

The standard [intro.races] specifies: for every scalar object `M`, all modifications to that object form a **total order**. This total order is called the modification order of `M`.

```
Modification order of atomic variable x:

  Timeline (global total order):
  ──────────────────────────────────────────→
  store(x, 1)   store(x, 2)   store(x, 3)
  ──────────────────────────────────────────→
      W₁            W₂            W₃

  Any thread's observed write sequence must be a prefix of this total order:
  · Thread A may see: [W₁] (only the first write)
  · Thread B may see: [W₁, W₂]
  · Thread C may see: [W₁, W₂, W₃]
  · Impossible: a thread first sees W₂ then W₁ (violates total order)
```

The total order of the modification order guarantees a key invariant: **once a thread observes a write W, it cannot "go back" to observe a value that was overwritten before W.**

---

## 2. Coherence Requirements

The coherence requirements of the modification order have three layers:

### 2.1 Monotonic Read

If a read operation `R` reads the value of write operation `W_a`, and there is another read operation `R'` after `R` (sequenced-before within the same thread), then `R'` cannot read a write that precedes `W_a`.

```cpp
std::atomic<int> x{0};

// Thread 1
x.store(1, std::memory_order_relaxed); // W₁
x.store(2, std::memory_order_relaxed); // W₂
x.store(3, std::memory_order_relaxed); // W₃

// Thread 2
int a = x.load(std::memory_order_relaxed); // R₁
int b = x.load(std::memory_order_relaxed); // R₂

// If R₁ == 2, then R₂ cannot be 1 (cannot "go back")
// If R₁ == 2, then R₂ ∈ {2, 3}
```

### 2.2 Write-Write Coherence

Within the same thread, if write operation `W_a` is sequenced-before write operation `W_b`, then in `x`'s modification order, `W_a` must appear before `W_b`.

```cpp
std::atomic<int> x{0};

// Thread 1
x.store(1, std::memory_order_relaxed); // W_a: sequenced-before W_b
x.store(2, std::memory_order_relaxed); // W_b
// In the modification order, W₁(1) must be before W₂(2)

// Thread 2
int val = x.load(std::memory_order_relaxed);
// May read 0 (initial value), 1, or 2
// But if it reads 1, it observed W_a
// Subsequent reads cannot read 0 (monotonicity)
```

### 2.3 Read-Read Coherence

If read operation `R_a` happens-before read operation `R_b` (not just sequenced-before within the same thread), and `R_a` reads from write operation `W`, then `R_b` must read from `W` or from a write that is later than `W` in the modification order.

---

## 3. Read-From Relationship

When a read operation `R` reads the value written by write operation `W`, we say `R` **reads-from** `W` (denoted `R rf W`).

```cpp
std::atomic<int> x{0};

// Thread A
x.store(42, std::memory_order_relaxed); // W

// Thread B
int v = x.load(std::memory_order_relaxed); // R
// If v == 42, then R rf W
```

The read-from relationship is central to memory model reasoning. The C++ standard defines legal executions through the following relationship composition:

```
sequenced-before (sb)     → same-thread ordering
reads-from (rf)           → which write a read observed
modification order (mo)   → write-write total order
synchronizes-with (sw)    → release-acquire synchronization
happens-before (hb)       → transitive closure of sb ∪ sw

An execution is legal ⟺ there exists a choice of mo and rf
such that no reads-from occurs before happens-before.
```

---

## 4. Interaction Between Happens-Before and Modification Order

The happens-before relationship constrains which reads-from relationships are legal:

```
Rule: if W happens-before W', then W must be ordered before W' in the modification order.

Corollary: if R happens-before W (W is ordered after the write that R reads in the modification order),
     then R cannot read the value written by W — because the read must happen before the write.
```

**Key distinction**:
- **Happens-before** is the causal relationship of program logic, established by synchronization operations
- **Modification order** is a total order freely chosen by hardware/compiler (but must be consistent with happens-before)

```cpp
std::atomic<int> x{0};

// Thread A
x.store(1, std::memory_order_release); // W₁ — release

// Thread B
int a = x.load(std::memory_order_acquire); // R₁ — acquire
// If R₁ reads the value from W₁ (i.e., a == 1),
// then W₁ synchronizes-with R₁ → W₁ happens-before R₁

// Due to happens-before transitivity, W₁ happens-before all operations after R₁
// Subsequent reads cannot see modifications earlier than W₁
```

---

## 5. Relaxed Ordering and Modification Order Visibility

`memory_order_relaxed` only guarantees atomicity and modification order consistency; it does not establish a synchronizes-with relationship. This means visibility between threads is entirely determined by the modification order:

```cpp
std::atomic<int> x{0};
std::atomic<int> y{0};

// Thread A
x.store(1, std::memory_order_relaxed); // W_x
y.store(1, std::memory_order_relaxed); // W_y

// Thread B
int a = y.load(std::memory_order_relaxed); // R_y
int b = x.load(std::memory_order_relaxed); // R_x

// Possible result: a == 1 && b == 0
// Reason:
//   · W_x and W_y are independent in their respective modification orders
//   · W_x is sequenced-before W_y in Thread A, but relaxed doesn't establish sw
//   · Thread B can see W_y's effect first, then see a value before W_x
//   · This is the standard result of the store-buffering (SB) litmus test
```

### 5.1 Allowed Reorderings Under relaxed

```
Allowed compiler/CPU behavior:

  Source order:       Actual execution order (legal):
  store(x, 1)    store(y, 1)      ← compiler or CPU can swap
  store(y, 1)    store(x, 1)      ← because relaxed has no ordering constraints

  ┌──────────────────────────────────────────┐
  │ x86 (TSO):                               │
  │   store-store not reordered → won't occur│
  │   spontaneously                           │
  │   But store buffer can cause observation delay│
  │                                          │
  │ ARM (weak ordering):                     │
  │   store-store can be reordered → SB more │
  │   likely to be observed                  │
  └──────────────────────────────────────────┘
```

---

## 6. Store-Buffering (SB) Litmus Test

SB is the most classic weak-consistency litmus test, used to detect store-store reordering:

```cpp
// SB (Store Buffering) litmus test
std::atomic<int> x{0};
std::atomic<int> y{0};

// Initial state: x == 0, y == 0

// Thread A                              // Thread B
x.store(1, std::memory_order_relaxed); // W_x
int r1 = y.load(                     // R_y
    std::memory_order_relaxed);

                                       y.store(1, std::memory_order_relaxed); // W_y
                                       int r2 = x.load(                     // R_x
                                           std::memory_order_relaxed);
```

### 6.1 Possible Results

```
r1 == 0 && r2 == 0 — legal!

  Thread A's perspective:     Thread B's perspective:
  W_x=1 executes first       W_y=1 executes first
  R_y reads 0 (didn't see W_y) R_x reads 0 (didn't see W_x)

  This is completely legal on weak-ordering architectures:
  · W_x enters A's store buffer, not yet visible to B
  · W_y enters B's store buffer, not yet visible to A
  · Both reads fetch old values from their respective caches
```

### 6.2 How to Eliminate the SB Result

```cpp
// Approach 1: seq_cst — simplest and definitely effective
x.store(1, std::memory_order_seq_cst); // W_x
int r1 = y.load(std::memory_order_seq_cst); // R_y

// seq_cst guarantees global total order (SC order), impossible for both to read 0

// Approach 2: seq_cst fence
// Thread A
x.store(1, std::memory_order_relaxed);
std::atomic_thread_fence(std::memory_order_seq_cst);
int r1 = y.load(std::memory_order_relaxed);

// Thread B
y.store(1, std::memory_order_relaxed);
std::atomic_thread_fence(std::memory_order_seq_cst);
int r2 = x.load(std::memory_order_relaxed);

// seq_cst fence guarantees the two fences are ordered in a global total order
// → impossible for r1 == 0 && r2 == 0
```

---

## 7. Hardware Mapping of Modification Order

### 7.1 x86-TSO (Total Store Order)

```
x86's TSO model naturally guarantees:
  · store-store ordering → modification order consistent with program order
  · load-load ordering
  · load-store ordering
  · But store-load may not be ordered (via store buffer)

  So SB test with r1==0 && r2==0 practically never occurs on x86.
  But the C++ standard allows it (compiler may reorder), so synchronization is still needed.
```

### 7.2 ARM/POWER Weak Ordering Model

```
ARM and POWER are weak-consistency architectures:
  · All store-store, load-load, load-store, store-load may be reordered
  · Explicit barriers needed (DMB/DSB on ARM, lwsync/sync on POWER)
  · relaxed operations can be freely reordered
  · Modification order visibility may have significant delay

  Practical implication: SB tests without synchronization on ARM frequently produce r1==0 && r2==0
```

### 7.3 Dual-Level Reordering: Compiler and Hardware

```
Two levels of reordering:

  Source              Compiler Optimization    CPU Execution
  ┌────────┐       ┌────────────┐       ┌──────────────┐
  │ store A │  ──→  │ store B    │  ──→  │ store B      │
  │ store B │       │ store A    │       │ (enters SB)  │
  └────────┘       └────────────┘       │ store A      │
                                         │ (enters SB)  │
                                         └──────────────┘
  Even if source order is A→B, compiler may swap to B→A
  Even if compiler order is A→B, CPU may make it visible as B→A

  C++ atomic operation's memory_order imposes constraints at both the compiler and CPU levels
```

---

## 8. Reasoning About Program Correctness with Modification Order

### 8.1 Flag Guard Pattern

```cpp
std::atomic<int> data{0};
std::atomic<int> flag{0};

// Thread A (producer)
data.store(42, std::memory_order_relaxed); // W_data
flag.store(1, std::memory_order_release);  // W_flag (release)

// Thread B (consumer)
while (flag.load(std::memory_order_acquire) != 1) {} // R_flag (acquire)
int val = data.load(std::memory_order_relaxed);       // R_data

// Correctness reasoning:
// 1. W_data sequenced-before W_flag (same thread)
// 2. In flag's modification order, W_flag is after the initial value 0
// 3. R_flag reads W_flag's value → R_flag rf W_flag
// 4. W_flag release synchronizes-with R_flag acquire
// 5. Therefore W_data happens-before R_data
// 6. R_data must see W_data's effect → val == 42 ✓

// Key: modification order guarantees W_flag is after 0 in flag's mo
//       synchronizes-with establishes happens-before relationship
//       happens-before guarantees data's write is visible
```

### 8.2 Modification Order in Release Sequences

```cpp
std::atomic<int> head{0};

// Modification order constrains release sequences:
// head's modification order: initial(0) → W_a(1) → W_b(2) → ...
// If W_a is a release, W_b is an RMW that reads W_a's value,
// then W_b is also in the release sequence headed by W_a.
// Modification order contiguity is the prerequisite for release sequences to exist.
```

---

## 9. Common Pitfalls and Best Practices

### 9.1 Misconception: relaxed Guarantees Global Consistency

```cpp
// ❌ Wrong understanding: relaxed guarantees all threads see the same modification order
// ✅ Correct understanding: relaxed guarantees each atomic object's own modification order consistency
//             Does not guarantee cross-variable ordering consistency

std::atomic<int> a{0}, b{0};
// Thread 1                    // Thread 2
a.store(1, relaxed);         b.store(1, relaxed);
int rb = b.load(relaxed);    int ra = a.load(relaxed);
// rb == 0 && ra == 0 is legal!
```

### 9.2 Misconception: mo Constrains Other Variables

```cpp
// ❌ Wrong understanding: if W₁ is before W₂ in x's mo, then y's writes can also see the ordering
// ✅ Correct understanding: mo only constrains a single atomic object; there are no constraints between
//    different atomic objects' mo
//    Cross-variable ordering requires happens-before (through release-acquire or seq_cst)
```

### 9.3 Best Practices

```
· Use relaxed only for statistical counters, sequence number generators,
  and other operations with no cross-variable dependencies
· Cross-variable visibility needed → at least release-acquire
· Uncertain → use seq_cst (default, safest)
· Verify correctness → use ThreadSanitizer + litmus test
· Only downgrade ordering after confirming performance bottleneck → test first, then change
```

---

## 10. Summary

```
Key properties of modification order:
┌─────────────────────────────────────────────────────────────┐
│ 1. Each atomic object has one and only one modification order (total order) │
│ 2. Threads cannot "go back" in reads (monotonic read/coherence) │
│ 3. Same-thread writes are ordered in mo (write-write coherence) │
│ 4. mo must be consistent with happens-before               │
│ 5. relaxed only guarantees mo consistency, doesn't establish sync │
│ 6. release-acquire builds cross-thread happens-before on top of mo │
│ 7. seq_cst establishes global total order across all atomic operations │
└─────────────────────────────────────────────────────────────┘
```
