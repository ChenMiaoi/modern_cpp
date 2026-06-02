---
title: "Release Sequence"
topic: topics
feature: memory-model-release-sequence
standard: C++
status_checked_at: 2026-06-02
---

# Release Sequence

> Release sequence is one of the most subtle constructs in the C++ memory model: it allows a release operation to propagate its synchronization semantics through a chain of read-modify-write operations, so that a distant acquire read can still establish a synchronizes-with relationship with the original release.

---

## 1. Definition

The standard [atomics.order] defines:

> A release sequence headed by a release operation `A` is the maximal contiguous subsequence of the modification order of an atomic object `M` satisfying:
> 1. The first operation is `A` (the release write)
> 2. Each subsequent operation is a read-modify-write operation on `M`
> 3. Each RMW reads the value written by the immediately preceding operation in the modification order

```
Modification order of atomic object M:

  ──────────────────────────────────────────────────────────→
  W₀(initial)  A(rel)  RMW₁  RMW₂  RMW₃  W_other(rel)  ...
  ──────────────────────────────────────────────────────────→
                ↑
            release head
                │
                └── release sequence = {A, RMW₁, RMW₂, RMW₃}
                    (ends at W_other because W_other is a new release
                      write that doesn't read the preceding operation's value)
```

---

## 2. Why Release Sequences Are Needed

Consider a typical lock-free scenario: multiple threads append operations to the same atomic variable through CAS loops.

```cpp
std::atomic<int> shared_counter{0};

// Thread A: publish initial value (release)
shared_counter.store(100, std::memory_order_release); // A (release head)

// Thread B: CAS increment (RMW)
int expected = shared_counter.load(std::memory_order_relaxed);
while (!shared_counter.compare_exchange_weak(
    expected, expected + 1,
    std::memory_order_acq_rel,   // success: RMW
    std::memory_order_relaxed    // failure: relaxed
)) {}

// Thread C: CAS increment (RMW)
expected = shared_counter.load(std::memory_order_relaxed);
while (!shared_counter.compare_exchange_weak(
    expected, expected + 1,
    std::memory_order_acq_rel,
    std::memory_order_relaxed
)) {}

// Thread D: read final value (acquire)
int val = shared_counter.load(std::memory_order_acquire); // D (acquire)
```

**Problem**: Thread D's acquire read reads the value written by Thread C's RMW. Can Thread D establish a synchronizes-with relationship with Thread A's release?

```
Without release sequences:
  A(release) → C's RMW → D(acquire)
  But there's no direct synchronizes-with between A and C's RMW
  The synchronization chain between A and D is broken

With release sequences:
  A is the release head
  Both B's RMW and C's RMW read the preceding operation's value
  → {A, B's RMW, C's RMW} form a release sequence
  D reads C's RMW's value → D's acquire synchronizes with A's release
  → A happens-before D ✓
```

---

## 3. Rules for Constructing Release Sequences

### 3.1 Must Be RMW

Every operation in a release sequence (except the head) **must be a read-modify-write** operation. A plain load or store breaks the sequence.

```cpp
std::atomic<int> x{0};

x.store(1, std::memory_order_release);          // A (head)
int v = x.fetch_add(1, std::memory_order_acq_rel); // RMW, in sequence
v = x.fetch_sub(1, std::memory_order_acq_rel);     // RMW, in sequence
x.store(5, std::memory_order_relaxed);             // ❌ Plain store, breaks sequence
int w = x.fetch_add(1, std::memory_order_acq_rel);  // New sequence starts here (but no release head)
```

### 3.2 Same Atomic Object

All operations in a release sequence must act on the **same** atomic object.

```cpp
std::atomic<int> x{0}, y{0};
x.store(1, std::memory_order_release);    // head (on x)
// RMWs on y are not in this sequence, because they act on a different object
```

### 3.3 Contiguity in Modification Order

The sequence must be a **contiguous** subsequence in the modification order. If an operation not belonging to the sequence (such as a plain store) is inserted in the middle, the sequence is broken.

### 3.4 Types of Release Head

The release head can be:
- `store` with `memory_order_release`
- `exchange` or CAS success branch with `memory_order_release` or `memory_order_acq_rel`
- `fetch_add` and other RMWs with `memory_order_release` or `memory_order_acq_rel`

---

## 4. Synchronizes-With Establishment Conditions

```
Rule [atomics.order]:

  If the following conditions are all satisfied, a synchronizes-with relationship exists:
  1. A release sequence's head is a release operation A
  2. Operation B is an acquire read on that atomic object
  3. B reads the value written by some operation in the release sequence

  → A synchronizes-with B
```

```cpp
// Complete example
std::atomic<int> flag{0};
int data = 0;

// Thread 1: release write
data = 42;
flag.store(1, std::memory_order_release); // A

// Thread 2: RMW (doesn't change synchronization effect)
flag.fetch_add(0, std::memory_order_acq_rel); // RMW₁: 1→1 (no-op RMW)

// Thread 3: RMW
flag.fetch_add(0, std::memory_order_acq_rel); // RMW₂: 1→1

// Thread 4: acquire read
int v = flag.load(std::memory_order_acquire); // B
if (v >= 1) {
    // release sequence = {A, RMW₁, RMW₂}
    // B reads the value written by RMW₂ → A synchronizes-with B
    assert(data == 42); // guaranteed to pass
}
```

---

## 5. Interaction Between Acquire Fence and Release Sequences

`std::atomic_thread_fence(memory_order_acquire)` can be used together with release sequences:

```cpp
std::atomic<int> flag{0};
int data = 0;

// Thread 1
data = 100;
flag.store(1, std::memory_order_release); // A

// Thread 2
flag.fetch_add(1, std::memory_order_relaxed); // RMW

// Thread 3
int v = flag.load(std::memory_order_relaxed); // reads 2
std::atomic_thread_fence(std::memory_order_acquire); // acquire fence

// Here:
// · flag's release sequence includes {A, Thread 2's RMW}
// · Thread 3's load reads the value from the RMW
// · acquire fence semantics: all relaxed loads before the fence
//   are treated as if they were acquire loads
// · Therefore A synchronizes-with the fence
// · data's write before A happens-before code after the fence
assert(data == 100); // guaranteed to pass
```

---

## 6. C++20 Changes (P0735)

P0735 ("Interaction of `atomic_thread_fence` with release sequences") made significant modifications to C++17's acquire fence interaction with release sequences.

### 6.1 C++17 Problem

In the C++17 standard, the interaction between acquire fences and release sequences had ambiguity. Specifically, when an acquire fence is followed by a relaxed load that reads from a value in a release sequence, the rules for whether the fence synchronizes with the release head were insufficiently clear, causing difficulties for implementers.

### 6.2 P0735 Solution

P0735 introduced the concept of **release fence sequence**, unifying fence synchronization semantics into the sequence model:

```cpp
// C++20 behavior
std::atomic<int> x{0};
int data = 0;

// Thread 1
data = 42;
std::atomic_thread_fence(std::memory_order_release); // release fence
x.store(1, std::memory_order_relaxed);               // relaxed store

// Thread 2: RMW
x.fetch_add(1, std::memory_order_relaxed);           // 1→2

// Thread 3
int v = x.load(std::memory_order_relaxed);           // reads 2
std::atomic_thread_fence(std::memory_order_acquire); // acquire fence

// C++20: release fence → relaxed store(x,1) → RMW → relaxed load(x,2) → acquire fence
// Forms a fence-based synchronizes-with relationship
// data == 42 is guaranteed visible
```

### 6.3 Implementation Impact

```
┌──────────────────────────────────────────────────────────┐
│ P0735 impact on compilers and hardware:                  │
│                                                          │
│ · release fence's hardware effect (e.g., x86 MFENCE,    │
│   ARM DMB) must extend to subsequent relaxed stores      │
│ · acquire fence's effect must extend to preceding relaxed loads │
│ · Implementation is simpler, semantics are clearer       │
│ · However: fences on some weak-ordering hardware remain expensive│
└──────────────────────────────────────────────────────────┘
```

---

## 7. In Practice: Release Sequences in Lock-Free Queues

Release sequences are the most typical use case in lock-free queues. Using the core CAS chain of the Michael-Scott queue as an example:

```cpp
template <typename T>
class lock_free_queue {
    struct node {
        std::atomic<T*> data{nullptr};
        std::atomic<node*> next{nullptr};
    };

    std::atomic<node*> head_;
    std::atomic<node*> tail_;

public:
    void push(T value) {
        auto* new_node = new node;
        new_node->data.store(new T(std::move(value)),
                             std::memory_order_relaxed);

        while (true) {
            node* tail = tail_.load(std::memory_order_acquire);
            node* next = tail->next.load(std::memory_order_acquire);

            if (tail == tail_.load(std::memory_order_acquire)) {
                if (next == nullptr) {
                    // Try to link new node to tail
                    if (tail->next.compare_exchange_weak(
                            next, new_node,
                            std::memory_order_release,    // success: release
                            std::memory_order_relaxed)) {
                        // Success! Try to advance tail
                        tail_.compare_exchange_strong(
                            tail, new_node,
                            std::memory_order_release,
                            std::memory_order_relaxed);
                        break;
                    }
                } else {
                    // Help other threads advance tail
                    tail_.compare_exchange_strong(
                        tail, next,
                        std::memory_order_release,
                        std::memory_order_relaxed);
                }
            }
        }
    }

    bool try_pop(T& result) {
        while (true) {
            node* head = head_.load(std::memory_order_acquire);
            node* tail = tail_.load(std::memory_order_acquire);
            node* next = head->next.load(std::memory_order_acquire);

            if (head == head_.load(std::memory_order_acquire)) {
                if (head == tail) {
                    if (next == nullptr) return false;
                    tail_.compare_exchange_strong(
                        tail, next,
                        std::memory_order_release,
                        std::memory_order_relaxed);
                } else {
                    T* data = next->data.load(std::memory_order_acquire);
                    if (head_.compare_exchange_strong(
                            head, next,
                            std::memory_order_acq_rel,
                            std::memory_order_acquire)) {
                        // Release sequences come into play here:
                        // push's release CAS (tail->next)
                        // may form a release sequence with subsequent CAS operations
                        // (helping advance tail/head)
                        // Our acquire CAS reads a value from the sequence
                        // → establishes synchronizes-with with the original push
                        result = *data;
                        delete data;
                        delete head;
                        return true;
                    }
                }
            }
        }
    }
};
```

### 7.1 Role of Release Sequences in the Queue

```
Push operation's release write:
  · tail->next.compare_exchange(nullptr, new_node, release) — release head
  · This CAS is an RMW that writes the address of new_node

  Other threads may help advance tail through multiple CAS rounds:
  · tail_.compare_exchange(old, next, release) — RMW

  Pop operation's acquire read:
  · head_.compare_exchange(old, next, acq_rel) — reads a value written by some thread

  Release sequence guarantees:
  · Even if pop's CAS doesn't directly read push's release write
  · As long as it reads a value from some RMW in the sequence
  · It can establish synchronizes-with with the original release
  · → data's write is visible to pop
```

---

## 8. Boundary Conditions of Release Sequences

### 8.1 Plain Store Breaks the Sequence

```cpp
std::atomic<int> x{0};
x.store(1, std::memory_order_release);     // A (head)
x.fetch_add(1, std::memory_order_acq_rel); // RMW: 1→2 (in sequence)
x.store(5, std::memory_order_relaxed);     // ❌ Plain store, breaks!
x.fetch_add(1, std::memory_order_acq_rel); // RMW: 5→6 (no release head)

// If Thread B reads 6, it cannot synchronize with A
// Because the intermediate store(5) broke the release sequence
```

### 8.2 RMW's Ordering Doesn't Matter

```cpp
std::atomic<int> x{0};
x.store(1, std::memory_order_release);        // A (head)
x.fetch_add(1, std::memory_order_relaxed);     // RMW with relaxed works too!
x.fetch_add(1, std::memory_order_relaxed);     // Still in the release sequence

// Release sequence does not require RMW operations to have any specific ordering
// Only requires they are RMW operations and contiguous in the modification order
// This is a very important property: RMW's ordering does not affect sequence construction
```

### 8.3 CAS Failure Does Not Enter the Sequence

```cpp
std::atomic<int> x{0};
x.store(1, std::memory_order_release);         // A (head)

int expected = 2; // intentionally mismatched
x.compare_exchange_weak(expected, 3,
    std::memory_order_acq_rel,
    std::memory_order_relaxed);
// CAS fails → doesn't modify x → doesn't enter modification order → not in release sequence
```

---

## 9. Relationship with Modification Order

```
Release sequences depend entirely on the modification order:

  modification order of x:
  W₀    A     RMW₁   RMW₂   RMW₃     W_new
  ─────────────────────────────────────────────→
  init  rel   acq_r  rel    acq_r    release

  release sequence of A = {A, RMW₁, RMW₂, RMW₃}
  (ends at W_new because W_new is a new release store that doesn't read RMW₃'s value)

  Key insight:
  · Release sequences are subsets of the modification order
  · Modification order is a total order of a single atomic object
  · Release sequences leverage this total order to "propagate" release semantics
  · Each RMW doesn't need to be release — only requires contiguous sequence
```

---

## 10. Summary

```
Release sequence key points:
┌────────────────────────────────────────────────────────────────┐
│ 1. Release sequence = release head + contiguous RMW chain      │
│ 2. RMW's memory_order does not affect sequence construction    │
│ 3. Plain stores break the sequence                             │
│ 4. acquire read of any operation's value in the sequence       │
│    → synchronizes with the release head                        │
│ 5. This enables multiple threads to indirectly propagate       │
│    synchronization semantics through CAS loops                 │
│ 6. Core synchronization mechanism for lock-free queues,        │
│    stacks, and other data structures                           │
│ 7. C++20 (P0735) refined the interaction between fences       │
│    and release sequences                                       │
└────────────────────────────────────────────────────────────────┘
```
