---
title: "无锁栈与 ABA 问题"
topic: topics
feature: memory-model-lock-free-stack-aba
standard: C++
status_checked_at: 2026-06-02
---

# 无锁栈与 ABA 问题

> Treiber 栈是无锁数据结构的经典入门案例。它的简洁掩盖了一个致命陷阱：ABA 问题。本文深入分析 ABA 的产生机制，并介绍三种主流解决方案——标记指针、Hazard Pointer、Epoch-Based Reclamation。

---

## 1. Treiber 栈

### 1.1 基本结构

```
Treiber 栈（1986 年，R. Kent Treiber）：

  push/pop 都通过 CAS 修改 head 指针

  ┌────────┐     ┌────────┐     ┌────────┐
  │ data: C│     │ data: B│     │ data: A│
  │ next: ─┼────→│ next: ─┼────→│ next: ∅│
  └────────┘     └────────┘     └────────┘
       ↑
      head

  push(node):  node->next = head; CAS(&head, node->next, node)
  pop():       old = head; CAS(&head, old, old->next)
```

### 1.2 完整实现

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
        // ⚠ 危险！直接 delete old_head 会导致 ABA 问题
        delete old_head;
        return result;
    }
};
```

---

## 2. ABA 问题详解

### 2.1 问题复现

```
初始状态：head → [A] → [B] → [C] → ∅

时间线：
  T₁ (线程 1: pop)                T₂ (线程 2)
  ─────────────────────────────   ──────────────────────
  old = head → A
  old->next → B
  // 准备 CAS，但被挂起
                                  pop(): CAS成功，head → B
                                  delete A
                                  push(D): new node at A的地址
                                  // 如果分配器复用了 A 的内存地址
                                  // 则 D 的地址 == A 的地址
                                  pop(): CAS成功，head → C
                                  pop(): CAS成功，head → ∅
                                  push(A'): new node，地址碰巧是之前 A 的地址
                                  // head → [A'] → ∅
  // 线程 1 醒来
  CAS(head, A, B)
  // head 当前值确实是"A的地址"（但已经是 A'了！）
  // CAS 成功！head → B
  // 但 B 的 next 指向 C，而 C 已经被 pop 掉了！
  // → use-after-free！
```

### 2.2 ABA 的根因

```
CAS 只比较**值**（指针地址），不比较**版本**或**代次**：

  CAS(&head, expected, desired)
       ↓        ↓         ↓
      地址    旧值       新值

  当 expected 恰好等于当前 head 的值时，CAS 成功
  但这个"相等"可能是：
  · 同一个对象（正常情况）
  · 不同对象，恰好分配在同一地址（ABA！）

  在使用 malloc/free 的用户态程序中，地址复用是常见的。
  在内核或嵌入式系统中，如果内存不回收，ABA 不会发生。
```

### 2.3 ABA 问题不只是栈的问题

```
ABA 影响所有使用 CAS + 指针的无锁结构：

  · Treiber 栈（pop 操作）
  · Michael-Scott 队列（dequeue 操作）
  · 无锁链表（delete 操作）
  · 无锁哈希表（bucket 操作）

  任何"读取指针 → 计算依赖值 → CAS"的模式都有 ABA 风险
```

---

## 3. 方案一：标记指针（Tagged Pointers）

### 3.1 核心思想

```
在指针旁边存储一个单调递增的版本号：
  · CAS 比较 (ptr, tag) 的组合
  · 即使 ptr 地址被复用，tag 也不同
  · 需要 double-width CAS（x86: cmpxchg16b）

  tagged_ptr = {ptr, tag}
  CAS(&head, {A, 5}, {B, 6})  // 比较地址+版本号
  → 即使 A 的地址被复用，tag 不是 5，CAS 失败
```

### 3.2 实现（使用 128-bit CAS）

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

    // 需要 16 字节对齐以使用 cmpxchg16b
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

### 3.3 64 位环境下的指针打包

```cpp
// 替代方案：将 tag 打包进 64 位指针
// 前提：用户态虚拟地址只用了 48 位（或更少）

// 方法 1：利用高位
// 用户态地址范围：0x0000_0000_0000 ~ 0x0000_7FFF_FFFF_FFFF（47 位）
// 高 16 位可用于 tag（但需注意符号扩展）

// 方法 2：利用对齐
// 如果节点至少 16 字节对齐，低 4 位为 0
// 可以用低 4 位存 tag——但只有 16 个值，容易溢出

// 方法 3：压缩指针 + 高位 tag（最实用）
struct packed_tagged_ptr {
    uint64_t value;

    static constexpr int TAG_BITS = 16;
    static constexpr uint64_t PTR_MASK = (1ULL << 48) - 1;
    static constexpr uint64_t TAG_MASK = ~PTR_MASK;

    void* ptr() const {
        // 符号扩展：48 位 → 64 位
        uint64_t p = value & PTR_MASK;
        if (p & (1ULL << 47)) p |= ~PTR_MASK; // 负地址扩展
        return reinterpret_cast<void*>(p);
    }

    uint64_t tag() const { return (value & TAG_MASK) >> 48; }

    static packed_tagged_ptr make(void* p, uint64_t t) {
        uint64_t v = (reinterpret_cast<uint64_t>(p) & PTR_MASK)
                   | (t << 48);
        return {v};
    }
};

// 用普通的 64-bit CAS 即可
std::atomic<uint64_t> head;

// CAS 时比较整个 64 位（地址 + tag）
```

### 3.4 标记指针的局限性

```
局限性：
  · 64 位环境只有 16 位 tag → 65536 次操作后 tag 回绕
    （但回绕到相同 tag 的概率极低，需要恰好在同一地址发生）
  · 128-bit CAS 在 x86 上慢 2-3 倍
  · ARM 上没有原生 128-bit CAS，需要 LDAXP/STLXP 循环
  · 指针打包增加了代码复杂度和移植性问题
  · tag 位数有限，极端高争用下仍有小概率 ABA

  适用场景：
  · 简单的栈/队列，低到中等争用
  · 嵌入式或内核环境，不需要通用内存分配器
  · 对性能要求极高，不能接受 hazard pointer 的开销
```

---

## 4. 方案二：Hazard Pointer

（详见 hazard-pointer.md，此处为概览）

```
Hazard Pointer 核心思想（Maged Michael, 2004）：

  1. 每个线程有一个（或多个）hazard pointer 槽位
  2. 线程在访问共享节点前，将自己的 hazard pointer 设为该节点地址
  3. 其他线程在释放节点前，扫描所有线程的 hazard pointer
  4. 只有没有被任何 hazard pointer 保护的节点才会被真正删除

  ┌───────────────────────────────────────────────────┐
  │ 线程 1: hp = node_A;  // "我在访问 A"            │
  │ 线程 2: 想删 A → 扫描 hp → 发现线程 1 保护 A    │
  │         → 将 A 放入 retire list，不删除            │
  │ 线程 1: hp = nullptr;  // "我不再访问 A"          │
  │ 线程 2: 下次 scan → A 不再被保护 → 安全删除      │
  └───────────────────────────────────────────────────┘

  优势：不需要 double-width CAS，不需要地址空间技巧
  劣势：retire list 管理、scan 开销、内存使用延迟回收
```

---

## 5. 方案三：Epoch-Based Reclamation

```
Epoch-Based Reclamation (EBR) 核心思想：

  1. 维护全局 epoch 计数器（通常 0, 1, 2 循环）
  2. 每个线程记录自己当前所在的 epoch
  3. 线程进入临界区时"宣布"当前 epoch
  4. 退出临界区时清除
  5. 回收时：只有当所有线程都离开了旧 epoch，才回收该 epoch 的对象

  ┌───────────────────────────────────────────────────┐
  │ global_epoch = 2                                  │
  │                                                   │
  │ 线程 1: local_epoch = 2 (活跃)                    │
  │ 线程 2: local_epoch = 2 (活跃)                    │
  │ 线程 3: local_epoch = 1 (滞后)                    │
  │                                                   │
  │ epoch 0 的 retire list: [A, B, C]                │
  │ → 不能回收（线程 3 还在 epoch 1）                │
  │                                                   │
  │ 线程 3 推进到 epoch 2                             │
  │ → 现在可以安全回收 epoch 0 的 [A, B, C]          │
  └───────────────────────────────────────────────────┘

  优势：实现简单，性能好（只增加一次 atomic load per 临界区入口）
  劣势：一个滞后线程会阻止所有回收（"臭名昭著的 laggard"问题）
```

---

## 6. 使用标记指针的完整无锁栈

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

### 6.1 验证 ABA 安全性

```
ABA 安全性验证：

  假设线程 1 在 pop() 中被挂起，期间发生了完整的 ABA 循环：
  · head 从 {A, 5} 变成 {B, 6} 变成 {A', 7}

  线程 1 醒来，尝试 CAS：
  · expected = {A, 5}
  · current  = {A', 7}
  · ptr 相同（A 的地址），但 tag 不同（5 ≠ 7）
  · CAS 失败 ✓

  线程 1 重新读取 head = {A', 7}，重新计算 desired = {B', 8}
  现在它看到了正确的 next 指针

  结论：64 位 tag 使得 ABA 在实践中不可能发生
  （需要恰好 2^64 次操作后 tag 回绕到相同值）
```

---

## 7. x86 DCAS 对 ABA 的预防

```
x86 cmpxchg16b 指令：

  ; 输入：RDX:RAX = expected, RCX:RBX = desired
  ; 操作：原子地比较 [mem] 与 RDX:RAX
  ;       相等则写入 RCX:RBX，ZF=1
  ;       不相等则读取 [mem] 到 RDX:RAX，ZF=0
  lock cmpxchg16b [mem]

  在 C++ 中使用：
  struct alignas(16) tagged_ptr { void* p; uint64_t t; };
  std::atomic<tagged_ptr> ap;

  tagged_ptr exp = ap.load();
  tagged_ptr des = {new_ptr, exp.t + 1};
  while (!ap.compare_exchange_weak(exp, des)) {
      des.p = computed_ptr;
      des.t = exp.t + 1;
  }

  注意事项：
  · 必须 16 字节对齐（否则 #GP 异常）
  · 比 cmpxchg8b 慢 2-3 倍
  · 在虚拟化环境中可能被 trap-and-emulate（更慢）
  · 不是所有 CPU 都支持（需要 CX16 CPUID 特性）
```

---

## 8. 方案对比

```
┌──────────────┬──────────────┬───────────────┬──────────────┐
│              │ 标记指针     │ Hazard Ptr    │ Epoch-Based  │
├──────────────┼──────────────┼───────────────┼──────────────┤
│ CAS 宽度     │ 128-bit      │ 64-bit        │ 64-bit       │
│ 内存开销     │ 无额外       │ O(N×T)        │ O(T)         │
│ 回收延迟     │ 立即         │ 延迟（scan）  │ 延迟（epoch）│
│ 实现复杂度   │ 中等         │ 较高          │ 较低         │
│ ABA 安全性   │ 物理级别     │ 逻辑级别      │ 逻辑级别     │
│ 适用场景     │ 简单结构     │ 通用          │ 读多写少     │
│ 可移植性     │ 差（需DCAS） │ 好            │ 好           │
│ 滞后线程影响 │ 无           │ 无            │ 阻塞回收     │
└──────────────┴──────────────┴───────────────┴──────────────┘
```

---

## 9. 不解决 ABA 会怎样

```cpp
// 真实的 ABA 崩溃场景（简化）：
// pop() 中 delete old_head 后，内存被分配器回收
// 新的节点恰好在同一地址
// CAS 成功但 next 指针已失效

// 后果：
// 1. use-after-free → 段错误（最好的情况）
// 2. 数据损坏 → 静默错误（最坏的情况）
// 3. 安全漏洞 → 可被利用
//
// 在生产环境中，ABA 导致的崩溃通常极难复现
// 因为它依赖于特定的线程调度、内存分配时机
// TSan (ThreadSanitizer) 可以帮助检测部分场景
// 但无法覆盖所有 ABA 模式
```

---

## 10. 总结

```
ABA 问题速查：
┌─────────────────────────────────────────────────────────────┐
│ 问题：CAS 比较值（地址），不区分不同代次的对象             │
│ 触发：读取 → 挂起 → 地址被复用 → CAS 成功但语义错误       │
│                                                              │
│ 方案选择：                                                   │
│ · 简单结构 + x86 专用 → 标记指针 (cmpxchg16b)              │
│ · 通用结构 + 可移植 → Hazard Pointer                       │
│ · 读多写少 + 简单实现 → Epoch-Based Reclamation            │
│ · 不需要回收 → 不考虑 ABA（如只 push 不 pop）              │
└─────────────────────────────────────────────────────────────┘
```
