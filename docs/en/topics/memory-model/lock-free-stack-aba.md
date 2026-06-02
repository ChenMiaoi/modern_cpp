---
title: "Lock-Free Stack and ABA Problem"
topic: topics
feature: memory-model-lock-free-stack-aba
standard: C++
status_checked_at: 2026-06-02
---

# Lock-Free Stack and ABA Problem

> The Treiber stack is the classic introductory example of lock-free data structures. Its simplicity conceals a fatal pitfall: the ABA problem. This article deeply analyzes the mechanism behind ABA and introduces three mainstream solutions — tagged pointers, Hazard Pointer, and Epoch-Based Reclamation.

---

## 1. Treiber Stack

### 1.1 Basic Structure

```
Treiber stack (1986, R. Kent Treiber):

  push/pop both modify the head pointer via CAS

  ┌────────┐     ┌────────┐     ┌────────┐
  │ data: C│     │ data: B│     │ data: A│
  │ next: ─┼────→│ next: ─┼────→│ next: ∅│
  └────────┘     └────────┘     └────────┘
       ↑
      head

  push(node):  node->next = head; CAS(&head, node->next, node)
  pop():       old = head; CAS(&head, old, old->next)
```

### 1.2 Complete Implementation

```cpp
template <typename T>
class treiber_stack {
    struct node {
        T data;
        node* next;
        explicit node(T val) : data(std::move(val)), next(nullptr) {}
    };

    std::atomic<node*> head_{nullptr};

public:
    void push(T value) {
        auto* n = new node(std::move(value));
        n->next = head_.load(std::memory_order_relaxed);
        while (!head_.compare_exchange_weak(
            n->next, n,
            std::memory_order_release,
            std::memory_order_relaxed)) {}
    }

    std::optional<T> pop() {
        node* old_head = head_.load(std::memory_order_acquire);
        while (old_head && !head_.compare_exchange_weak(
            old_head, old_head->next,
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {}
        if (!old_head) return std::nullopt;

        T result = std::move(old_head->data);
        // ⚠ Dangerous! Directly deleting old_head causes ABA problem
        delete old_head;
        return result;
    }
};
```

---

## 2. ABA Problem Detailed Analysis

### 2.1 Problem Reproduction

```
Initial state: head → [A] → [B] → [C] → ∅

Timeline:
  T₁ (Thread 1: pop)                T₂ (Thread 2)
  ─────────────────────────────   ──────────────────────
  old = head → A
  old->next → B
  // About to CAS, but gets suspended
                                  pop(): CAS succeeds, head → B
                                  delete A
                                  push(D): new node at A's address
                                  // If allocator reused A's memory address
                                  // then D's address == A's address
                                  pop(): CAS succeeds, head → C
                                  pop(): CAS succeeds, head → ∅
                                  push(A'): new node, address happens to be A's old address
                                  // head → [A'] → ∅
  // Thread 1 wakes up
  CAS(head, A, B)
  // head's current value is indeed "A's address" (but it's A' now!)
  // CAS succeeds! head → B
  // But B's next points to C, which has already been popped!
  // → use-after-free!
```

### 2.2 Root Cause of ABA

```
CAS only compares **values** (pointer addresses), not **versions** or **generations**:

  CAS(&head, expected, desired)
       ↓        ↓         ↓
     address   old value   new value

  CAS succeeds when expected happens to equal the current head value
  But this "equality" could be:
  · The same object (normal case)
  · Different object, allocated at the same address (ABA!)

  In user-space programs using malloc/free, address reuse is common.
  In kernel or embedded systems, if memory is not reclaimed, ABA won't occur.
```

### 2.3 ABA Problem Is Not Just a Stack Problem

```
ABA affects all lock-free structures that use CAS + pointers:

  · Treiber stack (pop operation)
  · Michael-Scott queue (dequeue operation)
  · Lock-free linked list (delete operation)
  · Lock-free hash table (bucket operation)

  Any pattern of "read pointer → compute dependent value → CAS" has ABA risk
```

---

## 3. Solution 1: Tagged Pointers

### 3.1 Core Idea

```
Store a monotonically increasing version number alongside the pointer:
  · CAS compares the (ptr, tag) combination
  · Even if the ptr address is reused, the tag will differ
  · Requires double-width CAS (x86: cmpxchg16b)

  tagged_ptr = {ptr, tag}
  CAS(&head, {A, 5}, {B, 6})  // compare address + version
  → Even if A's address is reused, if tag is not 5, CAS fails
```

### 3.2 Implementation (Using 128-bit CAS)

```cpp
#include <atomic>
#include <cstdint>

struct tagged_ptr {
    void*      ptr;
    uint64_t   tag;

    bool operator==(const tagged_ptr& o) const {
        return ptr == o.ptr && tag == o.tag;
    }
};

static_assert(sizeof(tagged_ptr) == 16);

template <typename T>
class aba_safe_stack {
    struct node {
        T data;
        node* next;
        explicit node(T val) : data(std::move(val)), next(nullptr) {}
    };

    // Needs 16-byte alignment for cmpxchg16b
    struct alignas(16) atomic_tagged_ptr {
        tagged_ptr value;
    };

    atomic_tagged_ptr head_{};

public:
    void push(T val) {
        auto* n = new node(std::move(val));
        tagged_ptr old_head = head_.value.load(std::memory_order_relaxed);
        tagged_ptr new_head;
        do {
            n->next = static_cast<node*>(old_head.ptr);
            new_head = {n, old_head.tag + 1};
        } while (!cas_head(old_head, new_head));
    }

    std::optional<T> pop() {
        tagged_ptr old_head = head_.value.load(std::memory_order_acquire);
        tagged_ptr new_head;
        do {
            if (old_head.ptr == nullptr) return std::nullopt;
            auto* node = static_cast<struct node*>(old_head.ptr);
            new_head = {node->next, old_head.tag + 1};
        } while (!cas_head(old_head, new_head));

        auto* node = static_cast<struct node*>(old_head.ptr);
        T result = std::move(node->data);
        delete node;
        return result;
    }

private:
    bool cas_head(tagged_ptr& expected, tagged_ptr desired) {
        return __atomic_compare_exchange(
            &head_.value, &expected, &desired,
            false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    }
};
```

### 3.3 Pointer Packing in 64-bit Environments

```cpp
// Alternative: pack the tag into a 64-bit pointer
// Prerequisite: user-space virtual addresses only use 48 bits (or fewer)

// Method 1: use high bits
// User-space address range: 0x0000_0000_0000 ~ 0x0000_7FFF_FFFF_FFFF (47 bits)
// Top 16 bits can be used for tag (but watch out for sign extension)

// Method 2: use alignment to guarantee low bits are zero
// If nodes are at least 16-byte aligned, low 4 bits are 0
// Can store tag in low 4 bits — but only 16 values, easy to overflow

// Method 3: compressed pointer + high-bit tag (most practical)
struct packed_tagged_ptr {
    uint64_t value;

    static constexpr int TAG_BITS = 16;
    static constexpr uint64_t PTR_MASK = (1ULL << 48) - 1;
    static constexpr uint64_t TAG_MASK = ~PTR_MASK;

    void* ptr() const {
        // Sign extension: 48-bit → 64-bit
        uint64_t p = value & PTR_MASK;
        if (p & (1ULL << 47)) p |= ~PTR_MASK; // negative address extension
        return reinterpret_cast<void*>(p);
    }

    uint64_t tag() const { return (value & TAG_MASK) >> 48; }

    static packed_tagged_ptr make(void* p, uint64_t t) {
        uint64_t v = (reinterpret_cast<uint64_t>(p) & PTR_MASK)
                   | (t << 48);
        return {v};
    }
};

// Use ordinary 64-bit CAS
std::atomic<uint64_t> head;

// CAS compares the full 64 bits (address + tag)
```

### 3.4 Limitations of Tagged Pointers

```
Limitations:
  · 64-bit environment only has 16-bit tag → tag wraps after 65536 operations
    (but probability of wrapping to the same tag is extremely low,
     requires the exact same address to be involved)
  · 128-bit CAS is 2-3x slower on x86
  · ARM has no native 128-bit CAS, needs LDAXP/STLXP loop
  · Pointer packing increases code complexity and portability issues
  · Limited tag bits, extremely high contention still has small ABA probability

  Suitable scenarios:
  · Simple stack/queue, low to moderate contention
  · Embedded or kernel environments, no general-purpose memory allocator needed
  · Extreme performance requirements, cannot accept hazard pointer overhead
```

---

## 4. Solution 2: Hazard Pointer

(See hazard-pointer.md for details; this is an overview)

```
Hazard Pointer core idea (Maged Michael, 2004):

  1. Each thread has one (or more) hazard pointer slots
  2. Before accessing a shared node, a thread sets its hazard pointer to the node address
  3. Before releasing a node, other threads scan all threads' hazard pointers
  4. Only nodes not protected by any hazard pointer are actually deleted

  ┌───────────────────────────────────────────────────┐
  │ Thread 1: hp = node_A;  // "I am accessing A"     │
  │ Thread 2: wants to delete A → scans hp → finds Thread 1 protects A │
  │         → places A in retire list, does not delete │
  │ Thread 1: hp = nullptr;  // "I am no longer accessing A" │
  │ Thread 2: next scan → A no longer protected → safe to delete │
  └───────────────────────────────────────────────────┘

  Advantage: no double-width CAS needed, no address space tricks
  Disadvantage: retire list management, scan overhead, deferred memory release
```

---

## 5. Solution 3: Epoch-Based Reclamation

```
Epoch-Based Reclamation (EBR) core idea:

  1. Maintain a global epoch counter (typically 0, 1, 2 cycling)
  2. Each thread records its current epoch
  3. A thread "announces" the current epoch when entering a critical section
  4. Clears it when exiting the critical section
  5. Reclamation: only when all threads have left an old epoch are objects from that epoch reclaimed

  ┌───────────────────────────────────────────────────┐
  │ global_epoch = 2                                  │
  │                                                   │
  │ Thread 1: local_epoch = 2 (active)                │
  │ Thread 2: local_epoch = 2 (active)                │
  │ Thread 3: local_epoch = 1 (lagging)               │
  │                                                   │
  │ epoch 0 retire list: [A, B, C]                    │
  │ → cannot reclaim (Thread 3 still on epoch 1)      │
  │                                                   │
  │ Thread 3 advances to epoch 2                      │
  │ → now safe to reclaim [A, B, C] from epoch 0      │
  └───────────────────────────────────────────────────┘

  Advantage: simple to implement, good performance (only one additional atomic load per critical section entry)
  Disadvantage: a single lagging thread blocks all reclamation (the "notorious laggard" problem)
```

---

## 6. Complete Lock-Free Stack Using Tagged Pointers

```cpp
template <typename T>
class tagged_lock_free_stack {
    struct node {
        T data;
        node* next;
        explicit node(T val) : data(std::move(val)), next(nullptr) {}
    };

    struct alignas(16) tagged {
        node*    ptr = nullptr;
        uint64_t tag = 0;

        bool operator==(const tagged& o) const {
            return ptr == o.ptr && tag == o.tag;
        }
    };

    std::atomic<tagged> head_;

    bool cas(tagged& expected, tagged desired) {
        return __atomic_compare_exchange(
            &head_, &expected, &desired,
            false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    }

public:
    tagged_lock_free_stack() : head_{{nullptr, 0}} {}

    void push(T value) {
        auto* n = new node(std::move(value));
        tagged old = head_.load(std::memory_order_relaxed);
        tagged desired;
        do {
            n->next = old.ptr;
            desired = {n, old.tag + 1};
        } while (!cas(old, desired));
    }

    std::optional<T> pop() {
        tagged old = head_.load(std::memory_order_acquire);
        tagged desired;
        node* n;
        do {
            n = old.ptr;
            if (!n) return std::nullopt;
            desired = {n->next, old.tag + 1};
        } while (!cas(old, desired));

        T result = std::move(n->data);
        delete n;
        return result;
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire).ptr == nullptr;
    }
};
```

### 6.1 Verifying ABA Safety

```
ABA safety verification:

  Assume Thread 1 is suspended during pop(), and a complete ABA cycle occurs:
  · head changes from {A, 5} to {B, 6} to {A', 7}

  Thread 1 wakes up, attempts CAS:
  · expected = {A, 5}
  · current  = {A', 7}
  · ptr is the same (A's address), but tag differs (5 ≠ 7)
  · CAS fails ✓

  Thread 1 re-reads head = {A', 7}, recalculates desired = {B', 8}
  Now it sees the correct next pointer

  Conclusion: 64-bit tag makes ABA practically impossible
  (requires exactly 2^64 operations for the tag to wrap to the same value)
```

---

## 7. x86 DCAS ABA Prevention

```
x86 cmpxchg16b instruction:

  ; Input: RDX:RAX = expected, RCX:RBX = desired
  ; Operation: atomically compare [mem] with RDX:RAX
  ;       if equal, write RCX:RBX, set ZF=1
  ;       if not equal, read [mem] into RDX:RAX, set ZF=0
  lock cmpxchg16b [mem]

  Usage in C++:
  struct alignas(16) tagged_ptr { void* p; uint64_t t; };
  std::atomic<tagged_ptr> ap;

  tagged_ptr exp = ap.load();
  tagged_ptr des = {new_ptr, exp.t + 1};
  while (!ap.compare_exchange_weak(exp, des)) {
      des.p = computed_ptr;
      des.t = exp.t + 1;
  }

  Notes:
  · Must be 16-byte aligned (otherwise #GP exception)
  · 2-3x slower than cmpxchg8b
  · May be trap-and-emulated in virtualization environments (even slower)
  · Not all CPUs support it (requires CX16 CPUID feature)
```

---

## 8. Solution Comparison

```
┌──────────────┬──────────────┬───────────────┬──────────────┐
│              │ Tagged Ptr   │ Hazard Ptr    │ Epoch-Based  │
├──────────────┼──────────────┼───────────────┼──────────────┤
│ CAS width    │ 128-bit      │ 64-bit        │ 64-bit       │
│ Memory cost  │ None extra   │ O(N×T)        │ O(T)         │
│ Reclaim delay│ Immediate    │ Deferred (scan)│ Deferred (epoch)│
│ Complexity   │ Medium       │ Higher         │ Lower        │
│ ABA safety   │ Physical     │ Logical        │ Logical      │
│ Suitable for │ Simple struct│ General-purpose│ Read-heavy   │
│ Portability  │ Poor (DCAS)  │ Good           │ Good         │
│ Laggard impact│ None        │ None           │ Blocks reclaim│
└──────────────┴──────────────┴───────────────┴──────────────┘
```

---

## 9. What Happens Without Solving ABA

```cpp
// Real ABA crash scenario (simplified):
// In pop(), after deleting old_head, the memory is reclaimed by the allocator
// A new node happens to be allocated at the same address
// CAS succeeds but the next pointer is now invalid

// Consequences:
// 1. use-after-free → segfault (best case)
// 2. Data corruption → silent errors (worst case)
// 3. Security vulnerability → exploitable
//
// In production, ABA-caused crashes are usually extremely hard to reproduce
// because they depend on specific thread scheduling and memory allocation timing
// TSan (ThreadSanitizer) can help detect some scenarios
// but cannot cover all ABA patterns
```

---

## 10. Summary

```
ABA problem quick reference:
┌─────────────────────────────────────────────────────────────┐
│ Problem: CAS compares values (addresses), cannot distinguish│
│          different generations of objects                    │
│ Trigger: read → suspend → address reused → CAS succeeds     │
│          but semantics are wrong                             │
│                                                              │
│ Solution selection:                                          │
│ · Simple struct + x86-only → Tagged pointer (cmpxchg16b)   │
│ · General struct + portable → Hazard Pointer                │
│ · Read-heavy + simple impl → Epoch-Based Reclamation        │
│ · No reclamation needed → ABA not a concern (e.g. push-only)│
└─────────────────────────────────────────────────────────────┘
```
