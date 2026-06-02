---
title: "CAS 操作与内存序（Compare-And-Swap Ordering）"
topic: topics
feature: memory-model-cas-ordering
standard: C++
status_checked_at: 2026-06-02
---

# CAS 操作与内存序（Compare-And-Swap Ordering）

> compare_exchange 是无锁编程的核心原语。它允许线程以原子方式比较并条件性地更新共享变量，是实现无锁数据结构的基石。理解 CAS 的 success/failure ordering 组合、spurious failure、以及 CAS 循环模式，是正确编写无锁代码的前提。

---

## 1. compare_exchange_weak vs compare_exchange_strong

```cpp
// C++ 原子接口
bool compare_exchange_weak(T& expected, T desired,
                           std::memory_order success,
                           std::memory_order failure);

bool compare_exchange_strong(T& expected, T desired,
                             std::memory_order success,
                             std::memory_order failure);
```

### 1.1 weak vs strong 的区别

```
compare_exchange_weak：
  · 即使 *this == expected 也可能返回 false（spurious failure）
  · 在 LL/SC 架构（ARM、POWER、RISC-V）上实现更高效
  · 必须在循环中使用

compare_exchange_strong：
  · 只有 *this != expected 时才返回 false
  · 内部可能是 CAS 循环（在 LL/SC 上包一层 loop）
  · 不需要用户写循环，但内部可能重试

硬件映射：
  ┌──────────────────────────────────────────────┐
  │ x86:                                         │
  │   weak 和 strong 都编译为 CMPXCHG           │
  │   x86 的 CAS 指令不产生 spurious failure     │
  │   → weak 和 strong 性能相同                  │
  │                                              │
  │ ARM/POWER/RISC-V:                            │
  │   weak → LR/SC 指令对（ldxr/stxr）          │
  │   strong → LR/SC 循环                        │
  │   weak 更高效（避免循环开销）                │
  └──────────────────────────────────────────────┘
```

### 1.2 LL/SC 架构上的实现

```
ARM AArch64 上的 compare_exchange_weak：

  ldxr  w0, [x1]       // Load-Exclusive (LL)
  cmp   w0, w2          // 比较 *this 与 expected
  b.ne  fail
  stxr  w3, w4, [x1]   // Store-Exclusive (SC)
  // 如果 stxr 失败（其他核干扰了缓存行），返回 false
  // 这就是 spurious failure 的硬件来源

ARM AArch64 上的 compare_exchange_strong：

  loop:
    ldxr  w0, [x1]     // LL
    cmp   w0, w2
    b.ne  fail
    stxr  w3, w4, [x1] // SC
    cbnz  w3, loop      // SC 失败则重试
  fail:
    ...
```

---

## 2. Success/Failure Ordering 组合

CAS 有两个 ordering 参数：成功时和失败时。标准对 failure ordering 施加了约束。

### 2.1 合法的 ordering 组合

```
success ordering    failure ordering    合法性
──────────────────────────────────────────────────
relaxed             relaxed             ✅
acquire             acquire             ✅
release             relaxed             ✅
acq_rel             acquire             ✅
seq_cst             seq_cst             ✅
──────────────────────────────────────────────────
release             acquire             ❌ 不合法
acq_rel             relaxed             ❌ 不合法
acquire             relaxed             ❌ 不合法（failure 不能弱于 success）
seq_cst             acquire             ✅
──────────────────────────────────────────────────

约束规则：
  1. failure ordering 不能是 release 或 acq_rel
  2. failure ordering 不能比 success ordering 更强
  3. 若 success 是 release，则 failure 只能是 relaxed
  4. 若 success 是 acquire，则 failure 只能是 acquire 或 relaxed
```

### 2.2 为什么失败分支不能是 release/acq_rel

```cpp
// CAS 失败时只是读取了当前值，没有写入
// release 语义的含义是：此写入之前的读写不能重排到此写入之后
// 但失败分支没有写入！release 语义没有意义

x.compare_exchange_weak(expected, desired,
    std::memory_order_acq_rel,     // 成功：读写都同步
    std::memory_order_acquire);    // 失败：只需要读同步

// 想象 CAS 失败时的硬件行为：
// 只是 load 了当前值到 expected → 只需要约束 load 的顺序
// → acquire 就够了
```

### 2.3 典型组合

```cpp
// 模式 1：无锁计数器（relaxed/relaxed）
// 只需要原子性，不需要同步
int expected = counter.load(std::memory_order_relaxed);
while (!counter.compare_exchange_weak(
    expected, expected + 1,
    std::memory_order_relaxed,  // success
    std::memory_order_relaxed)) // failure
{}

// 模式 2：flag 操作（acq_rel/acquire）
// 成功时需要 read-write 同步，失败时只需要 read 同步
int expected = flag.load(std::memory_order_relaxed);
while (!flag.compare_exchange_weak(
    expected, expected | FLAG_BIT,
    std::memory_order_acq_rel,   // success: read+write 同步
    std::memory_order_acquire))  // failure: read 同步
{}

// 模式 3：全局顺序（seq_cst/seq_cst）
// 需要最强保证时
int expected = x.load(std::memory_order_seq_cst);
while (!x.compare_exchange_weak(
    expected, desired,
    std::memory_order_seq_cst,  // success
    std::memory_order_seq_cst)) // failure
{}
```

---

## 3. Spurious Failure（虚假失败）

### 3.1 产生原因

```
spurious failure 的来源：

  1. LL/SC 硬件层面（ARM、POWER、RISC-V）
     · LL (Load-Exclusive/Link) 设置了一个监视标记
     · 在 LL 和 SC 之间，如果以下情况发生，SC 失败：
       a. 另一个核写入了同一缓存行
       b. 本核发生了上下文切换（某些实现）
       c. 缓存行被 evict 并重新加载
       d. 中断处理程序访问了该缓存行
     · 这些情况下即使值没变，CAS 也返回 false

  2. 编译器实现层面
     · 某些编译器在 weak CAS 循环中插入 yield 提示

  3. 不影响正确性
     · expected 参数在虚假失败时被更新为当前值
     · 循环会用新值重试
```

### 3.2 weak 必须在循环中使用

```cpp
// ❌ 错误：weak 没有循环
if (x.compare_exchange_weak(expected, desired,
    std::memory_order_acq_rel,
    std::memory_order_acquire)) {
    // CAS 成功
} else {
    // CAS 失败——可能是 spurious failure！
    // 这里不应该做"失败处理"，因为可能只是虚假失败
}

// ✅ 正确：weak 在循环中
while (!x.compare_exchange_weak(expected, desired,
    std::memory_order_acq_rel,
    std::memory_order_acquire)) {
    // expected 已自动更新为当前值
    // 可以在这里做其他操作（如修改 desired）
}

// ✅ 或者用 strong
if (x.compare_exchange_strong(expected, desired,
    std::memory_order_acq_rel,
    std::memory_order_acquire)) {
    // 确定 CAS 成功
}
```

---

## 4. CAS 循环模式

### 4.1 基本 CAS 循环

```cpp
// 原子递增（fetch_add 的等价实现）
std::atomic<int> counter{0};

int old_val = counter.load(std::memory_order_relaxed);
int new_val;
do {
    new_val = old_val + 1;
} while (!counter.compare_exchange_weak(
    old_val, new_val,
    std::memory_order_relaxed,
    std::memory_order_relaxed));
// old_val 在每次失败时自动更新
```

### 4.2 带计算的 CAS 循环

```cpp
// 原子乘法
std::atomic<int> x{0};

int old_val = x.load(std::memory_order_relaxed);
int new_val;
do {
    new_val = old_val * 2;
    // 在此处可以加入溢出检查、边界检查等
    if (new_val > MAX_VALUE) break;
} while (!x.compare_exchange_weak(old_val, new_val,
    std::memory_order_acq_rel,
    std::memory_order_acquire));
```

### 4.3 带条件的 CAS 循环

```cpp
// 只在值满足条件时更新
std::atomic<int> balance{1000};

int current = balance.load(std::memory_order_acquire);
while (current >= 100) {  // 条件：余额 >= 100
    int new_balance = current - 100;
    if (balance.compare_exchange_weak(current, new_balance,
        std::memory_order_acq_rel,
        std::memory_order_acquire)) {
        // 扣款成功
        break;
    }
    // current 已更新为最新值，循环重新检查条件
}
```

### 4.4 CAS 循环的性能问题

```
高争用场景下 CAS 循环的问题：

  N 个线程同时 CAS：
  ┌───────────────────────────────────────────────┐
  │ 线程 1: load val=5, CAS(5,6) → 成功          │
  │ 线程 2: load val=5, CAS(5,6) → 失败          │
  │ 线程 2: load val=6, CAS(6,7) → 失败          │
  │ 线程 3: load val=6, CAS(6,7) → 成功          │
  │ 线程 2: load val=7, CAS(7,8) → 成功          │
  │ ...                                           │
  │ 线程 N: 可能需要 N 次重试                     │
  └───────────────────────────────────────────────┘

  问题：
  · 大量失败的 CAS 浪费总线带宽
  · O(N²) 级别的总线事务
  · 缓存行在核之间"乒乓"

  优化方案：
  1. fetch_add 替代 CAS 循环（如计数器场景）
  2. backoff（指数退避）
  3. 分片（sharding）
  4. 结合 relaxed + 本地批量处理
```

---

## 5. ABA 问题简介

### 5.1 问题描述

```
ABA 是 CAS 最经典的陷阱：

  时间 ──────────────────────────────────────────────→

  线程 1: 读取 A（head → node_A）
          // 被挂起...被唤醒
          CAS(head, A, new_node) → 成功
          // 但 head 已经不是原来的 node_A 了！
          // 它被删了又被重新分配（碰巧地址相同）

  线程 2: 读取 A（head → node_A）
          CAS(head, A, node_B) → 成功（head 现在是 B）
          删除 node_A
          分配新节点——碰巧复用了 node_A 的地址！
          CAS(head, B, node_A) → 成功（head 又变回 "A"）

  ┌──────────────────────────────────────────────┐
  │ 初始状态：head → [A] → [C] → ...           │
  │                                              │
  │ 线程 2：pop A                                │
  │   head → [B] → [C] → ...                   │
  │   delete A                                   │
  │                                              │
  │ 线程 2：push 新节点（复用了 A 的地址）       │
  │   head → [A'] → [B] → [C] → ...            │
  │                                              │
  │ 线程 1：CAS(head, A, new_node) → 成功！     │
  │   head → [new] → [B] → [C] → ...           │
  │   但线程 1 以为 A→C 的 next 仍然有效        │
  │   实际上 A'→B，丢失了 B 和 C！              │
  └──────────────────────────────────────────────┘

  核心问题：CAS 只比较值（地址），不比较"版本"或"代次"
```

### 5.2 ABA 问题的解决方案预览

```
解决方案一览（详见 lock-free-stack-aba.md）：

  1. 标记指针（Tagged Pointers）
     · 在指针高位存储版本号
     · CAS 比较"地址+版本号"
     · 需要 double-width CAS（x86: cmpxchg16b）

  2. Hazard Pointers（风险指针）
     · 保护正在访问的节点不被回收
     · 延迟回收 + 安全扫描
     · 详见 hazard-pointer.md

  3. Epoch-Based Reclamation（基于纪元的回收）
     · 全局 epoch 计数器
     · 线程进入/退出临界区时更新 epoch
     · 只回收"无人引用"的节点

  4. RCU（Read-Copy-Update）
     · Linux 内核的解决方案
     · 读者无开销，写者承担复制+同步成本
```

---

## 6. Double CAS（cmpxchg16b）

### 6.1 x86 上的 128-bit CAS

```cpp
// x86-64 的 cmpxchg16b 指令可以原子地比较并交换 16 字节数据
// 典型用途：标记指针（pointer + version tag）

// GCC/Clang 内建函数：
struct tagged_ptr {
    void* ptr;
    uint64_t tag;
};

// 需要对齐到 16 字节
alignas(16) std::atomic<tagged_ptr> head;

// 使用 cmpxchg16b：
bool cas(tagged_ptr& expected, tagged_ptr desired) {
    return __atomic_compare_exchange(
        &head, &expected, &desired,
        false,                       // not weak
        __ATOMIC_SEQ_CST,
        __ATOMIC_SEQ_CST
    );
}

// 编译器生成：
// lock cmpxchg16b [head]
// RAX:RCX 与 [head] 比较
// 相等则写入 RDX:RBX
// 不相等则读取 [head] 到 RAX:RCX
```

### 6.2 标记指针打包

```cpp
// 在 64 位系统上，用户态指针通常只用了 48 位
// 可以用剩余的 16 位存储标记

// 方案 1：利用虚拟地址空间未使用的位
// 用户态地址空间：0x0000_0000_0000_0000 ~ 0x0000_7FFF_FFFF_FFFF（47 位）
// 高 17 位可用于标记（但需注意指针符号扩展）

// 方案 2：用对齐保证低位为 0
// 如果对象 8 字节对齐，低 3 位为 0，可存储标记
// 但 3 位标记太小，容易溢出

// 方案 3：用 128 位结构体 + cmpxchg16b（最可靠）
struct alignas(16) tagged_ptr {
    void*      ptr;  // 64 位指针
    uint64_t   tag;  // 64 位版本号——永远不会溢出

    // 全宽 CAS：同时比较 ptr 和 tag
};
```

### 6.3 cmpxchg16b 的性能

```
cmpxchg16b 性能特征：

  · 比 cmpxchg8b（64-bit CAS）慢约 2-3 倍
  · 锁住整条缓存行（通常 64 字节）
  · 在高争用场景下退化严重
  · ARM 上没有原生 128-bit CAS（需要 LL/SC 或 LDAXP/STLXP）
  · Apple M 系列芯片的 LDAXP/STLXP 性能不错

  实际使用建议：
  · 如果标记指针的需求，优先考虑标记指针 + cmpxchg16b
  · 如果需要更灵活的版本号，用 hazard pointer 或 epoch-based
```

---

## 7. CAS 的硬件实现

### 7.1 x86 CMPXCHG

```
x86 CMPXCHG 指令流程：

  CMPXCHG [mem], src:
    if (RAX == [mem]):
        [mem] = src      // 比较成功，写入新值
        ZF = 1           // 设置零标志
    else:
        RAX = [mem]      // 比较失败，读取当前值到 RAX
        ZF = 0

  LOCK CMPXCHG:
    · LOCK 前缀锁住缓存行（或总线锁）
    · 整个操作是原子的
    · 缓存一致性协议保证其他核看到原子性

  缓存行状态转换（MESI 协议）：
    1. 发起核将缓存行设为 Exclusive/Modified
    2. 其他核的对应缓存行设为 Invalid
    3. 执行比较和条件写入
    4. 释放锁
```

### 7.2 ARM LL/SC

```
ARM Load-Exclusive / Store-Exclusive：

  ldxr x0, [x1]    // Load-Exclusive: 读取并设置 exclusive monitor
  // ... 计算新值 ...
  stxr w2, x3, [x1] // Store-Exclusive: 条件写入
  cbnz w2, retry     // 如果 SC 失败（w2 != 0），重试

  Exclusive Monitor 工作原理：
  ┌─────────────────────────────────────────────┐
  │ 核心维护一个 exclusive monitor（标记位）    │
  │                                             │
  │ ldxr 设置 monitor 标记该缓存行              │
  │ 以下情况清除 monitor：                      │
  │   · 其他核写入了同一缓存行                  │
  │   · 本核执行了 clrex 或另一个 ldxr          │
  │   · 上下文切换（取决于实现）                │
  │                                             │
  │ stxr 检查 monitor 是否仍然有效              │
  │   · 有效 → 写入成功，w2=0                   │
  │   · 无效 → 写入失败，w2=1（spurious）       │
  └─────────────────────────────────────────────┘
```

---

## 8. 实战：无锁栈（CAS 版本预览）

```cpp
// 简化版 Treiber 栈（CAS 核心逻辑）
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
        // CAS 循环：尝试将 head 从 old_head 改为 new_node
        while (!head_.compare_exchange_weak(
            new_node->next,    // expected: old head
            new_node,          // desired: new node
            std::memory_order_release,
            std::memory_order_relaxed))
        {}
        // new_node->next 在每次失败时自动更新为最新的 head
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
        // ⚠ 此处有 ABA 问题：delete old_head 可能导致地址复用
        // 解决方案见 lock-free-stack-aba.md
        return result;
    }
};
```

---

## 9. CAS 模式速查

```
┌─────────────────────────────────────────────────────────────────┐
│ 模式                     CAS 类型    ordering                   │
│─────────────────────────────────────────────────────────────────│
│ 简单计数器               weak        relaxed/relaxed            │
│ flag 设置/清除           weak        acq_rel/acquire            │
│ 指针交换                 weak        acq_rel/acquire            │
│ 链表节点插入             strong      release/relaxed            │
│ 链表节点删除（pop）      strong      acq_rel/acquire            │
│ 需要全局顺序             weak        seq_cst/seq_cst            │
│ Double CAS (16B)         strong      seq_cst/seq_cst            │
│─────────────────────────────────────────────────────────────────│
│ weak 用于循环内部        strong 用于循环内部或单次尝试          │
│ 如果不确定               用 seq_cst（默认最安全）               │
└─────────────────────────────────────────────────────────────────┘
```
