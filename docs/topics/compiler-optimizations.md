# C++ 编译器优化全景

> 编译器优化是 C++ 性能的"隐藏层"。你写的代码和实际执行的机器码之间隔着一整套优化管线。理解这些优化不是可选的——它决定了你能否写出真正高效的代码，以及能否在性能出问题时定位原因。

---

## 优化管线总览

```
C++ 源代码
    │
    ▼
┌─────────────────────────────────────────────────────────┐
│ 前端（Frontend）                                         │
│  词法分析 → 语法分析 → 语义分析 → IR 生成                │
│                                                         │
│  C++ 特有优化：                                          │
│  · constexpr/consteval 求值                              │
│  · 拷贝消除（RVO/NRVO）                                  │
│  · 模板实例化去重                                        │
│  · [[likely]]/[[unlikely]] 分支权重标注                  │
└──────────────────────┬──────────────────────────────────┘
                       │ LLVM IR / GIMPLE (GCC)
                       ▼
┌─────────────────────────────────────────────────────────┐
│ 中端（Middle-end）—— 优化的主战场                        │
│                                                         │
│  函数级优化：                                            │
│  · 内联（Inlining）← 最重要的单个优化                    │
│  · SROA（结构体拆解为标量）                               │
│  · 死代码消除（DCE）/ 死存储消除（DSE）                   │
│  · 常量传播 / 常量折叠                                   │
│  · 全局值编号（GVN）                                     │
│  · 公共子表达式消除（CSE）                                │
│                                                         │
│  循环优化：                                              │
│  · 循环不变量外提（LICM）                                │
│  · 循环展开（Unroll）                                    │
│  · 循环向量化（Vectorize）                               │
│  · 循环交换（Interchange）                               │
│  · 循环旋转（Rotation）                                  │
│  · 循环融合 / 循环分裂                                   │
│                                                         │
│  控制流优化：                                            │
│  · 跳转线程化（Jump Threading）                          │
│  · 尾合并（Tail Merging）                                │
│  · 条件传播（Correlated Propagation）                    │
│  · SimplifyCFG                                          │
│                                                         │
│  过程间优化（IPO / LTO）：                               │
│  · 过程间内联                                           │
│  · 去虚拟化（Devirtualization）                          │
│  · 过程间常量传播                                        │
│  · 全局死代码消除                                        │
│                                                         │
│  内存优化：                                              │
│  · 别名分析（Alias Analysis / TBAA）                     │
│  · 内存依赖分析                                          │
│  · MemCpyOpt                                            │
│  · ScalarEvolution（循环索引分析）                       │
└──────────────────────┬──────────────────────────────────┘
                       │ 优化后的 IR
                       ▼
┌─────────────────────────────────────────────────────────┐
│ 后端（Backend）                                          │
│                                                         │
│  · 指令选择（SelectionDAG / GlobalISel / FastISel）      │
│  · 指令调度（Instruction Scheduling）                    │
│  · 寄存器分配（Register Allocation）                     │
│  · 窥孔优化（Peephole Optimization）                     │
│  · 尾调用优化（Tail Call Optimization）                  │
│  · 目标特定优化（SSE/AVX/NEON 向量化）                   │
└──────────────────────┬──────────────────────────────────┘
                       │
                       ▼
                机器码（.o / .obj）
```

---

## 一、前端优化：C++ 特有的编译期变换

### 1.1 拷贝消除（RVO / NRVO）

C++ 标准**允许**（C++17 起强制要求部分场景）编译器消除临时对象的拷贝/移动：

```
源码：
  std::string make() {
      std::string s = "hello";
      return s;  // NRVO：直接在调用者的栈帧上构造
  }
  auto x = make();

无优化：
  make() 中构造 s → 拷贝/移动到临时对象 → 拷贝/移动到 x → 析构临时对象 → 析构 s

RVO 后：
  直接在 x 的地址上构造 → 0 次拷贝，0 次移动

  调用者栈帧：
  ┌──────────────┐
  │ x 的地址     │ ← make() 的返回值直接写入这里
  └──────────────┘
```

**C++17 强制拷贝消除（guaranteed copy elision）**：
```cpp
auto p = Widget{};  // C++17 起：即使 Widget 的移动构造被删除，也能编译
// 编译器直接在 p 的地址上构造 Widget，不存在"临时对象"
```

### 1.2 constexpr / consteval 求值

```
源码：
  constexpr int factorial(int n) {
      return n <= 1 ? 1 : n * factorial(n - 1);
  }
  int arr[factorial(5)];  // 编译期求值 → arr[120]

编译器行为：
  ┌──────────────────────────────────────┐
  │ constexpr 解释器（编译期内置 VM）    │
  │                                      │
  │ 输入：factorial(5)                   │
  │ 执行：5 * 4 * 3 * 2 * 1 = 120       │
  │ 输出：常量 120                       │
  └──────────────────────────────────────┘
  
  最终 IR 中：int arr[120];  // 零运行时开销
```

### 1.3 模板实例化去重

```
同一模板特化在不同翻译单元中实例化时，链接器只保留一份：

  // a.cpp                          // b.cpp
  vector<int> v1;                    vector<int> v2;

  两个翻译单元各自实例化 vector<int>
  → 链接时合并为一个实例（COMDAT folding）
  → 不会增加二进制体积
```

---

## 二、内联（Inlining）：最重要的单个优化

内联是编译器优化的**基石**——它将函数体复制到调用点，消除了函数调用开销，并为后续优化打开了大门。

### 2.1 为什么内联是最重要的

```
内联前：                      内联后：
┌─────────────┐              ┌─────────────────────────┐
│ 调用 foo(x) │              │ y = x * 2 + 1;          │ ← 函数体展开
│             │              │ if (y > 0) return y;    │
│ foo:        │              │ else return -y;         │
│   y = x * 2 │              │                         │
│   y += 1    │    ────→     │ 后续优化可以：           │
│   if y>0    │              │ · 常量传播（若 x 已知）  │
│     ret y   │              │ · 死代码消除            │
│   else      │              │ · 循环展开              │
│     ret -y  │              │ · 向量化                │
└─────────────┘              └─────────────────────────┘
```

内联解锁的后续优化：
- **常量传播**：如果参数是常量，内联后整个函数体可以折叠为常量
- **死代码消除**：`if (constexpr_condition)` 的未走分支被消除
- **循环优化**：内联后循环体变大，可能暴露向量化机会
- **别名分析**：内联后编译器能看到更多指针关系

### 2.2 内联决策

```
LLVM 的内联启发式（简化）：

  内联收益 = 调用开销消除 + 后续优化收益
  内联成本 = 代码膨胀 → I-cache 压力 + 编译时间

  ┌──────────────────────────────────────────┐
  │ 以下情况倾向于内联：                      │
  │ · 函数体很小（< 几条指令）               │
  │ · 只有一个调用点（不膨胀）               │
  │ · 参数是编译期常量（解锁常量折叠）       │
  │ · 标记了 __attribute__((always_inline))  │
  │ · [[gnu::flatten]]（递归内联整个调用链）  │
  │                                          │
  │ 以下情况不内联：                          │
  │ · 函数体太大                              │
  │ · 递归函数（一般不内联，除非尾递归）     │
  │ · 虚函数（无法静态解析）                  │
  │ · -Os（优化体积时门槛更严格）            │
  └──────────────────────────────────────────┘
```

### 2.3 实际案例

```cpp
// 源码
int compute(int x) {
    return x * x + 2 * x + 1;
}
int result = compute(42);

// 内联 + 常量折叠后：
// compute(42) → 42*42 + 2*42 + 1 → 1764 + 84 + 1 = 1849
// 最终 IR：int result = 1849;  // 零运行时计算
```

---

## 三、SROA：结构体拆解为标量

SROA（Scalar Replacement of Aggregates）是 LLVM 中最重要的优化之一——将结构体拆解为独立的标量变量。

```
源码：
  struct Point { int x, y; };
  Point p;
  p.x = 10;
  p.y = 20;
  return p.x + p.y;

SROA 前（IR 中 p 是 alloca）：
  %p = alloca { i32, i32 }
  store { i32, i32 } { i32 10, i32 20 }, ptr %p
  %x = load i32, ptr getelementptr(%p, 0, 0)
  %y = load i32, ptr getelementptr(%p, 0, 1)
  %sum = add i32 %x, %y

SROA 后（拆解为独立变量）：
  ; p 被完全消除，x 和 y 成为 SSA 寄存器
  %sum = add i32 10, 20    ; 常量传播进一步折叠为 30
```

**SROA 是 `mem2reg` 的泛化**：`mem2reg` 只处理单一变量的 alloca→SSA 转换；SROA 处理结构体、数组等复合类型的拆解。

---

## 四、循环优化

### 4.1 循环不变量外提（LICM）

```
源码：
  for (int i = 0; i < n; ++i) {
      a[i] = x * y + i;  // x * y 在循环中不变
  }

LICM 后：
  int tmp = x * y;       // 提到循环外
  for (int i = 0; i < n; ++i) {
      a[i] = tmp + i;
  }
```

### 4.2 循环展开（Loop Unrolling）

```
源码：
  for (int i = 0; i < 4; ++i) a[i] = 0;

完全展开后：
  a[0] = 0; a[1] = 0; a[2] = 0; a[3] = 0;
  // 消除分支、消除循环变量递增

部分展开（4 路）：
  for (int i = 0; i < n; i += 4) {
      a[i]   = 0;
      a[i+1] = 0;
      a[i+2] = 0;
      a[i+3] = 0;
  }
  // 分支次数减少 4 倍，为向量化创造条件
```

### 4.3 循环向量化（Loop Vectorization）

```
源码：
  for (int i = 0; i < n; ++i)
      c[i] = a[i] + b[i];

SSE2 向量化后（128-bit，4 个 float）：
  for (i = 0; i < n; i += 4) {
      va = _mm_load_ps(&a[i]);      // 加载 4 个 float
      vb = _mm_load_ps(&b[i]);      // 加载 4 个 float
      vc = _mm_add_ps(va, vb);      // 一条指令完成 4 次加法
      _mm_store_ps(&c[i], vc);      // 存储 4 个结果
  }

AVX2 向量化后（256-bit，8 个 float）：
  // 同样逻辑，但一次处理 8 个 float → 吞吐量翻倍

AVX-512 向量化后（512-bit，16 个 float）：
  // 一次处理 16 个 float → 吞吐量再翻倍
```

**自动向量化的条件**：
```
编译器需要证明：
  1. 循环次数在编译期可知，或有明确的尾部处理
  2. 没有循环依赖（a[i] 不依赖 a[i-1] 的计算结果）
  3. 内存访问是连续的（无步长 > 1 的间接访问）
  4. 没有别名冲突（a 和 b 不重叠）

  __restrict__ 告诉编译器"指针不别名" → 满足条件 4
```

### 4.4 循环不变条件外提 + 循环旋转

```
源码：
  while (x > 0) {
      if (cond) { x -= a; }
      else      { x -= b; }
  }

循环旋转后（Loop Rotation）：
  if (x > 0) {
      do {
          if (cond) { x -= a; }
          else      { x -= b; }
      } while (x > 0);
  }
  // 消除了一次条件分支（循环底部直接跳回循环体）
```

### 4.5 循环融合 / 循环分裂

```
循环融合（Loop Fusion）：
  // 源码：两个独立循环
  for (i) a[i] = f(i);
  for (i) b[i] = g(a[i]);
  
  // 融合后：一次遍历
  for (i) { a[i] = f(i); b[i] = g(a[i]); }
  // 减少循环开销，提高缓存局部性

循环分裂（Loop Fission）：
  // 源码：循环体过大
  for (i) { a[i] = f(i); b[i] = g(i); c[i] = h(i); }
  
  // 分裂后：每个循环更紧凑，可能分别向量化
  for (i) a[i] = f(i);
  for (i) b[i] = g(i);
  for (i) c[i] = h(i);
```

---

## 五、控制流优化

### 5.1 跳转线程化（Jump Threading）

```
源码：
  if (x > 0) {
      // ... 一些代码 ...
      if (x > 0) {  // 冗余条件（x 在此期间未改变）
          do_something();
      }
  }

Jump Threading 后：
  if (x > 0) {
      // ... 一些代码 ...
      do_something();  // 直接跳过冗余条件检查
  }
```

### 5.2 尾合并（Tail Merging）

```
源码：
  switch (type) {
      case A: process_a(); cleanup(); return;
      case B: process_b(); cleanup(); return;
      case C: process_c(); cleanup(); return;
  }

Tail Merging 后：
  switch (type) {
      case A: process_a(); goto common;
      case B: process_b(); goto common;
      case C: process_c(); goto common;
  }
  common: cleanup(); return;
  // 三个 return 合并为一个 → 减少代码体积
```

### 5.3 条件传播（Correlated Propagation）

```
源码：
  if (x > 0) {
      // ... 20 行代码 ...
      if (x > 0) {  // x 在 if 块内未修改 → 条件恒为 true
          do_work();
      }
  }

Correlated Propagation 后：
  if (x > 0) {
      // ... 20 行代码 ...
      do_work();  // 条件被传播消除
  }
```

---

## 六、别名分析与 TBAA

### 6.1 别名分析（Alias Analysis）

```
问题：编译器需要知道两个指针是否指向同一内存

  void foo(int* a, int* b, int* c) {
      *a = 1;
      *b = 2;
      *c = *a;  // c 可能等于 a 吗？可能等于 b 吗？
  }

  // 如果编译器能证明 c != b，则 *c = *a = 1
  // 如果 c 可能等于 b，则 *c = 2（因为 *b = 2 覆盖了 *a = 1）
```

### 6.2 TBAA（Type-Based Alias Analysis）

```
C/C++ 的严格别名规则：不同类型的指针通常不别名

  int* pi = ...;
  float* pf = ...;
  *pi = 42;
  *pf = 3.14f;
  printf("%d", *pi);  // 编译器可以假设 *pi 仍然是 42
                       // 因为 int* 和 float* 根据 TBAA 不别名

TBAA 元数据（LLVM IR）：
  store i32 42, ptr %pi, !tbaa !{!"int", !"any pointer"}
  store float 3.14, ptr %pf, !tbaa !{!"float", !"any pointer"}
  ; 两个 !tbaa 标签类型不同 → 编译器认为不别名
```

### 6.3 `__restrict__` 告诉编译器

```cpp
// 没有 restrict → 编译器必须假设 a 和 b 可能重叠
void add(float* a, float* b, int n) {
    for (int i = 0; i < n; ++i) a[i] += b[i];
    // 编译器不能向量化（a[i] 写入可能影响后续 b[i] 读取）
}

// 有 restrict → 编译器知道 a 和 b 不重叠 → 可以安全向量化
void add(float* __restrict__ a, float* __restrict__ b, int n) {
    for (int i = 0; i < n; ++i) a[i] += b[i];
    // 编译器可以向量化：先加载所有 b[i]，再批量加到 a[i]
}
```

---

## 七、去虚拟化（Devirtualization）

### 7.1 虚函数的性能问题

```
虚函数调用：
  obj->virtual_method(args);
  
  机器码：
  mov rax, [obj]           ; 加载 vtable 指针
  mov rax, [rax + offset]  ; 从 vtable 加载函数指针
  call rax                  ; 间接调用

  问题：
  · 间接调用 → CPU 分支预测器难以预测目标
  · 无法内联（编译期不知道实际类型）
  · 阻碍后续所有优化
```

### 7.2 `final` 触发去虚拟化

```cpp
class Base {
    virtual void foo();
};
class Derived final : public Base {
    void foo() override;
};

void call(Derived* d) {
    d->foo();  // Derived 是 final → 编译器知道实际类型
    // 直接调用 Derived::foo()，无需查 vtable
    // 可以内联！
}
```

### 7.3 编译器自动去虚拟化

```cpp
// 编译器可以追踪对象的实际类型
Derived d;
Base* b = &d;
b->foo();  // 编译器知道 b 指向 Derived → 直接调用 Derived::foo()

// 连续调用同一对象的虚函数 → 编译器可以缓存 vtable 查找
obj->a();  // 加载 vtable
obj->b();  // 复用已加载的 vtable（如果编译器能证明 obj 类型未变）
```

---

## 八、死代码消除（DCE）与死存储消除（DSE）

### 8.1 DCE：消除不影响程序输出的代码

```
源码：
  int x = compute();  // 如果 x 从未被使用
  int y = other();    // 如果 y 只被用于后续不需要的计算
  return 42;

DCE 后：
  return 42;  // x 和 y 的计算被完全消除
```

### 8.2 DSE：消除被覆盖的存储

```
源码：
  *p = 1;
  // ... 一些不读取 *p 的代码 ...
  *p = 2;     // 第一次写入被覆盖 → 可以消除

DSE 后：
  // *p = 1;  被消除
  *p = 2;
```

### 8.3 Aggressive DSE

```
源码：
  void foo(int* p) {
      *p = 42;
  }  // 函数返回后，*p 是局部状态 → 写入可以消除

  如果编译器能证明 p 指向的内存在函数返回后不会被读取
  → 整个写入操作被消除
```

---

## 九、过程间优化（IPO / LTO）

### 9.1 链接时优化（LTO）

```
传统编译：
  a.cpp → a.o (已优化)
  b.cpp → b.o (已优化)
  链接: a.o + b.o → 可执行文件
  // 每个 .o 独立优化，跨文件信息丢失

LTO 编译：
  a.cpp → a.o (带 IR 元数据)
  b.cpp → b.o (带 IR 元数据)
  链接: 合并 IR → 全局优化 → 生成机器码
  // 跨文件内联、跨文件常量传播、全局死代码消除
```

### 9.2 LTO 的实际收益

```
启用 LTO 的典型收益：
  · 跨文件内联（最重要）
  · 跨文件常量传播
  · 全局死代码消除（未使用的跨文件函数）
  · 跨文件类型层次分析（去虚拟化）
  · 跨文件内存优化

代价：
  · 链接时间显著增加
  · 内存使用增加（所有 IR 同时在内存中）
  · 增量编译支持差（改一个文件需要重新链接全部）
```

### 9.3 ThinLTO（轻量 LTO）

```
ThinLTO（LLVM 默认 LTO 模式）：
  · 链接时只合并函数摘要（summary），不合并完整 IR
  · 并行优化每个模块（摘要指导跨模块内联决策）
  · 比 full LTO 快很多，但收益略低
  
  典型场景：大型项目用 ThinLTO，小型关键库用 Full LTO
```

---

## 十、Profile-Guided 优化（PGO）

### 10.1 PGO 的工作流程

```
Step 1: 插桩编译
  源码 → 插桩版可执行文件（记录分支走哪个、循环执行几次、函数调用频率）

Step 2: 收集 Profile
  运行插桩版，执行典型工作负载 → 生成 .profraw 文件

Step 3: 优化编译
  源码 + .profraw → 优化版可执行文件
  
  编译器利用 Profile 数据：
  · 热函数优先内联
  · 冷路径（异常处理、错误检查）移到代码段末尾
  · 分支权重标注（likely/unlikely 由实际数据决定，而非猜测）
  · 循环展开次数由实际迭代次数决定
  · 基本块排序（热路径连续放置，减少 I-cache miss）
```

### 10.2 PGO 的实际收益

```
Google 的内部实践：
  · Chromium: PGO 带来 ~10-15% 性能提升
  · 内部 C++ 服务: 平均 ~15-20% 吞吐量提升
  
  主要收益来源：
  · 更准确的内联决策（40%+ 的收益）
  · 更好的代码布局（30%+）
  · 更好的分支预测（20%+）
```

### 10.3 BOLT（Binary Optimization and Layout Tool）

```
BOLT 在链接后的二进制上工作：
  1. 收集运行时 Profile（通过 perf 或 LBR）
  2. 重排基本块和函数（热函数聚集，冷函数分离）
  3. 优化分支布局
  
  额外收益：5-10%（在 PGO 基础上）
  Facebook 在其服务上使用 BOLT 获得了显著收益
```

---

## 十一、优化屏障：什么阻止编译器优化

### 11.1 `volatile`

```
volatile 告诉编译器："每次访问都必须真正读/写内存"

  volatile int x = 0;
  x = 1;    // 必须写入内存
  x = 2;    // 必须写入内存（即使前一行刚写过）
  int y = x; // 必须从内存读取（即使编译器知道刚写入 2）

  编译器不能：
  · 消除对 volatile 变量的读写
  · 重排序 volatile 操作之间的相对顺序
  · 将 volatile 值缓存在寄存器中
```

### 11.2 原子操作与内存序

```
std::atomic<int> x;
x.store(1, std::memory_order_seq_cst);  // 全序保证
x.load(std::memory_order_acquire);       // acquire 语义

  编译器不能：
  · 将 acquire 之后的操作重排序到 acquire 之前
  · 将 release 之前的操作重排序到 release 之后
  · 消除对 atomic 变量的访问（除非能证明无其他线程可见）
```

### 11.3 内联汇编（`asm volatile`）

```
  asm volatile("cpuid" : "=a"(eax) : "a"(0) : "ebx", "ecx", "edx");
  
  编译器必须：
  · 在指定位置生成这条指令
  · 不能移动它（volatile 保证）
  · 不能假设它不修改"未列出"的寄存器（clobber list）
```

### 11.4 外部函数调用

```
  foo();      // 编译器不知道 foo 做了什么
  x = *p;    // foo 可能修改了 *p → 必须重新加载
  
  // 如果 foo 是内联的，编译器可以看到它不修改 *p
  // → x = *p 可以复用之前缓存的值
```

---

## 十二、实用技巧：如何观察和控制优化

### 12.1 Godbolt Compiler Explorer

```
https://godbolt.org/ — 在线查看编译器输出

使用方法：
  1. 输入 C++ 源码
  2. 选择编译器和优化级别
  3. 查看生成的汇编代码
  
  关键技巧：
  · 使用 -O2（而非 -O0）观察真实优化效果
  · 使用 -S -emit-llvm 查看 LLVM IR（比汇编更易读）
  · 使用 -Rpass=inline 查看哪些函数被内联
  · 使用 -Rpass=loop-vectorize 查看向量化决策
```

### 12.2 常用编译器标志

```
-O0: 无优化（调试用，编译最快）
-O1: 基本优化（消除明显死代码，简单内联）
-O2: 推荐的生产优化级别（平衡编译时间和运行性能）
-O3: 激进优化（更激进的内联、向量化、循环变换）
-Os: 优化体积（适合嵌入式、减少 I-cache 压力）
-Oz: 极致体积优化（进一步减少代码大小）

-Ofast: -O3 + 违反 IEEE 754 的浮点优化（可能改变计算结果）

Clang 特有：
  -Rpass=.*              : 报告所有优化决策
  -Rpass-missed=.*       : 报告错过的优化机会
  -Rpass-analysis=.*     : 报告优化分析结果
  -fsave-optimization-record : 生成 YAML 优化记录

GCC 特有：
  -fopt-info             : 报告优化决策
  -fopt-info-optimized   : 只报告成功优化
  -fopt-info-missed      : 只报告错过的优化
```

### 12.3 LLVM IR 查看

```bash
# 生成 LLVM IR（人类可读格式）
clang++ -S -emit-llvm -O2 source.cpp -o source.ll

# 查看优化 pass 的效果
clang++ -S -emit-llvm -O2 -mllvm -print-after-all source.cpp 2>&1 | less

# 只看特定 pass 后的 IR
clang++ -S -emit-llvm -O2 -mllvm -print-after=inline source.cpp
```

---

## 十三、C++ 特有的优化交互

### 13.1 `noexcept` 的优化影响

```cpp
// 没有 noexcept → 编译器必须生成异常处理代码
void process(std::vector<int>& v) {
    v.push_back(42);  // 可能抛异常 → 编译器生成 unwind 表
}

// 有 noexcept（或编译器推断为 noexcept）→ 无需 unwind 表
// vector::reserve 中的 move_if_noexcept 就利用了这一点：
//   noexcept move → 用 move（快）
//   可能抛异常的 move → 用 copy（慢但安全）
```

### 13.2 `constexpr` 的优化影响

```cpp
constexpr auto table = generate_lut();  // 编译期计算 → 嵌入 .rodata 段
// 运行时零开销，直接从只读数据段读取

// C++20 constexpr 更多场景：
constexpr std::vector<int> v = {1, 2, 3};  // 编译期 vector（C++20 起）
// 但注意：constexpr vector 在编译结束后其堆内存被释放
// 运行时使用的是编译器嵌入的常量副本
```

### 13.3 `[[no_unique_address]]` 的代码大小影响

```cpp
// 空分配器占空间
struct Bad {
    int data;
    std::allocator<int> alloc;  // 通常 1 字节，但对齐后可能占 4-8 字节
};

// 使用 [[no_unique_address]] 消除
struct Good {
    int data;
    [[no_unique_address]] std::allocator<int> alloc;  // 0 字节
};
// sizeof(Good) == sizeof(int) → 4 字节
// 更小的对象 → 更好的缓存利用率 → 更快的遍历
```

### 13.4 Trivially Relocatable 的未来

```
当前（C++26 前）：vector::reserve 必须逐个 move + destroy
  for (each element) {
      new (dst) T(std::move(*src));  // move construct
      src->~T();                      // destroy
  }

有了 trivially relocatable（P2786）：
  if constexpr (std::trivially_relocatable<T>) {
      memcpy(dst, src, n * sizeof(T));  // 一次 memcpy
  }
  // 对 unique_ptr、shared_ptr、string 等简单类型
  // 性能提升可达 5-10 倍（在 reserve 热路径上）
```

---

## 总结：优化决策树

```
写 C++ 代码时的编译器优化意识：

  1. 函数是否应该被内联？
     · 小函数（< 10 行）→ 编译器通常自动内联
     · 热路径大函数 → 考虑 __attribute__((always_inline))
     · 虚函数 → 使用 final 帮助去虚拟化

  2. 数据结构是否缓存友好？
     · 连续内存（vector、array）→ 遍历快
     · 节点式（list、map）→ 遍历慢但插入删除快
     · SoA vs AoS → 根据访问模式选择

  3. 循环是否可以被优化？
     · 没有循环依赖 → 可能被向量化
     · __restrict__ → 帮助别名分析
     · 循环体内的不变量 → 提到循环外

  4. 是否可以利用编译期计算？
     · constexpr 函数 → 编译期求值
     · 模板元编程 → 编译期生成代码
     · consteval → 强制编译期

  5. 是否给了编译器足够的信息？
     · noexcept → 消除异常处理开销
     · __restrict__ → 消除别名假设
     · [[likely]]/[[unlikely]] → 优化分支布局
     · alignas → 确保 SIMD 对齐

核心原则：
  · 先让编译器能优化（给信息），再手动优化
  · 用 -O2 编译并检查汇编，确认优化生效
  · 不要猜测性能——测量
  · 理解优化的边界（什么能优化，什么不能）
