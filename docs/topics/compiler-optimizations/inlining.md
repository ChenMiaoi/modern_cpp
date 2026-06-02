---
title: "函数内联"
topic: topics
feature: compiler-opt-inlining
standard: C++
status_checked_at: 2026-06-02
---

# 函数内联（Function Inlining）

> 内联是编译器优化管线中**最重要的单个优化**。它不仅消除函数调用开销，更重要的是将被调用者的代码暴露给调用者的上下文，解锁常量传播、死代码消除、循环向量化等后续优化。

---

## 内联的本质

```
内联前：                          内联后：
┌──────────────────┐             ┌──────────────────────────┐
│ caller:          │             │ caller:                  │
│   ...            │             │   ...                    │
│   call foo(a, b) │   ──────→   │   ; foo 的代码直接展开： │
│   ...            │             │   tmp = a + b;            │
│                  │             │   if (tmp > 0) ret tmp;   │
│ foo(x, y):       │             │   else ret -tmp;          │
│   z = x + y      │             │   ...                     │
│   if (z > 0)     │             │                           │
│     ret z        │             │ 后续优化可以：             │
│   else           │             │ · 若 a=5, b=3 → 常量折叠  │
│     ret -z       │             │ · 死分支消除              │
└──────────────────┘             └──────────────────────────┘
```

内联消除的开销：
- 参数压栈/传参指令
- `call`/`ret` 指令对（分支预测压力）
- 栈帧建立/销毁
- 寄存器保存/恢复

---

## 内联阈值与代价模型

### LLVM 内联代价模型

LLVM 的内联决策由 `InlineCost` 分析完成：

```
InlineCost = Σ(instruction_weight) - Threshold_bonus

决策规则：
  if InlineCost ≤ Threshold → 内联
  else → 不内联

Threshold 受以下因素影响：
  ┌───────────────────────────────────────────────────┐
  │ 基础阈值（由优化级别决定）：                       │
  │   -O0:  0    (几乎不内联)                          │
  │   -O1:  75                                          │
  │   -O2:  225                                         │
  │   -O3:  275                                         │
  │   -Os:  50   (优化体积时门槛更低)                  │
  │   -Oz:  25                                          │
  │                                                     │
  │ 加分项（降低内联成本）：                           │
  │   · 调用点是热路径（+15000）                       │
  │   · 参数是常量（+2000 per constant arg）           │
  │   · 只有一个调用点（+1500）                        │
  │   · 函数体很小（< 5 条指令，直接内联）            │
  │                                                     │
  │ 减分项（增加内联成本）：                           │
  │   · 函数体很大                                     │
  │   · 多个调用点（代码膨胀）                         │
  │   · 含有循环                                       │
  │   · 含有函数调用（间接开销不确定）                 │
  └───────────────────────────────────────────────────┘
```

### GCC 内联参数

```bash
# GCC 内联相关的 --param 选项
g++ -O2 \
    --param inline-unit-growth=30 \         # 单个翻译单元允许增长 30%
    --param large-function-growth=200 \     # 单个函数允许增长 200%
    --param inline-insns-auto=15 \          # 自动内联的指令数上限
    --param inline-insns-single=40 \        # 单一调用点的指令数上限
    --param max-inline-insns-size=500 \     # 硬上限
    test.cpp

# 查看 GCC 的内联决策
g++ -O2 -fdump-ipa-inline test.cpp
# 输出 .inline 文件包含每个内联决策的理由
```

### LLVM 内联阈值控制

```bash
# 手动设置内联阈值
clang++ -O2 -mllvm -inline-threshold=300 test.cpp

# 查看内联决策
clang++ -O2 -Rpass=inline test.cpp
# 输出：test.cpp:5:5: remark: 'foo' inlined into 'bar'

# 查看未内联的原因
clang++ -O2 -Rpass-missed=inline test.cpp
# 输出：test.cpp:5:5: remark: 'foo' not inlined: cost=500 threshold=225
```

---

## 强制内联与禁止内联

### 强制内联

```cpp
// GCC / Clang
__attribute__((always_inline))
inline void fast_path(int x) {
    // 编译器保证内联，即使阈值不够
}

// MSVC
__forceinline void fast_path(int x) {
    // MSVC 的强制内联
}

// C++20 无标准属性，但 GCC/Clang 支持：
[[gnu::always_inline]]       // GCC 10+, Clang 13+
[[gnu::flatten]]              // 递归内联整个调用树
void aggressive(int x) { }
```

### 禁止内联

```cpp
// GCC / Clang
__attribute__((noinline))
void debug_dump(const Widget& w) {
    // 永不内联 — 用于调试函数、性能关键的隔离点
}

// MSVC
__declspec(noinline) void debug_dump(const Widget& w) { }

// 用于控制代码布局：
__attribute__((noinline, cold))
void error_handler(int code) {
    // 不内联 + 标记为冷路径 → 编译器将其放入 .text.unlikely 段
}
```

### 实际应用模式

```cpp
// 模式 1：热路径强制内联
[[gnu::always_inline]] [[gnu::hot]]
inline uint64_t hash_mix(uint64_t x) {
    x ^= x >> 23;
    x *= 0x2127599bf4325c37ULL;
    x ^= x >> 47;
    return x;
}

// 模式 2：冷路径标记不内联
[[gnu::noinline]] [[gnu::cold]]
void slow_path(const char* msg) {
    fprintf(stderr, "ERROR: %s\n", msg);
    abort();
}

// 模式 3：Profile-guided 的条件内联
// PGO 数据会自动调整阈值：热路径阈值提高（更积极内联），
// 冷路径阈值降低（避免内联）
```

---

## 递归内联

编译器对递归函数的内联有严格限制：

```
递归内联策略：
  ┌─────────────────────────────────────────────────┐
  │ LLVM 默认行为：                                  │
  │  · 不内联递归调用                                │
  │  · 但可以内联递归函数到其调用者                  │
  │    （一次展开，不递归展开）                      │
  │                                                  │
  │ GCC 默认行为：                                   │
  │  · 同上                                          │
  │  · -foptimize-sibling-calls 优化尾递归为循环    │
  │                                                  │
  │ 尾递归优化：                                     │
  │  · tail call → 等价于 goto                       │
  │  · 不增加栈帧                                    │
  │  · 不算"内联"，但效果类似                        │
  └─────────────────────────────────────────────────┘
```

```cpp
// 尾递归 → 编译器优化为循环
int factorial(int n, int acc = 1) {
    if (n <= 1) return acc;
    return factorial(n - 1, n * acc);  // 尾位置
}

// 非尾递归 → 不能优化为循环
int tree_depth(Node* node) {
    if (!node) return 0;
    return 1 + std::max(tree_depth(node->left),   // 非尾位置
                        tree_depth(node->right));
}
```

---

## LTO 与跨模块内联

LTO（Link-Time Optimization）使内联可以跨越翻译单元边界：

```
传统编译（无 LTO）：
  a.cpp → a.o (foo 内联优化)  ──┐
                                 ├── 链接 → a.out
  b.cpp → b.o (bar 调用 foo)  ──┘
  问题：bar() 中对 foo() 的调用无法内联（foo 定义在 a.cpp）

LTO 编译：
  a.cpp → a.o (含 LLVM IR)   ──┐
                                ├── LTO 链接 → 跨模块内联 → a.out
  b.cpp → b.o (含 LLVM IR)   ──┘
  LTO 时 foo() 可以内联到 bar() 中
```

```bash
# Clang LTO 内联
clang++ -flto=thin -O2 a.cpp b.cpp -o app
# ThinLTO 自动跨模块内联热路径函数

# GCC LTO 内联
g++ -flto -O2 a.cpp b.cpp -o app
# Full LTO：所有模块合并后统一内联决策
```

---

## 去虚拟化通过内联

内联是实现去虚拟化（devirtualization）的前提：

```cpp
class Base {
public:
    virtual int compute(int x) = 0;
};

class Derived : public Base {
public:
    int compute(int x) override { return x * 2; }
};

void process(Base& obj) {
    int r = obj.compute(42);  // 虚调用
}
```

```
内联前（虚调用）：
  %vtable = load ptr, ptr %obj        ; 加载 vtable 指针
  %vfn = getelementptr ptr, %vtable, 1 ; 获取 compute 的槽位
  %fn = load ptr, ptr %vfn             ; 加载函数指针
  call %fn(ptr %obj, i32 42)           ; 间接调用

内联 + 去虚拟化后（如果编译器能推断类型是 Derived）：
  ; 虚调用被替换为直接调用
  ; 直接调用又被内联
  ret i32 84                           ; 42 * 2 = 84
```

---

## 内联对调试的影响

内联会破坏调试信息中的调用栈：

```
-O0 + -g：
  (gdb) bt
  #0  foo(x=42) at test.cpp:5
  #1  bar() at test.cpp:10
  #2  main() at test.cpp:15

-O2 + -g（foo 被内联到 bar）：
  (gdb) bt
  #0  bar() at test.cpp:5    ← foo 的行号，但栈帧是 bar
  #1  main() at test.cpp:15
  # foo 的栈帧"消失"了
```

```bash
# 调试构建建议：禁用内联或降低内联级别
clang++ -O1 -g -fno-inline test.cpp     # 禁止内联
clang++ -O2 -g -finline-hint-functions test.cpp  # 只内联标记了 inline 的

# Release + 调试符号：用于性能分析（不要求精确栈帧）
clang++ -O2 -g -fno-omit-frame-pointer test.cpp

# GCC 的调试友好选项
g++ -O2 -g -finline-limit=0 test.cpp    # 禁止自动内联
g++ -O2 -g -fno-inline test.cpp          # 完全禁止
```

---

## 内联与代码体积

内联是代码膨胀的主要来源：

```
内联膨胀示意：
  foo() 20 条指令，被调用 10 次
  bar() 30 条指令，被调用 5 次

  内联前：总代码 = 20 + 30 + 调用开销 ≈ 60 条
  全部内联后：总代码 = 20×10 + 30×5 = 350 条 ← 膨胀 5.8 倍

  I-Cache 影响：
  ┌──────────────────────────────────────────────┐
  │  L1 I-Cache 典型大小：32KB（~8K 指令）       │
  │  膨胀后的工作集可能超出 L1 → 频繁 I-Cache   │
  │  miss → 执行速度反而下降                     │
  └──────────────────────────────────────────────┘
```

```bash
# 优化体积的内联策略
clang++ -Os test.cpp   # 激进优化体积，内联门槛很低
clang++ -Oz test.cpp   # 极端优化体积

# 查看代码体积变化
size a.out  # text 段大小变化

# GCC：控制单个翻译单元的体积增长
g++ -O2 --param inline-unit-growth=10 test.cpp
# 限制内联后的代码膨胀为 10%（默认 30%）
```

---

## 编译器内联 Remarks

```bash
# 查看所有内联决策
clang++ -O2 -Rpass=inline test.cpp -c
# 输出示例：
# test.cpp:10:5: remark: 'fast_hash' inlined into 'process' with (cost=0, threshold=225) [-Rpass=inline]

# 查看未内联的原因
clang++ -O2 -Rpass-missed=inline test.cpp -c
# 输出示例：
# test.cpp:15:5: remark: 'complex_fn' not inlined into 'caller' because too costly to inline (cost=1200, threshold=225) [-Rpass-missed=inline]

# 查看内联后的代码变化
clang++ -O2 -Rpass-analysis=inline test.cpp -c

# GCC 等价：IPA inline dump
g++ -O2 -fdump-ipa-inline-details test.cpp
```

---

## 内联的最佳实践

```
┌─────────────────────────────────────────────────────────┐
│ DO（推荐做法）                                          │
│                                                         │
│  · 小函数放在头文件中定义（隐式 inline）               │
│  · 使用 __attribute__((always_inline)) 标记性能关键的  │
│    小函数（如 SIMD intrinsics wrapper）                 │
│  · 使用 __attribute__((noinline)) 标记错误处理路径     │
│  · 用 -Rpass=inline / -Rpass-missed=inline 审计内联决策│
│  · 用 LTO 打破翻译单元边界                              │
│  · PGO 数据帮助编译器区分热/冷路径                     │
│                                                         │
│ DON'T（避免做法）                                       │
│                                                         │
│  · 不要强制内联大函数（> 100 条指令）                  │
│  · 不要在递归函数上期望内联                             │
│  · 不要忽略 -Os 下的内联回退                           │
│  · 不要在虚函数上标记 always_inline（虚函数不内联）    │
│    除非配合 final/去虚拟化                             │
└─────────────────────────────────────────────────────────┘
```

---

## 延伸阅读

- [SROA](/topics/compiler-optimizations/sroa) — 内联后暴露更多 SROA 机会
- [去虚拟化](/topics/compiler-optimizations/devirtualization) — 内联是去虚拟化的前提
- [LTO](/topics/compiler-optimizations/lto) — 跨模块内联
- [PGO](/topics/compiler-optimizations/pgo) — Profile 引导内联决策
- [C++ 编译器优化全景](/topics/compiler-optimizations) — 整体优化管线
- [RAII 与资源管理](/topics/raii) — 小对象 RAII 的内联特性
- [值类别深度解析](/topics/value-categories-deep-dive) — 右值引用与内联的交互
