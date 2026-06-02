---
title: "SROA：标量替换聚合体"
topic: topics
feature: compiler-opt-sroa
standard: C++
status_checked_at: 2026-06-02
---

# SROA：标量替换聚合体（Scalar Replacement of Aggregates）

> SROA 是 LLVM 优化管线中排名前三的关键 pass。它将聚合类型（结构体、数组）的 `alloca` 拆解为独立的标量 SSA 寄存器，是连接"源码中的值语义"和"高效的寄存器级代码"之间的桥梁。

---

## SROA 做什么

SROA 的核心任务：**把内存中的结构体拆解为一组独立的标量变量，使其可以被 `mem2reg` 提升为 SSA 寄存器**。

```
优化前（前端生成的 LLVM IR）：
  %p = alloca { i32, i32 }
  store { i32, i32 } { i32 10, i32 20 }, ptr %p
  %x_ptr = getelementptr inbounds { i32, i32 }, ptr %p, i32 0, i32 0
  %y_ptr = getelementptr inbounds { i32, i32 }, ptr %p, i32 0, i32 1
  %x = load i32, ptr %x_ptr
  %y = load i32, ptr %y_ptr
  %sum = add i32 %x, %y

SROA + mem2reg 后：
  ; %p 的 alloca 被完全消除
  ; x, y 成为纯 SSA 值，常量传播进一步折叠
  ret i32 30
```

SROA 是 `mem2reg` 的泛化：
- `mem2reg`：处理单个标量 alloca → SSA
- SROA：先将聚合体拆解为多个标量 alloca，再交给 `mem2reg`

---

## 何时触发 SROA

SROA 在以下条件全部满足时生效：

```
┌─────────────────────────────────────────────────────────┐
│ SROA 前提条件                                             │
│                                                         │
│  1. alloca 的类型是聚合体（struct/array/vector）         │
│  2. 对聚合体的访问模式是"可分析的"                       │
│     · GEP 偏移是常量                                     │
│     · 没有跨字段的 load/store                            │
│  3. alloca 的地址没有逃逸到以下场景：                    │
│     · 传递给无法内联的函数                               │
│     · 存入全局变量                                       │
│     · 作为内联汇编的操作数                               │
│     · 被 volatile load/store 访问                        │
└─────────────────────────────────────────────────────────┘
```

### 简单案例

```cpp
struct Point { int x, y; };

int sum(Point p) {
    return p.x + p.y;
}
```

查看 SROA 前后的 IR：

```bash
# 生成未优化 IR
clang++ -O0 -Xclang -disable-O0-optnone -emit-llvm -S sroa.cpp -o sroa_before.ll

# 仅运行 SROA pass
opt -passes=sroa sroa_before.ll -S -o sroa_after.ll

# GCC 等价：查看 FRE/PRE 优化效果
g++ -O1 -fdump-tree-fre sroa.cpp
```

SROA 前的 IR 中 `Point p` 是一个 `{ i32, i32 }` 类型的 `alloca`，所有对 `p.x` 和 `p.y` 的访问都是通过 GEP + load/store。SROA 后，`alloca` 被拆成两个独立的 `i32` alloca，随后 `mem2reg` 将它们提升为 SSA 寄存器。

---

## SROA 的工作原理

SROA 的算法分三步：

```
步骤 1：分析（Analyze）
  ┌─────────────────────────────────────────────┐
  │ 遍历所有对 alloca 的 use                     │
  │ · GEP 访问 → 记录偏移和大小                  │
  │ · 整块 memcpy → 记录源/目标                  │
  │ · 如果发现无法拆解的访问 → 标记为不可优化    │
  └─────────────────────────────────────────────┘
          │
          ▼
步骤 2：切片（Partition / Slice）
  ┌─────────────────────────────────────────────┐
  │ 将聚合体按访问模式切为多个"切片"(slice)      │
  │ · { i32, i32 } → slice [0,4) 和 [4,8)       │
  │ · 如果字段从未被访问 → 标记为死切片          │
  └─────────────────────────────────────────────┘
          │
          ▼
步骤 3：重写（Rewrite）
  ┌─────────────────────────────────────────────┐
  │ 将每个切片替换为独立的 alloca                 │
  │ · GEP + load → load from new alloca          │
  │ · GEP + store → store to new alloca           │
  │ · 删除原始 alloca                             │
  │ · 委托 mem2reg 将新 alloca 提升为 SSA        │
  └─────────────────────────────────────────────┘
```

### 部分拆解

SROA 不需要所有字段都可分析。即使某个字段被间接访问（如通过指针），只要其他字段满足条件，SROA 也会部分拆解：

```cpp
struct Mixed {
    int simple;     // 只通过 p.simple 访问
    int complex;    // 通过 int* 指针间接访问
};

int partial(Mixed m) {
    int* ptr = &m.complex;
    *ptr = 42;
    return m.simple + m.complex;
}
```

SROA 会拆解 `simple` 字段为独立 alloca，但 `complex` 因为地址逃逸到 `ptr` 而保留原始 alloca 中。

---

## SROA 与 NRVO 的交互

NRVO（Named Return Value Optimization）在前端就消除了返回值拷贝，SROA 在后续 IR 层面进一步处理残留的聚合体：

```cpp
struct Result { int code; double value; };

Result compute() {
    Result r;
    r.code = 0;
    r.value = 3.14;
    return r;
}
```

```
NRVO 生效时：
  调用者提供返回地址 → compute() 直接在该地址构造 r
  → SROA 无事可做（没有 alloca）

NRVO 未生效时（例如多个返回路径）：
  compute() 中有 %r = alloca { i32, double }
  → SROA 将其拆解为两个独立 alloca
  → 各自提升为 SSA 寄存器
  → 最终通过 store 写入返回地址
```

**关键点**：NRVO 消除的是跨函数边界的拷贝，SROA 消除的是函数内部的聚合体内存访问。两者互补，不冲突。

---

## SROA 失败的场景

### 地址逃逸

```cpp
struct Config { int timeout; int retries; };

void init(Config* cfg);  // 外部函数，编译器看不到

Config make_config() {
    Config c;
    c.timeout = 30;
    c.retries = 3;
    init(&c);       // c 的地址逃逸 → SROA 无法拆解
    return c;
}
```

如果 `init` 被内联且内联后地址不再逃逸，SROA 可以在内联之后重新生效。这就是为什么 pass 顺序中 SROA 在内联之后再运行一次。

### volatile 访问

```cpp
struct HW { uint32_t status; uint32_t data; };
volatile HW* regs = /* MMIO 地址 */;

uint32_t read_data() {
    return regs->data;  // volatile → 编译器不能拆解或消除访问
}
```

volatile 保证访问顺序和次数，SROA 不能改变 volatile load/store 的语义。

### 内联汇编约束

```cpp
struct Vec { float x, y, z, w; };

void asm_use(Vec v) {
    asm volatile("" : : "x"(v.x), "y"(v.y));  // 寄存器约束绑定字段
    // 内联汇编通过约束引用了结构体字段
    // SROA 可能无法确定汇编是否通过其他方式访问了整个结构体
}
```

内联汇编对编译器来说是一个黑盒。如果汇编约束引用了聚合体的地址（而非值），SROA 必须保守地保留原始 alloca。

### 跨字段 memcpy

```cpp
struct Big { char data[4096]; };

void copy(Big* dst, const Big* src) {
    *dst = *src;  // 整块赋值 → 编译器生成 memcpy
}
```

SROA 对大结构体的整块 memcpy 通常不拆解——拆解后会产生数千条独立 load/store，比一次 memcpy 更慢。

---

## GCC 中的等价优化

GCC 没有名为 SROA 的 pass，但有功能等价的优化链：

```
GCC 优化链：
  FRE (Full Redundancy Elimination)
    → 消除冗余的 load
  PRE (Partial Redundancy Elimination)
    → 消除部分冗余的内存访问
  DSE (Dead Store Elimination)
    → 消除死存储
  phiprop
    → PHI 节点传播

这些 pass 协作完成类似 SROA 的工作，但不如 LLVM SROA 彻底。
```

```bash
# 查看 GCC 的优化效果
g++ -O2 -fdump-tree-all sroa.cpp
# 关注 .fre, .pre, .dse 文件
```

---

## 高级主题：SROA 与 ABI

SROA 的拆解会影响函数的 ABI 表示。当结构体被完全拆解为标量后，后端可能选择寄存器传参而非栈传参：

```
SROA 前（ABI 视角）：
  define void @foo(ptr %out) {
    %tmp = alloca { i32, i32 }
    ; 整个结构体在内存中
  }

SROA 后 + 标量传播：
  define i32 @bar() {
    ret i32 30
    ; 完全没有内存访问，结果通过寄存器返回
  }
```

这对性能的影响是显著的：一次函数调用从"压栈 → 调用 → 从栈读取 → 返回"变为"寄存器传参 → 直接返回"。

---

## 查看 SROA 的工作

```bash
# LLVM：打印 SROA 前后的 IR
clang++ -O2 -mllvm -print-after=sroa -mllvm -print-module-scope sroa.cpp -c 2>&1

# LLVM：使用 opt 工具单步执行
opt -passes='sroa,mem2reg' input.ll -S

# GCC：查看结构体标量化
g++ -O2 -fdump-tree-optimized sroa.cpp
# 查看 .optimized 文件，确认结构体访问是否被消除

# 编译器 remark：查看 SROA 的决策
clang++ -O2 -Rpass=sroa sroa.cpp -c
# 或
clang++ -O2 -Rpass-missed=sroa sroa.cpp -c
```

---

## 性能影响

SROA 对性能的影响主要体现在：

```
┌─────────────────────────────────────────────────────────┐
│ 直接收益                                                │
│  · 消除 load/store → 减少内存访问                       │
│  · SSA 寄存器 → 启用后续常量传播、CSE、死代码消除       │
│  · 寄存器传参 → 减少栈操作                              │
│                                                         │
│ 间接收益（SROA 启用的下游优化）                         │
│  · 全局值编号（GVN）可以合并相同的标量计算              │
│  · 循环优化可以分析纯寄存器的循环依赖                   │
│  · 指令调度有更多自由度                                 │
│                                                         │
│ 潜在负面影响                                            │
│  · 极大结构体的拆解可能增加寄存器压力                    │
│  · 部分拆解可能产生额外的 spill/fill                    │
└─────────────────────────────────────────────────────────┘
```

实际基准测试中，SROA 几乎总是正收益或无影响。禁用 SROA（`-mllvm -enable-sroa=false`）通常会导致 5-20% 的性能回退。

---

## 延伸阅读

- [内联优化](/topics/compiler-optimizations/inlining) — 内联为 SROA 创造条件
- [别名分析](/topics/compiler-optimizations/alias-analysis) — 别名信息帮助 SROA 判断地址是否逃逸
- [LTO](/topics/compiler-optimizations/lto) — 跨模块 SROA
- [C++ 编译器优化全景](/topics/compiler-optimizations) — 整体优化管线
- [值类别深度解析](/topics/value-categories-deep-dive) — 值语义与 SROA 的关系
- [性能优化](/topics/performance) — SROA 在实际性能调优中的角色
- LLVM 官方文档：[SROA Pass](https://llvm.org/docs/Passes.html#sroa-scalar-replacement-of-aggregates)
