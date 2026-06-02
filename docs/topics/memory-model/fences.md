---
title: "内存屏障（Fences）"
topic: topics
feature: memory-model-fences
standard: C++
status_checked_at: 2026-06-02
---

# 内存屏障（Fences）

> 独立的内存屏障（`atomic_thread_fence`）是 C++ 内存模型中同步原语的另一半——与原子操作的内嵌 ordering 不同，fence 在**没有特定原子操作**的场景下提供排序约束。理解 fence 与原子操作 ordering 的等价关系和差异，是写出正确无锁代码的前提。

---

## 1. 两种同步范式

```
范式 A：操作内嵌 ordering（operation-embedded ordering）
  x.store(1, std::memory_order_release);
  int v = x.load(std::memory_order_acquire);

范式 B：独立 fence + relaxed 操作
  x.store(1, std::memory_order_relaxed);
  std::atomic_thread_fence(std::memory_order_release);
  // ...
  int v = x.load(std::memory_order_relaxed);
  std::atomic_thread_fence(std::memory_order_acquire);

两种范式不完全等价——fence 版本的同步条件更严格（见第 4 节）
```

---

## 2. Fence 类型

### 2.1 Acquire Fence

```cpp
std::atomic_thread_fence(std::memory_order_acquire);

// 语义：fence 之前的所有 relaxed load 不能被重排到 fence 之后
// 硬件映射：
//   x86:  无指令（TSO 天然 load-load 保序），编译器 barrier 即可
//   ARM:  DMB ISHLD（数据内存屏障，加载方向）
//   POWER: lwsync
```

### 2.2 Release Fence

```cpp
std::atomic_thread_fence(std::memory_order_release);

// 语义：fence 之后的所有 relaxed store 不能被重排到 fence 之前
// 硬件映射：
//   x86:  无指令（TSO 天然 store-store 保序），编译器 barrier
//   ARM:  DMB ISH（全方向数据内存屏障）
//   POWER: lwsync
```

### 2.3 Acq_rel Fence

```cpp
std::atomic_thread_fence(std::memory_order_acq_rel);

// 语义：同时具有 acquire 和 release 效果
// 硬件映射：
//   x86:  无额外指令（TSO 兼容）
//   ARM:  DMB ISH
//   POWER: lwsync
```

### 2.4 Seq_cst Fence

```cpp
std::atomic_thread_fence(std::memory_order_seq_cst);

// 语义：最强——所有 seq_cst 操作和 fence 构成全局全序
// 硬件映射：
//   x86:  MFENCE（或带 LOCK 前缀的指令）
//   ARM:  DMB ISH（加上额外的全局顺序约束）
//   POWER: sync（比 lwsync 更强的屏障）
//
// ⚠ seq_cst fence 在所有架构上都有显著开销
```

---

## 3. x86-TSO vs ARM 弱序模型

### 3.1 x86-TSO（Total Store Order）

```
x86 硬件保证：
  ✅ load-load 保序
  ✅ load-store 保序
  ✅ store-store 保序
  ❌ store-load 不保序（唯一弱点，通过 store buffer 实现）

  含义：
  · acquire fence  → 无硬件指令（TSO 天然满足），只需编译器 barrier
  · release fence  → 无硬件指令，只需编译器 barrier
  · acq_rel fence  → 同上
  · seq_cst fence  → MFENCE（用于解决 store-load 重排）

  这就是为什么 x86 上大部分 fence 几乎零开销——
  硬件已经做了排序，fence 只需阻止编译器重排。
```

```
x86 store buffer 的 store-load 漏洞：

  CPU 核心                    Store Buffer        缓存/L3
  ┌──────────┐               ┌──────────┐       ┌──────────┐
  │ store x=1│ ──────────→   │ x: 1     │ ──→   │ x: 1     │
  │          │               │ (未提交)  │       │          │
  │ load y   │ ←──────────── │          │       │ y: 0     │
  │ 返回 0   │               └──────────┘       └──────────┘
  └──────────┘

  store 进入 store buffer 后尚未对其他核可见
  但本核的 load 可以从 store buffer 读取（store-to-load forwarding）
  对于 load y，直接从缓存读取旧值
  → 这就是 store-load 重排的硬件根源
```

### 3.2 ARM（AArch64 / ARMv8）

```
ARM 硬件保证：
  ❌ load-load 可以重排
  ❌ load-store 可以重排
  ❌ store-load 可以重排
  ❌ store-store 可以重排

  所有 fence 都需要实际的硬件指令：

  DMB ISHLD   → acquire fence（加载方向屏障）
  DMB ISH     → release fence（全方向屏障）
  DMB ISH     → acq_rel fence
  DMB ISH     → seq_cst fence（部分实现需要 DSB + ISB）

  含义：
  · ARM 上每个 fence 都有真实的硬件开销
  · 需要更谨慎地放置 fence
  · relaxed 操作的性能优势更明显（避免不必要的 fence）
```

### 3.3 POWER（IBM）

```
POWER 是最弱的主流架构之一：

  · 所有四种重排都可能发生
  · 需要 lwsync（轻量级同步屏障）或 sync（重量级同步）
  · 甚至 lwsync 也有数十周期的开销
  · sync 在某些实现中可能超过 100 周期

  POWER 上的编程启示：
  · 尽可能用 relaxed + 精确 fence 而非 blanket seq_cst
  · 但对正确性的要求更高，因为重排可能性更多
```

---

## 4. Fence 放置规则

### 4.1 Release Fence + Acquire Fence 配对

```cpp
// 正确的 release-acquire fence 配对
std::atomic<int> x{0};
int data = 0;

// 线程 A
data = 42;                                              // ① 普通写入
std::atomic_thread_fence(std::memory_order_release);    // ② release fence
x.store(1, std::memory_order_relaxed);                  // ③ relaxed store

// 线程 B
while (x.load(std::memory_order_relaxed) != 1) {}      // ④ relaxed load
std::atomic_thread_fence(std::memory_order_acquire);    // ⑤ acquire fence
int v = data;                                           // ⑥ 读取 data

// 同步条件：④ 读取到 ③ 写入的值（即 x == 1）
//   → ② release fence synchronizes-with ⑤ acquire fence
//   → ① happens-before ⑥
//   → v == 42 ✓
```

### 4.2 Fence 不保证具体操作的顺序

```cpp
// ✅ 正确用法：如果 B 的 while 退出（y == 1），则 v == 1
std::atomic<int> x{0}, y{0};

// 线程 A
x.store(1, std::memory_order_relaxed);
std::atomic_thread_fence(std::memory_order_release);
y.store(1, std::memory_order_relaxed);

// 线程 B
while (y.load(std::memory_order_relaxed) != 1) {}
std::atomic_thread_fence(std::memory_order_acquire);
int v = x.load(std::memory_order_relaxed); // v == 1

// ⚠ 但如果 B 的 load(y) 没有读取到 store(y)（仍然在循环），
//    那么 fence 不建立任何同步关系
//    fence 的同步是有条件的——必须通过原子操作的 rf 关系激活
```

### 4.3 单侧 Fence 无效

```cpp
// ❌ 错误：只有 release fence，没有 acquire fence
std::atomic<int> x{0};
int data = 0;

// 线程 A
data = 42;
std::atomic_thread_fence(std::memory_order_release);
x.store(1, std::memory_order_relaxed);

// 线程 B
while (x.load(std::memory_order_relaxed) != 1) {}
// 没有 acquire fence！
int v = data; // ❌ data 可能不是 42

// release fence 只约束了 A 侧的顺序
// B 侧没有 acquire 约束，编译器/CPU 可以将 load(data) 重排到 load(x) 之前
```

---

## 5. 独立 Fence vs 操作内嵌 Ordering

### 5.1 语义差异

```cpp
// 方案 1：内嵌 ordering
x.store(1, std::memory_order_release);
// 只约束本次 store 的 release 语义

// 方案 2：独立 fence
x.store(1, std::memory_order_relaxed);
std::atomic_thread_fence(std::memory_order_release);
// 约束此 fence 之前的所有 store（不限于 x）与之后的所有操作

// 关键区别：
// release fence 约束 fence 前面的所有写操作，不只是紧跟的那一个
// 而 store(release) 只约束本次 store 自身
```

```cpp
// 具体例子说明差异
int a = 0, b = 0;
std::atomic<int> flag{0};

// 方案 A：store(release) — 只保证 flag 之前的操作不重排到 flag 之后
a = 1;
b = 2;
flag.store(1, std::memory_order_release);
// ✓ a=1, b=2 不会重排到 flag.store 之后

// 方案 B：relaxed store + release fence
a = 1;
b = 2;
std::atomic_thread_fence(std::memory_order_release);
flag.store(1, std::memory_order_relaxed);
// ✓ 效果相同：fence 前的所有写入（a=1, b=2）不重排到 fence 之后
//   而 flag.store 在 fence 之后，所以 a=1, b=2 不会重排到 flag.store 之后
```

### 5.2 何时用独立 Fence

```
独立 fence 适用场景：

  1. 多个变量需要同步，但只有一个是原子变量
     int data1, data2, data3;
     std::atomic<int> flag{0};

     // 写入端
     data1 = ...; data2 = ...; data3 = ...;
     std::atomic_thread_fence(std::memory_order_release);
     flag.store(1, std::memory_order_relaxed);

  2. 多个原子变量需要统一的 fence 约束
     x.store(1, std::memory_order_relaxed);
     y.store(2, std::memory_order_relaxed);
     std::atomic_thread_fence(std::memory_order_release);
     // x 和 y 的 store 都受到同一个 fence 约束

  3. 已有 relaxed 操作，需要在不修改操作本身的情况下添加 ordering
     （代码库重构时常见）
```

### 5.3 何时用内嵌 Ordering

```
内嵌 ordering 适用场景：

  1. 单个原子变量的简单同步（最常见的模式）
     x.store(val, std::memory_order_release);
     auto v = x.load(std::memory_order_acquire);

  2. CAS 操作——fence 无法精确模拟 CAS 的 success/failure ordering
     x.compare_exchange_weak(expected, desired,
         std::memory_order_acq_rel,  // success
         std::memory_order_acquire); // failure

  3. 性能敏感——fence 可能比必要的排序更强
     （例如 ARM 上 release fence 用了 DMB ISH 全方向屏障，
       而 store(release) 只需要 DMB ISH 但可以被编译器进一步优化）
```

---

## 6. 常见 Fence 模式

### 6.1 Store-Load Fence（SB 消除）

```cpp
// Store-Buffering 消除：最经典的 seq_cst fence 用法
std::atomic<int> x{0}, y{0};

// 线程 A
x.store(1, std::memory_order_relaxed);
std::atomic_thread_fence(std::memory_order_seq_cst); // 关键！
int r1 = y.load(std::memory_order_relaxed);

// 线程 B
y.store(1, std::memory_order_relaxed);
std::atomic_thread_fence(std::memory_order_seq_cst); // 关键！
int r2 = x.load(std::memory_order_relaxed);

// 不可能 r1 == 0 && r2 == 0
// seq_cst fence 保证两个 fence 在全局全序中排定顺序

// ⚠ 仅用 release/acquire fence 无法保证消除 SB
//    需要 seq_cst fence 或 seq_cst 操作
```

### 6.2 IRIW（Independent Reads of Independent Writes）

```cpp
// 四线程 litmus test：IRIW
std::atomic<int> x{0}, y{0};

// 线程 A: x.store(1, relaxed)
// 线程 B: y.store(1, relaxed)
// 线程 C: r1=x.load(relaxed); r2=y.load(relaxed); // 看到 x=1, y=0
// 线程 D: r3=y.load(relaxed); r4=x.load(relaxed); // 看到 y=1, x=0

// seq_cst fence 可以消除这种不一致结果：

// 线程 C                          // 线程 D
r1 = x.load(relaxed);             r3 = y.load(relaxed);
std::atomic_thread_fence(         std::atomic_thread_fence(
    std::memory_order_seq_cst);       std::memory_order_seq_cst);
r2 = y.load(relaxed);             r4 = x.load(relaxed);

// seq_cst fence 保证 C 的 fence 和 D 的 fence 在全局全序中排定顺序
// → 不可能看到不一致的 store 顺序
```

### 6.3 多变量同步

```cpp
// 用单个 fence 同步多个数据
int a, b, c, d;
std::atomic<int> guard{0};

// 写入端
a = 1; b = 2; c = 3; d = 4;
// 只需要一个 release fence，而非四个 store(release)
std::atomic_thread_fence(std::memory_order_release);
guard.store(1, std::memory_order_relaxed);

// 读取端
while (guard.load(std::memory_order_relaxed) != 1) {}
std::atomic_thread_fence(std::memory_order_acquire);
// a, b, c, d 的值都保证可见
```

---

## 7. Fence 的编译器实现

### 7.1 编译器 Barrier

```cpp
// 在编译器层面，fence 的实现通常是 compiler barrier

// GCC/Clang 实现（简化）：
#if defined(__x86_64__)
  // acquire fence → 编译器 barrier（无硬件指令）
  #define ACQUIRE_FENCE() asm volatile("" ::: "memory")

  // release fence → 编译器 barrier
  #define RELEASE_FENCE() asm volatile("" ::: "memory")

  // seq_cst fence → MFENCE
  #define SEQ_CST_FENCE() asm volatile("mfence" ::: "memory")
#elif defined(__aarch64__)
  #define ACQUIRE_FENCE() asm volatile("dmb ishld" ::: "memory")
  #define RELEASE_FENCE() asm volatile("dmb ish" ::: "memory")
  #define SEQ_CST_FENCE() asm volatile("dmb ish" ::: "memory")
#endif

// "memory" clobber 告诉编译器：不要将任何内存操作跨此屏障重排
```

### 7.2 LLVM IR 中的 Fence

```llvm
; acquire fence
fence acquire

; release fence
fence release

; acq_rel fence
fence acq_rel

; seq_cst fence
fence seq_cst

; LLVM 后端将这些翻译为目标架构的具体指令
```

---

## 8. Fence 与 Release Sequence 的交互

```cpp
// C++20 中 P0735 规范了 fence 与 release sequence 的交互
std::atomic<int> x{0};
int data = 0;

// 线程 A：release fence + relaxed store
data = 42;
std::atomic_thread_fence(std::memory_order_release);
x.store(1, std::memory_order_relaxed);

// 线程 B：RMW（在释放序列中）
x.fetch_add(1, std::memory_order_relaxed);

// 线程 C：acquire fence（读取序列中的值）
int v = x.load(std::memory_order_relaxed); // 读取到 2
std::atomic_thread_fence(std::memory_order_acquire);

// C++20 保证：
// release fence 的效果可以通过释放序列传播
// → data == 42 对线程 C 可见
// C++17 在这个场景下语义不够明确，P0735 修复了它
```

---

## 9. 性能考量

```
各架构上 fence 的开销（典型值）：

  操作                          x86-64    AArch64    POWER
  ─────────────────────────────────────────────────────────
  store(release)                ~0 周期    ~10 周期    ~30 周期
  load(acquire)                 ~0 周期    ~10 周期    ~30 周期
  release fence                 ~0 周期    ~10 周期    ~30 周期
  acquire fence                 ~0 周期    ~10 周期    ~30 周期
  seq_cst fence                 ~30 周期   ~10 周期    ~100 周期
  seq_cst store                 ~30 周期   ~10 周期    ~100 周期
  ─────────────────────────────────────────────────────────

  关键观察：
  · x86 上 release/acquire fence 几乎免费（编译器 barrier 即可）
  · x86 上 seq_cst fence 昂贵（需要 MFENCE）
  · ARM 上所有 fence 都有成本（DMB 指令）
  · POWER 上所有 fence 都昂贵（lwsync/sync）
  · relaxed 操作在所有架构上都是零额外开销
```

---

## 10. 总结

```
Fence 使用速查：
┌───────────────────────────────────────────────────────────────┐
│ 场景                          推荐方案                        │
│ ──────────────────────────────────────────────────────────── │
│ 单变量 release-acquire        store(release) + load(acquire) │
│ 多变量 release-acquire        fence(release) + fence(acquire)│
│ 需要消除 SB (store-load)     seq_cst fence 或 seq_cst 操作  │
│ CAS 操作                     操作内嵌 ordering               │
│ 性能敏感的 ARM 代码           尽可能用 relaxed + 精确 fence  │
│ 不确定                       seq_cst（默认最安全）            │
└───────────────────────────────────────────────────────────────┘

核心原则：
· fence 必须与原子操作配对才能建立 synchronizes-with
· 单侧 fence 不产生同步效果
· seq_cst fence 是唯一能保证全局全序的 fence
· 在 x86 上 release/acquire fence 几乎免费，大胆使用
· 在 ARM/POWER 上每个 fence 都有成本，精打细算
```
