---
title: "CAS Operations and Memory Ordering (Compare-And-Swap Ordering)"
topic: topics
feature: memory-model-cas-ordering
standard: C++
status_checked_at: 2026-06-02
---

# CAS Operations and Memory Ordering (Compare-And-Swap Ordering)

> compare_exchange is the core primitive of lock-free programming. It allows threads to atomically compare and conditionally update shared variables, forming the cornerstone of lock-free data structures. Understanding CAS's success/failure ordering combinations, spurious failure, and CAS loop patterns is a prerequisite for correctly writing lock-free code.

---

## 1. compare_exchange_weak vs compare_exchange_strong

```cpp
// C++ atomic interface
bool compare_exchange_weak(T& expected, T desired,
                           std::memory_order success,
                           std::memory_order failure);

bool compare_exchange_strong(T& expected, T desired,
                             std::memory_order success,
                             std::memory_order failure);
```

### 1.1 weak vs strong Differences

```
compare_exchange_weak:
  · May return false even when *this == expected (spurious failure)
  · More efficient on LL/SC architectures (ARM, POWER, RISC-V)
  · Must be used in a loop

compare_exchange_strong:
  · Returns false only when *this != expected
  · Internally may be a CAS loop (wraps a loop on LL/SC)
  · No user-written loop needed, but internally may retry

Hardware mapping:
  ┌──────────────────────────────────────────────┐
  │ x86:                                         │
  │   Both weak and strong compile to CMPXCHG    │
  │   x86's CAS instruction doesn't produce spurious failure │
  │   → weak and strong have identical performance │
  │                                              │
  │ ARM/POWER/RISC-V:                            │
  │   weak → LR/SC instruction pair (ldxr/stxr) │
  │   strong → LR/SC loop                        │
  │   weak is more efficient (avoids loop overhead)│
  └──────────────────────────────────────────────┘
```

### 1.2 Implementation on LL/SC Architectures

```
compare_exchange_weak on ARM AArch64:

  ldxr  w0, [x1]       // Load-Exclusive (LL)
  cmp   w0, w2          // compare *this with expected
  b.ne  fail
  stxr  w3, w4, [x1]   // Store-Exclusive (SC)
  // If stxr fails (another core interfered with the cache line), return false
  // This is the hardware source of spurious failure

compare_exchange_strong on ARM AArch64:

  loop:
    ldxr  w0, [x1]     // LL
    cmp   w0, w2
    b.ne  fail
    stxr  w3, w4, [x1] // SC
    cbnz  w3, loop      // SC failed, retry
  fail:
    ...
```

---

## 2. Success/Failure Ordering Combinations

CAS has two ordering parameters: one for success and one for failure. The standard imposes constraints on failure ordering.

### 2.1 Legal Ordering Combinations

```
success ordering    failure ordering    Validity
──────────────────────────────────────────────────
relaxed             relaxed             ✅
acquire             acquire             ✅
release             relaxed             ✅
acq_rel             acquire             ✅
seq_cst             seq_cst             ✅
──────────────────────────────────────────────────
release             acquire             ❌ Invalid
acq_rel             relaxed             ❌ Invalid
acquire             relaxed             ❌ Invalid (failure cannot be weaker than success)
seq_cst             acquire             ✅
──────────────────────────────────────────────────

Constraint rules:
  1. failure ordering cannot be release or acq_rel
  2. failure ordering cannot be stronger than success ordering
  3. If success is release, failure can only be relaxed
  4. If success is acquire, failure can only be acquire or relaxed
```

### 2.2 Why the Failure Branch Cannot Be release/acq_rel

```cpp
// When CAS fails, it only reads the current value, no write occurs
// release semantics mean: reads/writes before this write cannot be reordered after it
// But the failure branch has no write! release semantics have no meaning

x.compare_exchange_weak(expected, desired,
    std::memory_order_acq_rel,     // success: both read and write sync
    std::memory_order_acquire);    // failure: only read sync needed

// Imagine the hardware behavior when CAS fails:
// Only loads the current value into expected → only need to constrain load ordering
// → acquire is sufficient
```

### 2.3 Typical Combinations

```cpp
// Pattern 1: lock-free counter (relaxed/relaxed)
// Only needs atomicity, no synchronization
int expected = counter.load(std::memory_order_relaxed);
while (!counter.compare_exchange_weak(
    expected, expected + 1,
    std::memory_order_relaxed,  // success
    std::memory_order_relaxed)) // failure
{}

// Pattern 2: flag operation (acq_rel/acquire)
// Needs read-write sync on success, only read sync on failure
int expected = flag.load(std::memory_order_relaxed);
while (!flag.compare_exchange_weak(
    expected, expected | FLAG_BIT,
    std::memory_order_acq_rel,   // success: read+write sync
    std::memory_order_acquire))  // failure: read sync
{}

// Pattern 3: global ordering (seq_cst/seq_cst)
// When strongest guarantee is needed
int expected = x.load(std::memory_order_seq_cst);
while (!x.compare_exchange_weak(
    expected, desired,
    std::memory_order_seq_cst,  // success
    std::memory_order_seq_cst)) // failure
{}
```

---

## 3. Spurious Failure

### 3.1 Causes

```
Sources of spurious failure:

  1. LL/SC hardware level (ARM, POWER, RISC-V)
     · LL (Load-Exclusive/Link) sets a monitor flag
     · Between LL and SC, if any of the following occurs, SC fails:
       a. Another core wrote to the same cache line
       b. This core had a context switch (some implementations)
       c. Cache line was evicted and reloaded
       d. Interrupt handler accessed that cache line
     · In these cases, even if the value didn't change, CAS returns false

  2. Compiler implementation level
     · Some compilers insert yield hints in weak CAS loops

  3. Does not affect correctness
     · The expected parameter is updated to the current value on spurious failure
     · The loop will retry with the new value
```

### 3.2 weak Must Be Used in a Loop

```cpp
// ❌ Wrong: weak without a loop
if (x.compare_exchange_weak(expected, desired,
    std::memory_order_acq_rel,
    std::memory_order_acquire)) {
    // CAS succeeded
} else {
    // CAS failed — may be spurious failure!
    // Should not do "failure handling" here, because it may just be spurious
}

// ✅ Correct: weak in a loop
while (!x.compare_exchange_weak(expected, desired,
    std::memory_order_acq_rel,
    std::memory_order_acquire)) {
    // expected has been automatically updated to current value
    // Can do other operations here (e.g., modify desired)
}

// ✅ Or use strong
if (x.compare_exchange_strong(expected, desired,
    std::memory_order_acq_rel,
    std::memory_order_acquire)) {
    // CAS definitively succeeded
}
```

---

## 4. CAS Loop Patterns

### 4.1 Basic CAS Loop

```cpp
// Atomic increment (equivalent implementation of fetch_add)
std::atomic<int> counter{0};

int old_val = counter.load(std::memory_order_relaxed);
int new_val;
do {
    new_val = old_val + 1;
} while (!counter.compare_exchange_weak(
    old_val, new_val,
    std::memory_order_relaxed,
    std::memory_order_relaxed));
// old_val is automatically updated on each failure
```

### 4.2 CAS Loop with Computation

```cpp
// Atomic multiplication
std::atomic<int> x{0};

int old_val = x.load(std::memory_order_relaxed);
int new_val;
do {
    new_val = old_val * 2;
    // Overflow checks, boundary checks, etc. can be added here
    if (new_val > MAX_VALUE) break;
} while (!x.compare_exchange_weak(old_val, new_val,
    std::memory_order_acq_rel,
    std::memory_order_acquire));
```

### 4.3 CAS Loop with Condition

```cpp
// Update only when the value satisfies a condition
std::atomic<int> balance{1000};

int current = balance.load(std::memory_order_acquire);
while (current >= 100) {  // Condition: balance >= 100
    int new_balance = current - 100;
    if (balance.compare_exchange_weak(current, new_balance,
        std::memory_order_acq_rel,
        std::memory_order_acquire)) {
        // Deduction succeeded
        break;
    }
    // current has been updated to latest value, loop re-checks condition
}
```

### 4.4 CAS Loop Performance Issues

```
Problems with CAS loops under high contention:

  N threads CAS-ing simultaneously:
  ┌───────────────────────────────────────────────┐
  │ Thread 1: load val=5, CAS(5,6) → success     │
  │ Thread 2: load val=5, CAS(5,6) → failure     │
  │ Thread 2: load val=6, CAS(6,7) → failure     │
  │ Thread 3: load val=6, CAS(6,7) → success     │
  │ Thread 2: load val=7, CAS(7,8) → success     │
  │ ...                                           │
  │ Thread N: may need N retries                  │
  └───────────────────────────────────────────────┘

  Problems:
  · Many failed CAS operations waste bus bandwidth
  · O(N²) level bus transactions
  · Cache line "ping-ponging" between cores

  Optimizations:
  1. fetch_add instead of CAS loop (e.g., counter scenarios)
  2. Backoff (exponential backoff)
  3. Sharding
  4. Combine relaxed + local batch processing
```

---

## 5. ABA Problem Introduction

### 5.1 Problem Description

```
ABA is the most classic CAS pitfall:

  Timeline ──────────────────────────────────────────────→

  Thread 1: reads A (head → node_A)
          // gets suspended... woken
          CAS(head, A, new_node) → succeeds
          // But head is no longer the original node_A!
          // It was deleted and reallocated (happens to have same address)

  Thread 2: reads A (head → node_A)
          CAS(head, A, node_B) → succeeds (head is now B)
          delete node_A
          allocate new node — happens to reuse node_A's address!
          CAS(head, B, node_A) → succeeds (head is "A" again)

  ┌──────────────────────────────────────────────┐
  │ Initial state: head → [A] → [C] → ...       │
  │                                              │
  │ Thread 2: pop A                              │
  │   head → [B] → [C] → ...                   │
  │   delete A                                   │
  │                                              │
  │ Thread 2: push new node (reuses A's address) │
  │   head → [A'] → [B] → [C] → ...            │
  │                                              │
  │ Thread 1: CAS(head, A, new_node) → succeeds! │
  │   head → [new] → [B] → [C] → ...           │
  │   But Thread 1 thinks A→C's next still valid │
  │   Actually A'→B, lost B and C!              │
  └──────────────────────────────────────────────┘

  Core problem: CAS only compares values (addresses), not "version" or "generation"
```

### 5.2 ABA Problem Solutions Preview

```
Solution overview (see lock-free-stack-aba.md for details):

  1. Tagged Pointers
     · Store version number in high bits of pointer
     · CAS compares "address + version"
     · Requires double-width CAS (x86: cmpxchg16b)

  2. Hazard Pointers
     · Protect nodes currently being accessed from reclamation
     · Deferred reclamation + safe scanning
     · See hazard-pointer.md

  3. Epoch-Based Reclamation
     · Global epoch counter
     · Threads update epoch on entering/exiting critical sections
     · Only reclaim "unreferenced" nodes

  4. RCU (Read-Copy-Update)
     · Linux kernel's solution
     · Zero overhead for readers, writers bear copy + sync costs
```

---

## 6. Double CAS (cmpxchg16b)

### 6.1 128-bit CAS on x86

```cpp
// x86-64's cmpxchg16b instruction can atomically compare and swap 16 bytes of data
// Typical usage: tagged pointers (pointer + version tag)

// GCC/Clang built-in:
struct tagged_ptr {
    void* ptr;
    uint64_t tag;
};

// Needs 16-byte alignment
alignas(16) std::atomic<tagged_ptr> head;

// Using cmpxchg16b:
bool cas(tagged_ptr& expected, tagged_ptr desired) {
    return __atomic_compare_exchange(
        &head, &expected, &desired,
        false,                       // not weak
        __ATOMIC_SEQ_CST,
        __ATOMIC_SEQ_CST
    );
}

// Compiler generates:
// lock cmpxchg16b [head]
// RAX:RCX compared with [head]
// If equal, write RDX:RBX
// If not equal, read [head] into RAX:RCX
```

### 6.2 Tagged Pointer Packing

```cpp
// On 64-bit systems, user-space pointers typically only use 48 bits
// The remaining 16 bits can store a tag

// Method 1: use unused bits in virtual address space
// User-space address space: 0x0000_0000_0000_0000 ~ 0x0000_7FFF_FFFF_FFFF (47 bits)
// Top 17 bits can be used for tagging (but watch for pointer sign extension)

// Method 2: use alignment to guarantee low bits are zero
// If objects are 8-byte aligned, low 3 bits are 0, can store tag
// But 3-bit tag is too small, easy to overflow

// Method 3: use 128-bit struct + cmpxchg16b (most reliable)
struct alignas(16) tagged_ptr {
    void*      ptr;  // 64-bit pointer
    uint64_t   tag;  // 64-bit version number — will never overflow

    // Full-width CAS: compares both ptr and tag simultaneously
};
```

### 6.3 cmpxchg16b Performance

```
cmpxchg16b performance characteristics:

  · ~2-3x slower than cmpxchg8b (64-bit CAS)
  · Locks the entire cache line (typically 64 bytes)
  · Degrades significantly under high contention
  · ARM has no native 128-bit CAS (needs LL/SC or LDAXP/STLXP)
  · Apple M-series chips have good LDAXP/STLXP performance

  Practical usage advice:
  · If tagged pointers are needed, prefer tagged pointer + cmpxchg16b
  · If more flexible versioning is needed, use hazard pointer or epoch-based
```

---

## 7. CAS Hardware Implementation

### 7.1 x86 CMPXCHG

```
x86 CMPXCHG instruction flow:

  CMPXCHG [mem], src:
    if (RAX == [mem]):
        [mem] = src      // comparison succeeded, write new value
        ZF = 1           // set zero flag
    else:
        RAX = [mem]      // comparison failed, read current value into RAX
        ZF = 0

  LOCK CMPXCHG:
    · LOCK prefix locks the cache line (or bus lock)
    · The entire operation is atomic
    · Cache coherence protocol ensures other cores see atomicity

  Cache line state transitions (MESI protocol):
    1. Initiating core sets cache line to Exclusive/Modified
    2. Other cores' corresponding cache lines set to Invalid
    3. Execute comparison and conditional write
    4. Release lock
```

### 7.2 ARM LL/SC

```
ARM Load-Exclusive / Store-Exclusive:

  ldxr x0, [x1]    // Load-Exclusive: read and set exclusive monitor
  // ... compute new value ...
  stxr w2, x3, [x1] // Store-Exclusive: conditional write
  cbnz w2, retry     // If SC failed (w2 != 0), retry

  Exclusive Monitor operation:
  ┌─────────────────────────────────────────────┐
  │ Core maintains an exclusive monitor (flag)  │
  │                                             │
  │ ldxr sets monitor, marking that cache line  │
  │ Monitor is cleared in the following cases:  │
  │   · Another core wrote to the same cache line│
  │   · This core executed clrex or another ldxr│
  │   · Context switch (implementation-dependent)│
  │                                             │
  │ stxr checks if monitor is still valid       │
  │   · Valid → write succeeds, w2=0            │
  │   · Invalid → write fails, w2=1 (spurious) │
  └─────────────────────────────────────────────┘
```

---

## 8. In Practice: Lock-Free Stack (CAS Version Preview)

```cpp
// Simplified Treiber stack (CAS core logic)
template <typename T>
class lock_free_stack {
    struct node {
        T data;
        node* next;
        node(T val) : data(std::move(val)), next(nullptr) {}
    };

    std::atomic<node*> head_{nullptr};

public:
    void push(T value) {
        auto* new_node = new node(std::move(value));
        new_node->next = head_.load(std::memory_order_relaxed);
        // CAS loop: try to change head from old_head to new_node
        while (!head_.compare_exchange_weak(
            new_node->next,    // expected: old head
            new_node,          // desired: new node
            std::memory_order_release,
            std::memory_order_relaxed))
        {}
        // new_node->next is automatically updated to the latest head on each failure
    }

    std::optional<T> pop() {
        node* old_head = head_.load(std::memory_order_acquire);
        while (old_head && !head_.compare_exchange_weak(
            old_head,           // expected: current head
            old_head->next,     // desired: head->next
            std::memory_order_acq_rel,
            std::memory_order_acquire))
        {}
        if (!old_head) return std::nullopt;

        T result = std::move(old_head->data);
        // ⚠ ABA problem here: deleting old_head may cause address reuse
        // See lock-free-stack-aba.md for solutions
        return result;
    }
};
```

---

## 9. CAS Pattern Quick Reference

```
┌─────────────────────────────────────────────────────────────────┐
│ Pattern                  CAS Type    Ordering                   │
│─────────────────────────────────────────────────────────────────│
│ Simple counter           weak        relaxed/relaxed            │
│ Flag set/clear           weak        acq_rel/acquire            │
│ Pointer swap             weak        acq_rel/acquire            │
│ Linked list insert       strong      release/relaxed            │
│ Linked list delete (pop) strong      acq_rel/acquire            │
│ Global ordering needed   weak        seq_cst/seq_cst            │
│ Double CAS (16B)         strong      seq_cst/seq_cst            │
│─────────────────────────────────────────────────────────────────│
│ weak for inside loops    strong for inside loops or single attempt│
│ If unsure                use seq_cst (safest default)           │
└─────────────────────────────────────────────────────────────────┘
```
