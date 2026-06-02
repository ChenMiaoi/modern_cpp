---
title: "释放序列（Release Sequence）"
topic: topics
feature: memory-model-release-sequence
standard: C++
status_checked_at: 2026-06-02
---

# 释放序列（Release Sequence）

> 释放序列是 C++ 内存模型中最精妙的构造之一：它允许一个 release 操作通过一连串 read-modify-write 操作传播其同步语义，使得一个远处的 acquire 读取仍然能与最初的 release 建立 synchronizes-with 关系。

---

## 1. 定义

标准 [atomics.order] 定义：

> 以某个 release 操作 `A` 为**头**（head）的释放序列，是原子对象 `M` 的修改顺序中满足以下条件的最大连续子序列：
> 1. 第一个操作是 `A`（release 写入）
> 2. 后续每个操作都是对该原子对象 `M` 的 read-modify-write 操作
> 3. 每个 RMW 读取的值由修改顺序中在它前面的那个操作写入

```
原子对象 M 的修改顺序：

  ──────────────────────────────────────────────────────────→
  W₀(initial)  A(rel)  RMW₁  RMW₂  RMW₃  W_other(rel)  ...
  ──────────────────────────────────────────────────────────→
                ↑
            release head
                │
                └── release sequence = {A, RMW₁, RMW₂, RMW₃}
                    （到 W_other 为止，因为 W_other 是新的 release
                      写入，不读取前一个操作的值）
```

---

## 2. 为什么需要释放序列

考虑一个典型的无锁场景：多个线程通过 CAS 循环向同一个原子变量追加操作。

```cpp
std::atomic<int> shared_counter{0};

// 线程 A：发布初始值（release）
shared_counter.store(100, std::memory_order_release); // A (release head)

// 线程 B：CAS 递增（RMW）
int expected = shared_counter.load(std::memory_order_relaxed);
while (!shared_counter.compare_exchange_weak(
    expected, expected + 1,
    std::memory_order_acq_rel,   // success: RMW
    std::memory_order_relaxed    // failure: relaxed
)) {}

// 线程 C：CAS 递增（RMW）
expected = shared_counter.load(std::memory_order_relaxed);
while (!shared_counter.compare_exchange_weak(
    expected, expected + 1,
    std::memory_order_acq_rel,
    std::memory_order_relaxed
)) {}

// 线程 D：读取最终值（acquire）
int val = shared_counter.load(std::memory_order_acquire); // D (acquire)
```

**问题**：线程 D 的 acquire 读取读到了线程 C 的 RMW 写入的值。线程 D 能否与线程 A 的 release 建立 synchronizes-with 关系？

```
不用释放序列的话：
  A(release) → C的RMW → D(acquire)
  但 A 与 C 的 RMW 之间没有直接的 synchronizes-with
  A 与 D 之间的同步链断了

用释放序列：
  A 是 release head
  B 的 RMW 和 C 的 RMW 都读取前一个操作的值
  → {A, B的RMW, C的RMW} 构成一个 release sequence
  D 读取 C 的 RMW 的值 → D 的 acquire 与 A 的 release 同步
  → A happens-before D ✓
```

---

## 3. 释放序列的构成规则

### 3.1 必须是 RMW

释放序列中的每个操作（除头之外）**必须是 read-modify-write** 操作。单纯的 load 或 store 会中断序列。

```cpp
std::atomic<int> x{0};

x.store(1, std::memory_order_release);          // A (head)
int v = x.fetch_add(1, std::memory_order_acq_rel); // RMW, 在序列中
v = x.fetch_sub(1, std::memory_order_acq_rel);     // RMW, 在序列中
x.store(5, std::memory_order_relaxed);             // ❌ 普通 store，中断序列
int w = x.fetch_add(1, std::memory_order_acq_rel);  // 新序列从这里开始（但没有 release head）
```

### 3.2 同一原子对象

释放序列中的所有操作必须作用于**同一个**原子对象。

```cpp
std::atomic<int> x{0}, y{0};
x.store(1, std::memory_order_release);    // head（对 x）
// y 的 RMW 不在此序列中，因为作用于不同对象
```

### 3.3 修改顺序的连续性

序列必须是修改顺序中的**连续**子序列。如果中间插入了一个不属于序列的操作（如普通 store），序列就中断了。

### 3.4 Release Head 的类型

release head 可以是：
- `store` with `memory_order_release`
- `exchange` 或 `CAS` 的成功分支 with `memory_order_release` 或 `memory_order_acq_rel`
- `fetch_add` 等 RMW with `memory_order_release` 或 `memory_order_acq_rel`

---

## 4. Synchronizes-With 建立条件

```
规则 [atomics.order]:

  如果以下条件同时满足，则存在 synchronizes-with 关系：
  1. 一个释放序列的头是 release 操作 A
  2. 操作 B 是对该原子对象的 acquire 读取
  3. B 读取了 release sequence 中某个操作写入的值

  → A synchronizes-with B
```

```cpp
// 完整示例
std::atomic<int> flag{0};
int data = 0;

// 线程 1：release 写入
data = 42;
flag.store(1, std::memory_order_release); // A

// 线程 2：RMW（不改变同步效果）
flag.fetch_add(0, std::memory_order_acq_rel); // RMW₁: 1→1（no-op RMW）

// 线程 3：RMW
flag.fetch_add(0, std::memory_order_acq_rel); // RMW₂: 1→1

// 线程 4：acquire 读取
int v = flag.load(std::memory_order_acquire); // B
if (v >= 1) {
    // release sequence = {A, RMW₁, RMW₂}
    // B 读取 RMW₂ 写入的值 → A synchronizes-with B
    assert(data == 42); // 保证通过
}
```

---

## 5. Acquire Fence 与释放序列的交互

`std::atomic_thread_fence(memory_order_acquire)` 可以与释放序列配合使用：

```cpp
std::atomic<int> flag{0};
int data = 0;

// 线程 1
data = 100;
flag.store(1, std::memory_order_release); // A

// 线程 2
flag.fetch_add(1, std::memory_order_relaxed); // RMW

// 线程 3
int v = flag.load(std::memory_order_relaxed); // 读取到 2
std::atomic_thread_fence(std::memory_order_acquire); // acquire fence

// 此处：
// · flag 的 release sequence 包含 {A, 线程 2 的 RMW}
// · 线程 3 的 load 读取了 RMW 的值
// · acquire fence 的语义：fence 之前的所有 relaxed 读取
//   仿佛都是 acquire 读取
// · 因此 A synchronizes-with fence
// · data 的写入在 A 之前 happens-before fence 之后的代码
assert(data == 100); // 保证通过
```

---

## 6. C++20 变更（P0735）

P0735（"Interaction of `atomic_thread_fence` with release sequences"）对 C++17 的 acquire fence 与释放序列的交互进行了重大修改。

### 6.1 C++17 的问题

C++17 标准中，acquire fence 与释放序列的交互存在歧义。具体而言，当一个 acquire fence 后面跟着一个 relaxed load，且该 load 读取了 release sequence 中的值时，fence 是否与 release head 同步的规则不够明确，给实现带来了困难。

### 6.2 P0735 的解决方案

P0735 引入了 **release fence sequence** 的概念，将 fence 的同步语义统一到序列模型中：

```cpp
// C++20 行为
std::atomic<int> x{0};
int data = 0;

// 线程 1
data = 42;
std::atomic_thread_fence(std::memory_order_release); // release fence
x.store(1, std::memory_order_relaxed);               // relaxed store

// 线程 2：RMW
x.fetch_add(1, std::memory_order_relaxed);           // 1→2

// 线程 3
int v = x.load(std::memory_order_relaxed);           // 读取到 2
std::atomic_thread_fence(std::memory_order_acquire); // acquire fence

// C++20: release fence → relaxed store(x,1) → RMW → relaxed load(x,2) → acquire fence
// 形成 fence-based synchronizes-with 关系
// data == 42 保证可见
```

### 6.3 实现影响

```
┌──────────────────────────────────────────────────────────┐
│ P0735 对编译器和硬件的影响：                              │
│                                                          │
│ · release fence 在硬件上的效果（如 x86 MFENCE, ARM DMB） │
│   必须覆盖到后续 relaxed store                           │
│ · acquire fence 的效果必须覆盖到之前的 relaxed load       │
│ · 实现更简单，语义更清晰                                 │
│ · 但：某些弱序硬件上的 fence 仍很昂贵                    │
└──────────────────────────────────────────────────────────┘
```

---

## 7. 实战：无锁队列中的释放序列

释放序列在无锁队列中是最典型的应用场景。以 Michael-Scott 队列的核心 CAS 链为例：

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
                    // 尝试将新节点链接到尾部
                    if (tail->next.compare_exchange_weak(
                            next, new_node,
                            std::memory_order_release,    // success: release
                            std::memory_order_relaxed)) {
                        // 成功！尝试推进 tail
                        tail_.compare_exchange_strong(
                            tail, new_node,
                            std::memory_order_release,
                            std::memory_order_relaxed);
                        break;
                    }
                } else {
                    // 帮助其他线程推进 tail
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
                        // 释放序列在这里起作用：
                        // push 的 release CAS（tail->next）
                        // 可能与后续多个 CAS（帮助推进 tail/head）
                        // 构成释放序列
                        // 我们的 acquire CAS 读取序列中的值
                        // → 与最初的 push 建立 synchronizes-with
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

### 7.1 释放序列在队列中的作用

```
push 操作的 release 写入：
  · tail->next.compare_exchange(nullptr, new_node, release) — release head
  · 这个 CAS 是一个 RMW，写入了 new_node 的地址

  其他线程可能通过多轮 CAS 帮助推进 tail：
  · tail_.compare_exchange(old, next, release) — RMW

  pop 操作的 acquire 读取：
  · head_.compare_exchange(old, next, acq_rel) — 读取了某个线程写入的值

  释放序列保证：
  · 即使 pop 的 CAS 不是直接读取 push 的 release 写入
  · 只要它读取了序列中某个 RMW 的值
  · 就能与最初的 release 建立 synchronizes-with
  · → data 的写入对 pop 可见
```

---

## 8. 释放序列的边界条件

### 8.1 普通 Store 中断序列

```cpp
std::atomic<int> x{0};
x.store(1, std::memory_order_release);     // A (head)
x.fetch_add(1, std::memory_order_acq_rel); // RMW: 1→2（在序列中）
x.store(5, std::memory_order_relaxed);     // ❌ 普通 store，中断！
x.fetch_add(1, std::memory_order_acq_rel); // RMW: 5→6（没有 release head）

// 如果线程 B 读取到 6，它不能与 A 同步
// 因为中间的 store(5) 中断了 release sequence
```

### 8.2 RMW 的 Ordering 无关紧要

```cpp
std::atomic<int> x{0};
x.store(1, std::memory_order_release);        // A (head)
x.fetch_add(1, std::memory_order_relaxed);     // RMW 用 relaxed 也可以！
x.fetch_add(1, std::memory_order_relaxed);     // 仍然在释放序列中

// release sequence 不要求 RMW 具有任何特定 ordering
// 只要求它们是 RMW 操作，且在修改顺序中连续
// 这是一个非常重要的特性：RMW 的 ordering 不影响序列的构成
```

### 8.3 CAS 失败不进入序列

```cpp
std::atomic<int> x{0};
x.store(1, std::memory_order_release);         // A (head)

int expected = 2; // 故意不匹配
x.compare_exchange_weak(expected, 3,
    std::memory_order_acq_rel,
    std::memory_order_relaxed);
// CAS 失败 → 不修改 x → 不进入修改顺序 → 不在释放序列中
```

---

## 9. 与 Modification Order 的关系

```
释放序列完全依赖于修改顺序：

  modification order of x:
  W₀    A     RMW₁   RMW₂   RMW₃     W_new
  ─────────────────────────────────────────────→
  init  rel   acq_r  rel    acq_r    release

  release sequence of A = {A, RMW₁, RMW₂, RMW₃}
  （到 W_new 为止，因为 W_new 是新 release store，不读取 RMW₃ 的值）

  关键洞察：
  · 释放序列是修改顺序的子集
  · 修改顺序是单个原子对象的全序
  · 释放序列利用了这个全序来"传播" release 语义
  · 不需要每个 RMW 都是 release，只要序列连续即可
```

---

## 10. 总结

```
释放序列核心要点：
┌────────────────────────────────────────────────────────────────┐
│ 1. 释放序列 = release head + 连续 RMW 操作链                  │
│ 2. RMW 的 memory_order 不影响序列构成                         │
│ 3. 普通 store 中断序列                                        │
│ 4. acquire 读取序列中任何操作的值 → 与 release head 同步      │
│ 5. 这使得多个线程可以通过 CAS 循环间接传播同步语义            │
│ 6. 无锁队列、栈等数据结构的核心同步机制                       │
│ 7. C++20 (P0735) 完善了 fence 与释放序列的交互                │
└────────────────────────────────────────────────────────────────┘
```
