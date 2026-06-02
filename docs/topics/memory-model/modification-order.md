---
title: "修改顺序（Modification Order）"
topic: topics
feature: memory-model-modification-order
standard: C++
status_checked_at: 2026-06-02
---

# 修改顺序（Modification Order）

> 每个原子对象都存在一个**修改顺序**（modification order）——对该对象所有写操作的全序排列。这是 C++ 内存模型中最基础却最容易被忽视的概念，它约束了不同线程能观察到的写入顺序。

---

## 1. 基本定义

标准 [intro.races] 规定：对于每一个标量对象 `M`，对该对象的所有修改操作构成一个**全序**（total order）。这个全序称为 `M` 的修改顺序。

```
原子变量 x 的修改顺序示例：

  时间轴（全局全序）：
  ──────────────────────────────────────────→
  store(x, 1)   store(x, 2)   store(x, 3)
  ──────────────────────────────────────────→
      W₁            W₂            W₃

  任何线程看到的写入序列必须是这个全序的前缀：
  · 线程 A 可能看到：[W₁]（只看到第一次写入）
  · 线程 B 可能看到：[W₁, W₂]
  · 线程 C 可能看到：[W₁, W₂, W₃]
  · 不可能出现：线程先看到 W₂ 再看到 W₁（违反全序）
```

修改顺序的全序性保证了一个关键不变量：**一旦线程观察到某个写入 W，它就不能"倒退"去观察 W 之前已经被覆盖的值。**

---

## 2. Coherence 要求

修改顺序的 coherence 要求分为三层：

### 2.1 单调读（Monotonic Read）

如果一个读操作 `R` 读取了写操作 `W_a` 的值，且 `R` 后面有另一个读操作 `R'`（同一线程内 sequenced-before），那么 `R'` 不能读取 `W_a` 之前的写入。

```cpp
std::atomic<int> x{0};

// 线程 1
x.store(1, std::memory_order_relaxed); // W₁
x.store(2, std::memory_order_relaxed); // W₂
x.store(3, std::memory_order_relaxed); // W₃

// 线程 2
int a = x.load(std::memory_order_relaxed); // R₁
int b = x.load(std::memory_order_relaxed); // R₂

// 如果 R₁ == 2，则 R₂ 不能是 1（不能"倒退"）
// 如果 R₁ == 2，则 R₂ ∈ {2, 3}
```

### 2.2 写入一致性（Write-Write Coherence）

同一线程中，如果写操作 `W_a` sequenced-before 写操作 `W_b`，那么在 `x` 的修改顺序中，`W_a` 必须出现在 `W_b` 之前。

```cpp
std::atomic<int> x{0};

// 线程 1
x.store(1, std::memory_order_relaxed); // W_a: sequenced-before W_b
x.store(2, std::memory_order_relaxed); // W_b
// 修改顺序中 W₁(1) 必须在 W₂(2) 之前

// 线程 2
int val = x.load(std::memory_order_relaxed);
// 可能读到 0（初始值）、1 或 2
// 但如果读到 1，说明它观察到了 W_a
// 后续再读不可能读到 0（单调性）
```

### 2.3 读取一致性（Read-Read Coherence）

如果读操作 `R_a` happens-before 读操作 `R_b`（不仅是同一线程的 sequenced-before），且 `R_a` 读取了写操作 `W`，那么 `R_b` 必须读取 `W` 或在修改顺序中比 `W` 更晚的写入。

---

## 3. Read-From 关系

当一个读操作 `R` 读取了写操作 `W` 写入的值时，我们说 `R` **reads-from** `W`（记作 `R rf W`）。

```cpp
std::atomic<int> x{0};

// 线程 A
x.store(42, std::memory_order_relaxed); // W

// 线程 B
int v = x.load(std::memory_order_relaxed); // R
// 若 v == 42，则 R rf W
```

Read-from 关系是内存模型推理的核心。C++ 标准通过以下关系组合来定义合法的执行：

```
sequenced-before (sb)     → 同线程顺序
reads-from (rf)           → 读到了哪个写
 modification order (mo)  → 写-写全序
synchronizes-with (sw)    → release-acquire 同步
happens-before (hb)       → sb ∪ sw 的传递闭包

一个执行是合法的 ⟺ 存在一种 mo 和 rf 的选择，
使得不发生 happens-before 前的 reads-from 乱序。
```

---

## 4. Happens-Before 与修改顺序的交互

Happens-before 关系约束了哪些 reads-from 是合法的：

```
规则：如果 W happens-before W'，那么在修改顺序中 W 必须排在 W' 之前。

推论：如果 R happens-before W（W 在修改顺序中排在 R 读取的写入之后），
     则 R 不能读取 W 写入的值——因为读取必须发生在写入之前。
```

**关键区分**：
- **Happens-before** 是程序逻辑的因果关系，由同步操作建立
- **Modification order** 是硬件/编译器自由选择的全序（但必须与 happens-before 一致）

```cpp
std::atomic<int> x{0};

// 线程 A
x.store(1, std::memory_order_release); // W₁ — release

// 线程 B
int a = x.load(std::memory_order_acquire); // R₁ — acquire
// 若 R₁ 读到 W₁ 的值（即 a == 1），
// 则 W₁ synchronizes-with R₁ → W₁ happens-before R₁

// 由于 happens-before 传递性，W₁ happens-before R₁ 之后的所有操作
// 后续读取不可能看到比 W₁ 更早的修改
```

---

## 5. Relaxed Ordering 与修改顺序可见性

`memory_order_relaxed` 只保证原子性和修改顺序一致性，不建立 synchronizes-with 关系。这意味着线程之间的可见性完全由修改顺序决定：

```cpp
std::atomic<int> x{0};
std::atomic<int> y{0};

// 线程 A
x.store(1, std::memory_order_relaxed); // W_x
y.store(1, std::memory_order_relaxed); // W_y

// 线程 B
int a = y.load(std::memory_order_relaxed); // R_y
int b = x.load(std::memory_order_relaxed); // R_x

// 可能结果：a == 1 && b == 0
// 原因：
//   · W_x 和 W_y 在各自的修改顺序中是独立的
//   · 线程 A 内 W_x sequenced-before W_y，但 relaxed 不建立 sw
//   · 线程 B 可以先看到 W_y 的效果，后看到 W_x 之前的值
//   · 这就是 store-buffering（SB）测试的标准结果
```

### 5.1 relaxed 下允许的重排

```
允许的编译器/CPU 行为：

  源码序：        实际执行序（合法）：
  store(x, 1)    store(y, 1)      ← 编译器或 CPU 可以交换
  store(y, 1)    store(x, 1)      ← 因为 relaxed 没有 ordering 约束

  ┌──────────────────────────────────────────┐
  │ x86 (TSO)：                              │
  │   store-store 不重排 → 不会自发发生      │
  │   但 store buffer 可能导致观察延迟        │
  │                                          │
  │ ARM (弱序)：                              │
  │   store-store 可以重排 → 更可能看到 SB   │
  └──────────────────────────────────────────┘
```

---

## 6. Store-Buffering（SB）存入缓冲测试

SB 是最经典的弱一致性 litmus test，用于检测 store-store 重排：

```cpp
// SB (Store Buffering) litmus test
std::atomic<int> x{0};
std::atomic<int> y{0};

// 初始状态：x == 0, y == 0

// 线程 A                              // 线程 B
x.store(1, std::memory_order_relaxed); // W_x
int r1 = y.load(                     // R_y
    std::memory_order_relaxed);

                                       y.store(1, std::memory_order_relaxed); // W_y
                                       int r2 = x.load(                     // R_x
                                           std::memory_order_relaxed);
```

### 6.1 可能的结果

```
r1 == 0 && r2 == 0 ——合法！

  线程 A 的视角：          线程 B 的视角：
  W_x=1 先执行             W_y=1 先执行
  R_y 读到 0（未见 W_y）   R_x 读到 0（未见 W_x）

  这在弱序架构上完全合法：
  · W_x 进入 A 的 store buffer，尚未对 B 可见
  · W_y 进入 B 的 store buffer，尚未对 A 可见
  · 两个读取都从各自的缓存读取旧值
```

### 6.2 如何消除 SB 结果

```cpp
// 方案 1：seq_cst — 最简单且确定有效
x.store(1, std::memory_order_seq_cst); // W_x
int r1 = y.load(std::memory_order_seq_cst); // R_y

// seq_cst 保证全局全序（SC 序），不可能两个都读到 0

// 方案 2：seq_cst fence
// 线程 A
x.store(1, std::memory_order_relaxed);
std::atomic_thread_fence(std::memory_order_seq_cst);
int r1 = y.load(std::memory_order_relaxed);

// 线程 B
y.store(1, std::memory_order_relaxed);
std::atomic_thread_fence(std::memory_order_seq_cst);
int r2 = x.load(std::memory_order_relaxed);

// seq_cst fence 保证两个 fence 在全局全序中排定顺序
// → 不可能 r1 == 0 && r2 == 0
```

---

## 7. 修改顺序的硬件映射

### 7.1 x86-TSO（Total Store Order）

```
x86 的 TSO 模型天然保证：
  · store-store 顺序 → 修改顺序与程序序一致
  · load-load 顺序
  · load-store 顺序
  · 但 store-load 可以不保序（通过 store buffer 实现）

  所以 SB 测试中 r1==0 && r2==0 在 x86 上实际上不会发生。
  但 C++ 标准允许它发生（编译器可能重排），所以仍需同步。
```

### 7.2 ARM/POWER 弱序模型

```
ARM 和 POWER 是弱一致性架构：
  · 所有 store-store、load-load、load-store、store-load 都可能重排
  · 需要显式 barrier（DMB/DSB on ARM, lwsync/sync on POWER）
  · relaxed 操作可以被自由重排
  · 修改顺序的可见性可能有显著延迟

  实际含义：在 ARM 上不加同步的 SB 测试经常得到 r1==0 && r2==0
```

### 7.3 编译器与硬件的双重重排

```
两个层次的重排：

  源码               编译器优化            CPU 执行
  ┌────────┐       ┌────────────┐       ┌──────────────┐
  │ store A │  ──→  │ store B    │  ──→  │ store B      │
  │ store B │       │ store A    │       │ （进入SB）   │
  └────────┘       └────────────┘       │ store A      │
                                         │ （进入SB）   │
                                         └──────────────┘
  即使源码序是 A→B，编译器可能交换为 B→A
  即使编译器序是 A→B，CPU 可能以 B→A 的顺序对外可见

  C++ 原子操作的 memory_order 在编译器和 CPU 两个层次都施加约束
```

---

## 8. 用修改顺序推理程序正确性

### 8.1 Flag 保护模式

```cpp
std::atomic<int> data{0};
std::atomic<int> flag{0};

// 线程 A（生产者）
data.store(42, std::memory_order_relaxed); // W_data
flag.store(1, std::memory_order_release);  // W_flag (release)

// 线程 B（消费者）
while (flag.load(std::memory_order_acquire) != 1) {} // R_flag (acquire)
int val = data.load(std::memory_order_relaxed);       // R_data

// 正确性推理：
// 1. W_data sequenced-before W_flag（同线程）
// 2. flag 的修改顺序中，W_flag 排在初始值 0 之后
// 3. R_flag 读到 W_flag 的值 → R_flag rf W_flag
// 4. W_flag release synchronizes-with R_flag acquire
// 5. 因此 W_data happens-before R_data
// 6. R_data 必须看到 W_data 的效果 → val == 42 ✓

// 关键：修改顺序保证 W_flag 在 flag 的 mo 中排在 0 之后
//       synchronizes-with 建立 happens-before 关系
//       happens-before 保证 data 的写入可见
```

### 8.2 Release Sequence 中的修改顺序

```cpp
std::atomic<int> head{0};

// 修改顺序约束 release sequence：
// head 的修改顺序：initial(0) → W_a(1) → W_b(2) → ...
// 如果 W_a 是 release，W_b 是 RMW 且读取了 W_a 的值，
// 则 W_b 也在以 W_a 为头的 release sequence 中。
// 修改顺序的连续性是 release sequence 存在的前提。
```

---

## 9. 常见陷阱与最佳实践

### 9.1 误解：relaxed 保证全局一致

```cpp
// ❌ 错误理解：relaxed 保证所有线程看到相同的修改顺序
// ✅ 正确理解：relaxed 保证每个原子对象自身的修改顺序一致性
//             不保证跨变量的顺序一致性

std::atomic<int> a{0}, b{0};
// 线程 1                    // 线程 2
a.store(1, relaxed);         b.store(1, relaxed);
int rb = b.load(relaxed);    int ra = a.load(relaxed);
// rb == 0 && ra == 0 是合法的！
```

### 9.2 误解：mo 约束其他变量

```cpp
// ❌ 错误理解：如果 x 的 mo 中 W₁ 在 W₂ 之前，那么 y 的写入也能看到顺序
// ✅ 正确理解：mo 只约束单个原子对象，不同原子对象的 mo 之间没有约束
//    跨变量顺序需要 happens-before（通过 release-acquire 或 seq_cst）
```

### 9.3 最佳实践

```
· 只用 relaxed 做统计计数器、序号生成器等无跨变量依赖的操作
· 需要跨变量可见性 → 至少用 release-acquire
· 不确定 → 用 seq_cst（默认值）
· 验证正确性 → 用 ThreadSanitizer + litmus test
· 性能瓶颈确认后才降级 ordering → 先测后改
```

---

## 10. 总结

```
修改顺序的关键性质：
┌─────────────────────────────────────────────────────────────┐
│ 1. 每个原子对象有且仅有一个修改顺序（全序）                 │
│ 2. 线程不能"倒退"读取（单调读/coherence）                  │
│ 3. 同一线程的写入在 mo 中保序（写入一致性）                │
│ 4. mo 与 happens-before 必须一致                             │
│ 5. relaxed 只保证 mo 一致性，不建立同步关系                 │
│ 6. release-acquire 在 mo 的基础上建立跨线程 happens-before  │
│ 7. seq_cst 在所有原子操作上建立全局全序                     │
└─────────────────────────────────────────────────────────────┘
```
