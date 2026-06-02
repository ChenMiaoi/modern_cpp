---
title: "风险指针（Hazard Pointer）"
topic: topics
feature: memory-model-hazard-pointer
standard: C++
status_checked_at: 2026-06-02
---

# 风险指针（Hazard Pointer）

> Hazard Pointer 是 Maged Michael 在 2004 年提出的无锁内存回收方案。它通过线程协作实现安全的延迟回收，解决了无锁数据结构中的 ABA 问题和 use-after-free 问题，而不需要垃圾回收器或 double-width CAS。

---

## 1. 问题背景

无锁数据结构中的核心矛盾：

```
线程 A 正在读取节点 X 的内容
线程 B 想删除节点 X

  ┌──────────────────────────────────────────┐
  │ 如果 B 先 delete X → A 是 use-after-free │
  │ 如果 A 先完成读取 → B 才能 delete X     │
  │                                          │
  │ 但 A 和 B 之间没有锁！如何协调？         │
  └──────────────────────────────────────────┘

传统方案的缺陷：
  · 带引用计数的 shared_ptr → 原子操作开销大，CAS 循环中无法安全获取引用
  · 垃圾回收器 → C++ 没有 GC，且暂停不可接受
  · 标记指针 → 需要 128-bit CAS，不通用
  · hazard pointer → 通过"宣布我正在访问"来保护节点
```

---

## 2. 算法核心

### 2.1 基本数据结构

```
全局共享状态：

  ┌─────────────────────────────────────────────────────────┐
  │ hazard_pointers[]                                       │
  │ ┌──────┬──────┬──────┬──────┐                          │
  │ │ T₁:  │ T₂:  │ T₃:  │ T₄:  │  每个线程一个槽位      │
  │ │ null │ null │ null │ null │                          │
  │ └──────┴──────┴──────┴──────┘                          │
  │                                                         │
  │ 线程 1 的 retire list: [node_A, node_B, ...]          │
  │ 线程 2 的 retire list: [node_C, ...]                  │
  │ 线程 3 的 retire list: []                              │
  └─────────────────────────────────────────────────────────┘
```

### 2.2 操作流程

```
读取共享节点的流程：

  1. 将节点地址存入自己的 hazard pointer 槽位
  2. 重新验证节点是否仍然有效（在数据结构中）
  3. 如果有效，安全地访问节点
  4. 访问完毕后清除 hazard pointer

删除节点的流程：

  1. 从数据结构中"逻辑删除"节点（CAS 移除）
  2. 将节点放入 retire list
  3. 当 retire list 达到阈值时，执行 scan + reclaim
```

---

## 3. 详细实现

### 3.1 Hazard Pointer 管理器

```cpp
#include <atomic>
#include <vector>
#include <functional>
#include <algorithm>

class hazard_pointer {
    // 每个线程最多保护的指针数量
    static constexpr int HP_PER_THREAD = 2;
    // retire list 触发 scan 的阈值
    static constexpr int RETIRE_THRESHOLD = 2 * HP_PER_THREAD;

    struct hazard_slot {
        std::atomic<void*> ptr{nullptr};
        std::atomic<bool>  active{false};
    };

    // 固定大小的 hazard pointer 表（简化版）
    // 实际实现中可能用 thread_local 注册 + 全局链表
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
    // 注册当前线程，获取一个 slot
    static int acquire_slot() {
        int idx = slot_count_.fetch_add(1, std::memory_order_relaxed);
        slots_[idx].active.store(true, std::memory_order_release);
        my_slot_ = idx;
        return idx;
    }

    // 获取当前线程的第 k 个 hazard pointer（k ∈ [0, HP_PER_THREAD)）
    class hp_guard {
        int slot_;
        int index_;
    public:
        hp_guard(int slot, int index) : slot_(slot), index_(index) {}

        // 保护一个指针：设置 hazard pointer 并返回指针值
        template <typename T>
        T* protect(std::atomic<T*>& src) {
            T* p;
            do {
                p = src.load(std::memory_order_acquire);
                slots_[slot_ + index_].ptr.store(
                    p, std::memory_order_release);
                // 重新读取以确认 p 仍然有效
            } while (p != src.load(std::memory_order_acquire));
            return p;
        }

        // 清除保护
        void clear() {
            slots_[slot_ + index_].ptr.store(
                nullptr, std::memory_order_release);
        }

        ~hp_guard() { clear(); }
    };

    // 获取当前线程的 hazard pointer guard
    static hp_guard get_guard(int index = 0) {
        if (my_slot_ == -1) acquire_slot();
        return {my_slot_, index};
    }

    // 延迟回收节点
    template <typename T>
    static void retire(T* ptr) {
        retire_list_.push_back({ptr, [](void* p) {
            delete static_cast<T*>(p);
        }});

        if (retire_list_.size() >= RETIRE_THRESHOLD) {
            scan_and_reclaim();
        }
    }

    // 扫描所有 hazard pointer，回收不被保护的节点
    static void scan_and_reclaim() {
        // 第一步：收集所有活跃的 hazard pointer
        std::vector<void*> protected_ptrs;
        for (int i = 0; i < MAX_THREADS; ++i) {
            if (slots_[i].active.load(std::memory_order_acquire)) {
                void* p = slots_[i].ptr.load(std::memory_order_acquire);
                if (p) protected_ptrs.push_back(p);
            }
        }

        // 排序以便二分查找
        std::sort(protected_ptrs.begin(), protected_ptrs.end());

        // 第二步：遍历 retire list，删除不被保护的节点
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

### 3.2 使用 Hazard Pointer 保护 pop 操作

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

            // hp.protect() 保证 old_head 不会被回收
            // 但它可能已经被从链表中移除了
            if (head_.load(std::memory_order_acquire) != old_head) {
                continue; // 已被其他线程 pop，重试
            }

            // 尝试 CAS 移除
            if (head_.compare_exchange_strong(
                    old_head, old_head->next.load(std::memory_order_acquire),
                    std::memory_order_acq_rel)) {
                T result = std::move(old_head->data);

                // 安全回收：先清除 hazard pointer，再 retire
                hp.clear();
                hazard_pointer::retire(old_head);

                return result;
            }
        }
    }
};
```

### 3.3 为什么需要二次验证

```
protect + 重新验证 的必要性：

  1. hp.protect(head_) → 设置 hazard pointer 指向 old_head
     可能在设置 hp 之前，old_head 已经被另一个线程删除了
     → protect 内部的 do-while 循环处理了这种情况

  2. protect 返回后，old_head 的内存是安全的（受 hp 保护）
     但它可能已经从链表中逻辑移除了
     → 需要检查 head_ == old_head 来确认它仍在链表中

  3. 时序：
     线程 A: old = protect(head)     → hp 指向 old
     线程 B: CAS(head, old, old->next) → old 从链表移除
     线程 B: retire(old)             → old 进入 retire list
     线程 B: scan → hp 指向 old → 不回收
     线程 A: head != old → 重试
     线程 A: hp.clear()              → 清除保护
     下次 scan → old 无人保护 → 安全回收
```

---

## 4. 与 shared_ptr 的对比

```
┌──────────────────┬────────────────────┬────────────────────┐
│                  │ Hazard Pointer     │ shared_ptr         │
├──────────────────┼────────────────────┼────────────────────┤
│ 引用计数         │ 无                 │ 原子引用计数       │
│ 读取开销         │ store(relaxed) +   │ atomic increment   │
│                  │ load(acquire)      │ + atomic decrement │
│ 写入（CAS）开销  │ 不影响             │ 需要额外同步       │
│ 内存回收         │ 延迟（批量）       │ 引用归零时立即回收 │
│ ABA 安全         │ ✅                  │ 部分安全           │
│ 循环引用         │ 不存在             │ 需要 weak_ptr      │
│ 线程数扩展性     │ O(T) 扫描          │ 无扩展问题         │
│ 内存峰值         │ 较高（延迟回收）   │ 较低（立即回收）   │
└──────────────────┴────────────────────┴────────────────────┘

核心差异：
  · shared_ptr 在 CAS 循环中无法安全获取引用（use-after-free 竞争）
  · hazard pointer 通过"先声明再访问"避免了这个竞争
  · 在高并发无锁场景下，hazard pointer 的开销远低于原子引用计数
```

---

## 5. 性能特征

### 5.1 开销分析

```
时间开销：
  · 保护一个指针：2× atomic store + 2× atomic load ≈ 10-40 ns
  · retire 一个节点：push_back ≈ 5 ns（摊销）
  · scan：O(T × H + R log R)
    T = 线程数, H = HP_PER_THREAD, R = retire list 大小
  · reclaim：O(R)

空间开销：
  · hazard pointer 表：T × HP_PER_THREAD × sizeof(atomic<void*>)
    = 128 × 2 × 8 = 2 KB（典型配置）
  · retire list：每个线程最多 RETIRE_THRESHOLD 个指针
    = 4 × (8 + sizeof(function)) ≈ 128 bytes per thread
  · 待回收节点的内存延迟释放（峰值 ≈ T × RETIRE_THRESHOLD × node_size）
```

### 5.2 扩展性

```
随线程数增长的开销：

  线程数    scan 开销     适用场景
  ─────────────────────────────────────
  4-8       低            单 socket 服务器
  16-64     中等          多 socket 服务器
  128+      高            需要分片优化

  扫描优化：
  · 将 scan 的结果缓存（如果 retire list 没有变化，重用上次的保护集合）
  · 分代回收：先扫描"年轻"的 retire list
  · 协作式扫描：让空闲线程帮忙扫描
```

---

## 6. C++26 std::hazard_pointer

C++26 计划引入标准的 hazard pointer 支持（P2530）：

```cpp
// C++26 预期接口（提案阶段）
#include <hazard_pointer>

// 获取当前线程的 hazard pointer
auto hp = std::hazard_pointer::make();

// 保护一个原子指针
node* p = hp.protect(head);

// 使用 p...
T val = p->data;

// 释放保护
hp.release();

// 退休+延迟回收
std::hazard_pointer::retire(old_node, [](node* p) { delete p; });
```

---

## 7. 改进方案：Domain-Based Hazard Pointer

```
Domain 分组优化（P2530 的设计）：

  不同的数据结构可以使用不同的 hazard pointer domain
  → scan 只扫描相关 domain 的 hazard pointer
  → 减少无关线程的干扰

  ┌───────────────────────┐   ┌───────────────────────┐
  │ Domain A (Stack)      │   │ Domain B (Queue)      │
  │ ┌──────┬──────┐      │   │ ┌──────┬──────┐       │
  │ │ T₁:  │ T₂:  │      │   │ │ T₁:  │ T₃:  │       │
  │ └──────┴──────┘      │   │ └──────┴──────┘       │
  │ retire: [X, Y, Z]   │   │ retire: [P, Q]        │
  └───────────────────────┘   └───────────────────────┘

  Stack 的 scan 只看 Domain A
  Queue 的 scan 只看 Domain B
  → 扫描范围更小，性能更好
```

---

## 8. 与 RCU 的对比

```
RCU (Read-Copy-Update) — Linux 内核的标准方案：

  ┌──────────────────┬────────────────────┬────────────────────┐
  │                  │ Hazard Pointer     │ RCU                │
  ├──────────────────┼────────────────────┼────────────────────┤
  │ 读取开销         │ store + load       │ 近乎零（preempt    │
  │                  │ (每个临界区)       │ disable/enable）    │
  │ 写入开销         │ O(T) scan          │ 等待 grace period  │
  │ 适用环境         │ 用户态             │ 内核态             │
  │ 内存开销         │ O(T × H + R)       │ O(R)               │
  │ 精度             │ 精确（per-ptr）    │ 粗粒度（per-epoch）│
  └──────────────────┴────────────────────┴────────────────────┘

  用户态 RCU（如 liburcu）：
  · 使用信号或 memory barrier 实现
  · 读取开销略高于内核 RCU
  · 但仍比 hazard pointer 的 per-access 开销低
```

---

## 9. 实用建议

```
何时使用 Hazard Pointer：
  · 无锁数据结构需要安全的内存回收
  · 不想或不能使用 GC
  · 数据结构的节点生命周期复杂
  · 线程数适中（< 128）

何时不用：
  · 有 GC 的环境
  · 读极少、写极少的场景（简单的 epoch-based 即可）
  · 极高线程数（> 1000）——scan 开销过大，考虑 RCU
  · 可以接受锁（mutex 的简单性远超无锁方案）

实现建议：
  · 从成熟的库开始：如 libcds、folly::hazptr
  · 不要自己从头实现——正确性验证极其困难
  · 用 ThreadSanitizer + 压力测试验证
```

---

## 10. 总结

```
Hazard Pointer 核心要点：
┌────────────────────────────────────────────────────────────────┐
│ 1. "先声明再访问"：设置 HP → 验证 → 使用 → 清除              │
│ 2. 删除者扫描所有 HP，只回收未被保护的节点                    │
│ 3. 无引用计数，无 GC，纯协作式延迟回收                        │
│ 4. 解决 ABA 和 use-after-free                                  │
│ 5. 扫描开销 O(T×H+R log R)，适合中等线程数                    │
│ 6. C++26 计划标准化 (P2530)                                   │
│ 7. 生产环境建议使用成熟库 (folly::hazptr, libcds)             │
└────────────────────────────────────────────────────────────────┘
```
