---
title: "Hazard Pointer"
topic: topics
feature: memory-model-hazard-pointer
standard: C++
status_checked_at: 2026-06-02
---

# Hazard Pointer

> Hazard Pointer is a lock-free memory reclamation scheme proposed by Maged Michael in 2004. It achieves safe deferred reclamation through thread cooperation, solving the ABA problem and use-after-free problem in lock-free data structures without requiring a garbage collector or double-width CAS.

---

## 1. Problem Background

The core contradiction in lock-free data structures:

```
Thread A is reading the contents of node X
Thread B wants to delete node X

  ┌──────────────────────────────────────────┐
  │ If B deletes X first → A has use-after-free │
  │ If A finishes reading first → then B can delete X │
  │                                          │
  │ But there is no lock between A and B! How to coordinate? │
  └──────────────────────────────────────────┘

Deficiencies of traditional approaches:
  · shared_ptr with reference counting → high atomic operation overhead,
    cannot safely acquire references in CAS loops
  · Garbage collector → C++ has no GC, and pauses are unacceptable
  · Tagged pointers → requires 128-bit CAS, not general-purpose
  · Hazard pointer → protects nodes by "announcing I am accessing it"
```

---

## 2. Algorithm Core

### 2.1 Basic Data Structures

```
Global shared state:

  ┌─────────────────────────────────────────────────────────┐
  │ hazard_pointers[]                                       │
  │ ┌──────┬──────┬──────┬──────┐                          │
  │ │ T₁:  │ T₂:  │ T₃:  │ T₄:  │  One slot per thread   │
  │ │ null │ null │ null │ null │                          │
  │ └──────┴──────┴──────┴──────┘                          │
  │                                                         │
  │ Thread 1 retire list: [node_A, node_B, ...]            │
  │ Thread 2 retire list: [node_C, ...]                    │
  │ Thread 3 retire list: []                                │
  └─────────────────────────────────────────────────────────┘
```

### 2.2 Operation Flow

```
Flow for reading a shared node:

  1. Store the node address into the thread's own hazard pointer slot
  2. Re-verify that the node is still valid (in the data structure)
  3. If valid, safely access the node
  4. Clear the hazard pointer after access is complete

Flow for deleting a node:

  1. "Logically delete" the node from the data structure (CAS removal)
  2. Place the node into the retire list
  3. When the retire list reaches a threshold, execute scan + reclaim
```

---

## 3. Detailed Implementation

### 3.1 Hazard Pointer Manager

```cpp
#include <atomic>
#include <vector>
#include <functional>
#include <algorithm>

class hazard_pointer {
    // Maximum number of pointers protected per thread
    static constexpr int HP_PER_THREAD = 2;
    // Retire list threshold to trigger scan
    static constexpr int RETIRE_THRESHOLD = 2 * HP_PER_THREAD;

    struct hazard_slot {
        std::atomic<void*> ptr{nullptr};
        std::atomic<bool>  active{false};
    };

    // Fixed-size hazard pointer table (simplified version)
    // Actual implementation may use thread_local registration + global linked list
    static constexpr int MAX_THREADS = 128;
    static inline hazard_slot slots_[MAX_THREADS];
    static inline std::atomic<int> slot_count_{0};

    struct retire_entry {
        void* ptr;
        std::function<void(void*)> deleter;
    };

    // thread_local retire list
    static inline thread_local std::vector<retire_entry> retire_list_;
    static inline thread_local int my_slot_ = -1;

public:
    // Register current thread, acquire a slot
    static int acquire_slot() {
        int idx = slot_count_.fetch_add(1, std::memory_order_relaxed);
        slots_[idx].active.store(true, std::memory_order_release);
        my_slot_ = idx;
        return idx;
    }

    // Get the k-th hazard pointer for the current thread (k ∈ [0, HP_PER_THREAD))
    class hp_guard {
        int slot_;
        int index_;
    public:
        hp_guard(int slot, int index) : slot_(slot), index_(index) {}

        // Protect a pointer: set the hazard pointer and return the pointer value
        template <typename T>
        T* protect(std::atomic<T*>& src) {
            T* p;
            do {
                p = src.load(std::memory_order_acquire);
                slots_[slot_ + index_].ptr.store(
                    p, std::memory_order_release);
                // Re-read to confirm p is still valid
            } while (p != src.load(std::memory_order_acquire));
            return p;
        }

        // Clear protection
        void clear() {
            slots_[slot_ + index_].ptr.store(
                nullptr, std::memory_order_release);
        }

        ~hp_guard() { clear(); }
    };

    // Get the current thread's hazard pointer guard
    static hp_guard get_guard(int index = 0) {
        if (my_slot_ == -1) acquire_slot();
        return {my_slot_, index};
    }

    // Deferred reclamation of a node
    template <typename T>
    static void retire(T* ptr) {
        retire_list_.push_back({ptr, [](void* p) {
            delete static_cast<T*>(p);
        }});

        if (retire_list_.size() >= RETIRE_THRESHOLD) {
            scan_and_reclaim();
        }
    }

    // Scan all hazard pointers, reclaim unprotected nodes
    static void scan_and_reclaim() {
        // Step 1: collect all active hazard pointers
        std::vector<void*> protected_ptrs;
        for (int i = 0; i < MAX_THREADS; ++i) {
            if (slots_[i].active.load(std::memory_order_acquire)) {
                void* p = slots_[i].ptr.load(std::memory_order_acquire);
                if (p) protected_ptrs.push_back(p);
            }
        }

        // Sort for binary search
        std::sort(protected_ptrs.begin(), protected_ptrs.end());

        // Step 2: traverse retire list, delete unprotected nodes
        auto it = std::remove_if(
            retire_list_.begin(), retire_list_.end(),
            [&](const retire_entry& entry) {
                bool is_protected = std::binary_search(
                    protected_ptrs.begin(),
                    protected_ptrs.end(),
                    entry.ptr);
                if (!is_protected) {
                    entry.deleter(entry.ptr);
                    return true;
                }
                return false;
            });
        retire_list_.erase(it, retire_list_.end());
    }
};
```

### 3.2 Protecting pop with Hazard Pointer

```cpp
template <typename T>
class hp_stack {
    struct node {
        T data;
        std::atomic<node*> next;
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
        auto hp = hazard_pointer::get_guard(0);

        while (true) {
            node* old_head = hp.protect(head_);

            if (!old_head) return std::nullopt;

            // hp.protect() guarantees old_head won't be reclaimed
            // But it may already have been removed from the linked list
            if (head_.load(std::memory_order_acquire) != old_head) {
                continue; // Already popped by another thread, retry
            }

            // Attempt CAS removal
            if (head_.compare_exchange_strong(
                    old_head, old_head->next.load(std::memory_order_acquire),
                    std::memory_order_acq_rel)) {
                T result = std::move(old_head->data);

                // Safe reclamation: clear hazard pointer first, then retire
                hp.clear();
                hazard_pointer::retire(old_head);

                return result;
            }
        }
    }
};
```

### 3.3 Why Double Verification Is Needed

```
Necessity of protect + re-verification:

  1. hp.protect(head_) → sets hazard pointer to point to old_head
     old_head may already have been deleted by another thread before hp was set
     → the do-while loop inside protect handles this case

  2. After protect returns, old_head's memory is safe (protected by hp)
     But it may have been logically removed from the linked list
     → Need to check head_ == old_head to confirm it's still in the list

  3. Timing:
     Thread A: old = protect(head)     → hp points to old
     Thread B: CAS(head, old, old->next) → old removed from list
     Thread B: retire(old)             → old enters retire list
     Thread B: scan → hp points to old → not reclaimed
     Thread A: head != old → retry
     Thread A: hp.clear()              → clear protection
     Next scan → old unprotected → safe to reclaim
```

---

## 4. Comparison with shared_ptr

```
┌──────────────────┬────────────────────┬────────────────────┐
│                  │ Hazard Pointer     │ shared_ptr         │
├──────────────────┼────────────────────┼────────────────────┤
│ Reference count  │ None               │ Atomic ref count   │
│ Read overhead    │ store(relaxed) +   │ atomic increment   │
│                  │ load(acquire)      │ + atomic decrement │
│ Write (CAS) cost │ Unaffected         │ Extra sync needed  │
│ Memory reclaim   │ Deferred (batch)   │ Immediate on zero  │
│ ABA safe         │ ✅                  │ Partially safe     │
│ Cyclic refs      │ Not an issue       │ Needs weak_ptr     │
│ Thread scaling   │ O(T) scan          │ No scaling issue   │
│ Peak memory      │ Higher (deferred)  │ Lower (immediate)  │
└──────────────────┴────────────────────┴────────────────────┘

Core difference:
  · shared_ptr cannot safely acquire references in CAS loops (use-after-free race)
  · Hazard pointer avoids this race by "declare before access"
  · Under high concurrency lock-free scenarios, hazard pointer overhead is much
    lower than atomic reference counting
```

---

## 5. Performance Characteristics

### 5.1 Overhead Analysis

```
Time overhead:
  · Protecting a pointer: 2× atomic store + 2× atomic load ≈ 10-40 ns
  · Retiring a node: push_back ≈ 5 ns (amortized)
  · scan: O(T × H + R log R)
    T = thread count, H = HP_PER_THREAD, R = retire list size
  · reclaim: O(R)

Space overhead:
  · Hazard pointer table: T × HP_PER_THREAD × sizeof(atomic<void*>)
    = 128 × 2 × 8 = 2 KB (typical configuration)
  · Retire list: at most RETIRE_THRESHOLD pointers per thread
    = 4 × (8 + sizeof(function)) ≈ 128 bytes per thread
  · Deferred memory release of pending nodes (peak ≈ T × RETIRE_THRESHOLD × node_size)
```

### 5.2 Scalability

```
Overhead as thread count grows:

  Threads     Scan overhead   Suitable scenario
  ─────────────────────────────────────
  4-8         Low             Single-socket server
  16-64       Medium          Multi-socket server
  128+        High            Needs sharding optimization

  Scan optimizations:
  · Cache scan results (reuse previous protected set if retire list unchanged)
  · Generational reclamation: scan "younger" retire lists first
  · Cooperative scanning: have idle threads help with scanning
```

---

## 6. C++26 std::hazard_pointer

C++26 plans to introduce standard hazard pointer support (P2530):

```cpp
// C++26 expected interface (proposal stage)
#include <hazard_pointer>

// Get the current thread's hazard pointer
auto hp = std::hazard_pointer::make();

// Protect an atomic pointer
node* p = hp.protect(head);

// Use p...
T val = p->data;

// Release protection
hp.release();

// Retire + deferred reclamation
std::hazard_pointer::retire(old_node, [](node* p) { delete p; });
```

---

## 7. Improvement: Domain-Based Hazard Pointer

```
Domain grouping optimization (P2530's design):

  Different data structures can use different hazard pointer domains
  → scan only scans hazard pointers in the relevant domain
  → reduces interference from unrelated threads

  ┌───────────────────────┐   ┌───────────────────────┐
  │ Domain A (Stack)      │   │ Domain B (Queue)      │
  │ ┌──────┬──────┐      │   │ ┌──────┬──────┐       │
  │ │ T₁:  │ T₂:  │      │   │ │ T₁:  │ T₃:  │       │
  │ └──────┴──────┘      │   │ └──────┴──────┘       │
  │ retire: [X, Y, Z]   │   │ retire: [P, Q]        │
  └───────────────────────┘   └───────────────────────┘

  Stack's scan only looks at Domain A
  Queue's scan only looks at Domain B
  → Smaller scan scope, better performance
```

---

## 8. Comparison with RCU

```
RCU (Read-Copy-Update) — the standard approach in the Linux kernel:

  ┌──────────────────┬────────────────────┬────────────────────┐
  │                  │ Hazard Pointer     │ RCU                │
  ├──────────────────┼────────────────────┼────────────────────┤
  │ Read overhead    │ store + load       │ Near zero (preempt │
  │                  │ (per critical sec) │ disable/enable)    │
  │ Write overhead   │ O(T) scan          │ Wait grace period  │
  │ Environment      │ User-space         │ Kernel-space       │
  │ Memory overhead  │ O(T × H + R)       │ O(R)               │
  │ Precision        │ Precise (per-ptr)  │ Coarse (per-epoch) │
  └──────────────────┴────────────────────┴────────────────────┘

  User-space RCU (e.g., liburcu):
  · Implemented using signals or memory barriers
  · Read overhead slightly higher than kernel RCU
  · But still lower than hazard pointer's per-access overhead
```

---

## 9. Practical Advice

```
When to use Hazard Pointer:
  · Lock-free data structures need safe memory reclamation
  · Cannot or do not want to use GC
  · Node lifecycle of the data structure is complex
  · Moderate thread count (< 128)

When not to use:
  · Environment with GC
  · Very rare reads and writes (simple epoch-based is sufficient)
  · Extremely high thread count (> 1000) — scan overhead too high, consider RCU
  · Locks are acceptable (mutex simplicity far exceeds lock-free approaches)

Implementation advice:
  · Start with mature libraries: e.g., libcds, folly::hazptr
  · Don't implement from scratch — correctness verification is extremely difficult
  · Validate with ThreadSanitizer + stress testing
```

---

## 10. Summary

```
Hazard Pointer key points:
┌────────────────────────────────────────────────────────────────┐
│ 1. "Declare before access": set HP → verify → use → clear     │
│ 2. Deleter scans all HPs, only reclaims unprotected nodes     │
│ 3. No reference counting, no GC, purely cooperative deferred reclaim│
│ 4. Solves ABA and use-after-free                               │
│ 5. Scan overhead O(T×H+R log R), suitable for moderate thread count│
│ 6. C++26 plans standardization (P2530)                        │
│ 7. Production environments recommend mature libraries (folly::hazptr, libcds)│
└────────────────────────────────────────────────────────────────┘
```
