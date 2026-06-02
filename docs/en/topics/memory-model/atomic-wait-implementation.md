---
title: "atomic wait/notify Implementation"
topic: topics
feature: memory-model-atomic-wait-implementation
standard: C++
status_checked_at: 2026-06-02
---

# atomic wait/notify Implementation

> C++20 introduced `std::atomic::wait()` and `std::atomic::notify_one/all()`, providing an efficient blocking wait mechanism for atomic variables. This article analyzes the implementation principles of these interfaces on Linux (futex), Windows (WaitOnAddress), macOS (ulock), as well as user-space designs like parking_lot.

---

## 1. C++20 wait/notify Interface

### 1.1 Basic Usage

```cpp
#include <atomic>
#include <thread>

std::atomic<int> state{0};

// Waiting thread
void waiter() {
    // Block until state != 0
    state.wait(0); // expected value is 0; blocks if current value == 0

    // Or with a timeout
    // state.wait(0, std::memory_order_seq_cst, 100ms);

    // After being woken, safely continue execution
    int val = state.load();
    // ...
}

// Notifying thread
void notifier() {
    // Do some work...
    state.store(1);

    // Wake one waiting thread
    state.notify_one();

    // Or wake all waiting threads
    // state.notify_all();
}
```

### 1.2 Semantics

```
state.wait(old):
  1. Atomically reads the current value of state
  2. If current value == old, blocks
  3. Returns when woken (may be notify, spurious wakeup, or timeout)
  4. If woken and current value != old, returns
     (spin-check to avoid missed wakeups)

state.notify_one():
  Wakes at least one thread waiting on this atomic variable

state.notify_all():
  Wakes all threads waiting on this atomic variable

Note:
  · wait may experience spurious wakeups
  · Must check the condition in a loop
  · Semantics are similar to POSIX condvar
```

### 1.3 Recommended Usage Pattern

```cpp
// Recommended pattern: wait in a loop
std::atomic<bool> ready{false};

// Waiter side
while (!ready.load(std::memory_order_acquire)) {
    ready.wait(false, std::memory_order_relaxed);
    // Re-check condition after wait returns
}

// Notifier side
ready.store(true, std::memory_order_release);
ready.notify_one();
```

---

## 2. Linux: futex

### 2.1 futex System Call

```c
// futex = Fast Userspace muTEX
// Linux-specific system call for implementing user-space synchronization primitives

#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

// futex wait
long futex_wait(int* addr, int expected, const struct timespec* timeout) {
    return syscall(SYS_futex, addr, FUTEX_WAIT, expected, timeout, NULL, 0);
}

// futex wake
long futex_wake(int* addr, int count) {
    return syscall(SYS_futex, addr, FUTEX_WAKE, count, NULL, NULL, 0);
}
```

### 2.2 How futex Works

```
Core idea of futex: user-space fast path + kernel-space slow path

  ┌───────────────────────────────────────────────────────┐
  │ User Space (Fast Path)                                │
  │   1. Atomically check *addr == expected               │
  │   2. If equal, enter kernel-space wait                │
  │   3. If not equal, return directly (no syscall needed)│
  │                                                       │
  │ Kernel Space (Slow Path)                              │
  │   1. Add current thread to the wait queue             │
  │   2. Yield CPU (thread state becomes TASK_INTERRUPTIBLE) │
  │   3. Wait to be woken                                 │
  │                                                       │
  │ futex_wake:                                           │
  │   1. Wake threads on the wait queue                   │
  │   2. Woken threads re-check the condition in user space│
  └───────────────────────────────────────────────────────┘
```

### 2.3 Implementing atomic::wait with futex

```cpp
// Simplified implementation (int type only)
#include <atomic>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace atomic_wait_impl {

void futex_wait(std::atomic<int>* addr, int expected) {
    // The kernel atomically checks *addr == expected
    // If not equal, futex returns EAGAIN directly
    syscall(SYS_futex,
            reinterpret_cast<int*>(addr),
            FUTEX_WAIT | FUTEX_PRIVATE_FLAG,
            expected,
            nullptr,  // no timeout
            nullptr,
            0);
}

void futex_wake_one(std::atomic<int>* addr) {
    syscall(SYS_futex,
            reinterpret_cast<int*>(addr),
            FUTEX_WAKE | FUTEX_PRIVATE_FLAG,
            1,  // wake one thread
            nullptr,
            nullptr,
            0);
}

void futex_wake_all(std::atomic<int>* addr) {
    syscall(SYS_futex,
            reinterpret_cast<int*>(addr),
            FUTEX_WAKE | FUTEX_PRIVATE_FLAG,
            INT_MAX,  // wake all threads
            nullptr,
            nullptr,
            0);
}

} // namespace atomic_wait_impl
```

### 2.4 FUTEX_WAIT_BITSET and FUTEX_WAKE_BITSET

```cpp
// Bitset operations allow more precise wakeups
// Can wake only threads waiting for a specific bit pattern

// Wait for a specific bit pattern
syscall(SYS_futex, addr,
        FUTEX_WAIT_BITSET | FUTEX_PRIVATE_FLAG,
        expected, timeout, nullptr,
        bitmask);  // 32-bit bitmask

// Wake threads with a specific bit pattern
syscall(SYS_futex, addr,
        FUTEX_WAKE_BITSET | FUTEX_PRIVATE_FLAG,
        INT_MAX, nullptr, nullptr,
        bitmask);

// Uses:
// · Implementing multiple wait types (read lock wait vs write lock wait)
// · Priority-based wakeups
// · Distinguishing signal and broadcast in condvar implementations
```

---

## 3. Windows: WaitOnAddress

### 3.1 API Overview

```cpp
#include <windows.h>

// WaitOnAddress introduced in Windows 8+
// Functionally equivalent to futex

BOOL WaitOnAddress(
    volatile VOID* Address,        // Address to wait on
    PVOID        CompareAddress,   // Value to compare against
    SIZE_T       AddressSize,      // Address size (1, 2, 4, or 8 bytes)
    DWORD        dwMilliseconds    // Timeout (INFINITE = wait forever)
);

VOID WakeByAddressSingle(PVOID Address);  // Wake one thread
VOID WakeByAddressAll(PVOID Address);     // Wake all threads
```

### 3.2 Implementation Principles

```
WaitOnAddress internal implementation (Windows kernel):

  ┌───────────────────────────────────────────────────────┐
  │ 1. User-space fast check:                             │
  │    if (memcmp(Address, CompareAddress, Size) != 0)    │
  │        return TRUE;  // Value already changed, no wait│
  │                                                       │
  │ 2. Enter kernel space (ntdll → ntoskrnl)              │
  │    · Add thread to a hash-based wait bucket           │
  │    · Wait buckets are grouped by Address hash         │
  │    · Thread enters waiting state                      │
  │                                                       │
  │ 3. WakeByAddressSingle/All:                           │
  │    · Compute hash of Address                          │
  │    · Wake threads in the wait bucket                  │
  └───────────────────────────────────────────────────────┘

  Differences from futex:
  · Supports arbitrary sizes of 1/2/4/8 bytes (futex only supports int)
  · No FUTEX_PRIVATE_FLAG concept (Windows process model differs)
  · Timeout parameter is in milliseconds (futex uses timespec)
  · Internally uses different hashing strategies
```

### 3.3 Implementing atomic::wait with WaitOnAddress

```cpp
#include <windows.h>

namespace atomic_wait_impl {

void wait(std::atomic<int>* addr, int expected) {
    // WaitOnAddress atomically compares *addr with expected
    WaitOnAddress(
        static_cast<volatile VOID*>(addr),
        &expected,
        sizeof(int),
        INFINITE);
}

void notify_one(std::atomic<int>* addr) {
    WakeByAddressSingle(addr);
}

void notify_all(std::atomic<int>* addr) {
    WakeByAddressAll(addr);
}

} // namespace atomic_wait_impl
```

---

## 4. macOS: ulock

### 4.1 ulock API

```cpp
// macOS uses the ulock family of system calls
// These are Apple-private, undocumented system calls

#include <sys/ulock.h>

// Wait operation
int __ulock_wait(uint32_t operation, void* addr, uint64_t value,
                 uint32_t timeout);

// Wake operation
int __ulock_wake(uint32_t operation, void* addr, uint64_t value);

// Operation types:
// UL_COMPARE_AND_WAIT      — compare and wait (similar to futex FUTEX_WAIT)
// UL_COMPARE_AND_WAIT_SHARED — shared memory version
// UL_UNFAIR_LOCK           — unfair lock wait
// UL_UNFAIR_LOCK_WAIT      — unfair lock wait (with priority inheritance)
// UL_COMPARE_AND_WAIT64    — 64-bit compare and wait
```

### 4.2 Implementation Principles

```
ulock implementation is similar to futex:

  · User-space fast path: atomic compare + conditional block
  · Kernel-space slow path: thread wait queue
  · Supports priority inheritance
  · Better integration with the Mach thread scheduler

  Other synchronization mechanisms on macOS:
  · pthread_mutex — may internally use ulock
  · dispatch_semaphore — GCD-level semaphore
  · os_unfair_lock — Apple-recommended low-level lock (replaces OSSpinLock)
```

---

## 5. parking_lot: User-Space Implementation

### 5.1 Design Philosophy

```
parking_lot (from the Rust ecosystem, also ported to C++) core philosophy:

  1. A global hash table maps addresses to wait queues
  2. All synchronization operations complete in user space (except thread suspend/wake)
  3. No need to allocate separate kernel resources per synchronization object
  4. Smaller and faster than pthread_mutex/condvar

  ┌───────────────────────────────────────────────────────┐
  │ Global hash table (typically 256-1024 buckets)        │
  │ ┌────────┬────────┬────────┬────────┬───────┐        │
  │ │ Bucket │ Bucket │ Bucket │ Bucket │  ...  │        │
  │ │   0    │   1    │   2    │   3    │       │        │
  │ └────────┴────────┴────────┴────────┴───────┘        │
  │       ↓                                               │
  │ Each bucket contains a mutex + condition variable +   │
  │ waiter linked list                                    │
  │                                                       │
  │ addr % NUM_BUCKETS → find the corresponding bucket    │
  │ The bucket uses a linked list to manage all threads   │
  │ waiting on that address                               │
  └───────────────────────────────────────────────────────┘
```

### 5.2 Wait Flow

```
park(addr, expected):
  1. hash = hash_addr(addr)
  2. lock bucket[hash].mutex
  3. Check *addr == expected (while holding the lock)
     · If not equal, unlock and return
  4. Add current thread to bucket[hash].waiters linked list
  5. Unlock bucket[hash].mutex
  6. Call platform primitive to suspend thread (Linux: futex, Windows: WaitOnAddress)
     or use a condition variable
  7. After being woken, remove self from the linked list

unpark_one(addr):
  1. hash = hash_addr(addr)
  2. lock bucket[hash].mutex
  3. Take one thread from bucket[hash].waiters
  4. Unlock bucket[hash].mutex
  5. Wake that thread
```

### 5.3 Performance Advantages

```
parking_lot vs native synchronization:

  ┌──────────────────┬──────────────┬──────────────┐
  │                  │ parking_lot  │ pthread      │
  ├──────────────────┼──────────────┼──────────────┤
  │ mutex size       │ 1 byte       │ 40-56 bytes  │
  │ condvar size     │ 0 (none)     │ 48 bytes     │
  │ Lock contention  │ Shorter spin │ Std backoff   │
  │ Spurious wakeup  │ Controllable │ Possibly more│
  │ Memory alloc     │ None         │ Possibly     │
  │ Cross-platform   │ High         │ Platform-dep │
  └──────────────────┴──────────────┴──────────────┘

  Key advantages:
  · mutex only needs 1 byte (2 bits to represent state)
  · No pre-allocated kernel resources per mutex/condvar
  · Wait queues shared via hash table, memory efficient
```

### 5.4 Relationship with futex

```
parking_lot and futex are not replacements for each other:

  · futex is a kernel primitive providing "user-space fast path + kernel-space suspend"
  · parking_lot is a user-space library using futex/WaitOnAddress as the underlying suspend mechanism

  Layering:
  ┌──────────────────────────────────────────┐
  │ parking_lot (user-space sync library)    │
  ├──────────────────────────────────────────┤
  │ futex / WaitOnAddress / ulock (OS primitives) │
  ├──────────────────────────────────────────┤
  │ Kernel thread scheduler                  │
  └──────────────────────────────────────────┘

  parking_lot can suspend threads via:
  · Linux:   futex(FUTEX_WAIT)
  · Windows: WaitOnAddress
  · macOS:   __ulock_wait or pthread_cond
```

---

## 6. Implementation Comparison

```
┌──────────────┬──────────────┬──────────────┬──────────────┬──────────────┐
│              │ futex        │ WaitOnAddress│ ulock        │ parking_lot  │
├──────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ Platform     │ Linux        │ Windows 8+   │ macOS        │ Cross-platform│
│ Granularity  │ int (4B)     │ 1/2/4/8B     │ 32/64-bit    │ Arbitrary    │
│ Hash table   │ Kernel-maint │ Kernel-maint │ Kernel-maint │ User-space   │
│ Timeout      │ ✅ (timespec) │ ✅ (ms)       │ ✅ (μs)       │ ✅            │
│ Priority Inh │ Partial      │ Limited      │ ✅            │ ❌            │
│ Spurious WU  │ Possible     │ Possible     │ Possible     │ Controllable │
│ Overhead     │ Syscall      │ Syscall      │ Syscall      │ User-space   │
│ Size         │ int*         │ void*        │ void*        │ 1-8 bytes    │
└──────────────┴──────────────┴──────────────┴──────────────┴──────────────┘
```

---

## 7. Standard Implementation Structure of std::atomic::wait

```
Common structure for implementing atomic::wait in the C++ standard library:

  template <typename T>
  void atomic<T>::wait(T old, memory_order mo) {
      // 1. Fast path: check if the value has already changed
      if (this->load(mo) != old) return;

      // 2. Brief spin (avoid unnecessary syscalls)
      for (int i = 0; i < SPIN_COUNT; ++i) {
          if (this->load(mo) != old) return;
          cpu_yield();  // x86: pause, ARM: yield
      }

      // 3. Enter blocking wait
      while (this->load(mo) == old) {
          platform_wait(this, old);  // futex / WaitOnAddress / ulock
      }
  }

  template <typename T>
  void atomic<T>::notify_one() {
      platform_wake_one(this);  // futex_wake / WakeByAddressSingle / ulock_wake
  }

  Spin count selection:
  · Linux libstdc++: SPIN_COUNT = 16
  · Linux libc++: SPIN_COUNT ≈ 20
  · MSVC: SPIN_COUNT depends on processor count
  · Goal: balance response latency and syscall overhead
```

---

## 8. Handling Spurious Wakeups

```cpp
// The C++ standard explicitly allows spurious wakeups
// Reason: implementations may have races when switching between kernel and user space

// Correct pattern: wait in a loop
std::atomic<int> state{0};

// Waiter side
int current = state.load(std::memory_order_acquire);
while (current == 0) {
    state.wait(0, std::memory_order_relaxed);
    current = state.load(std::memory_order_acquire);
}

// Incorrect pattern: assuming the condition is satisfied when wait returns
state.wait(0);  // may be a spurious wakeup
// Cannot assume state != 0, must re-check

// Sources of spurious wakeups:
// 1. futex: another thread woke the same futex, but our condition is not met
// 2. WaitOnAddress: timeout, signal interruption, hash collision
// 3. ulock: kernel scheduler preemption
// 4. parking_lot: other threads in the same hash bucket were woken
```

---

## 9. Differences Between atomic wait and condition_variable

```cpp
// condition_variable requires a mutex
std::mutex mtx;
std::condition_variable cv;
bool ready = false;

// Waiter side
{
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [] { return ready; });
    // mutex is re-acquired
}

// Notifier side
{
    std::lock_guard<std::mutex> lock(mtx);
    ready = true;
    cv.notify_one();
}

// atomic::wait is lighter weight:
// · No mutex needed (avoids mutex overhead)
// · No predicate lambda needed
// · Waits directly on the atomic value
// · Suitable for simple "flag" synchronization patterns
// · Not suitable for protecting complex state (use condvar instead)

// Selection guide:
// · Simple flag/state → atomic::wait
// · State requiring mutex protection → condition_variable
// · One-shot event → atomic::wait
// · Complex conditions (multiple variables) → condition_variable
```

---

## 10. Performance Characteristics

```
Typical overhead of each implementation:

  Operation                     futex        WaitOnAddress  parking_lot
  ──────────────────────────────────────────────────────────────────
  Uncontended (fast path)       ~5 ns        ~5 ns          ~2 ns
  Spin then block               ~500 ns      ~500 ns        ~300 ns
  Block + wake                  ~2-5 μs      ~2-5 μs        ~1-3 μs
  Heavy contention              ~10-50 μs    ~10-50 μs      ~5-20 μs
  ──────────────────────────────────────────────────────────────────

  Key observations:
  · parking_lot is fastest when uncontended (pure user-space, no syscall)
  · Little difference under contention (bottleneck is thread suspend/wake)
  · Spin count significantly affects latency-throughput tradeoff
  · Timed waits are slightly slower than infinite waits (timer setup needed)
```

---

## 11. Summary

```
atomic wait/notify implementation quick reference:
┌────────────────────────────────────────────────────────────────┐
│ Interface: wait(expected), notify_one(), notify_all()          │
│ Semantics: similar to condvar but no mutex, waits on atomic value│
│ Spurious wakeup: allowed, must check condition in a loop       │
│                                                                │
│ Platform implementations:                                      │
│ · Linux:   futex (FUTEX_WAIT / FUTEX_WAKE)                    │
│ · Windows: WaitOnAddress / WakeByAddress{Single,All}          │
│ · macOS:   __ulock_wait / __ulock_wake                        │
│ · Cross-platform: parking_lot (user-space hash table + OS suspend)│
│                                                                │
│ Selection advice:                                              │
│ · Simple flag sync → atomic::wait (lightest)                  │
│ · Mutex-protected complex state → condition_variable          │
│ · Extreme performance → parking_lot (smallest sync object, fastest fast path)│
│ · Priority inheritance needed → futex PI or ulock             │
└────────────────────────────────────────────────────────────────┘
```
