---
title: "Profile 引导优化（PGO）"
topic: topics
feature: compiler-opt-pgo
standard: C++
status_checked_at: 2026-06-02
---

# Profile 引导优化（Profile-Guided Optimization, PGO）

> PGO 让编译器基于真实的运行时数据做出优化决策——哪些分支更可能被执行、哪些函数是热路径、哪些代码几乎不走。这比静态启发式准确得多，通常带来 5-20% 的性能提升。

---

## PGO 的工作流程

```
PGO 三阶段流程：

  阶段 1：插桩构建（Instrumentation Build）
  ┌─────────────────────────────────────────────────┐
  │ 源码 → 编译（-fprofile-generate）→ 插桩可执行文件│
  │                                                  │
  │ 插桩后的代码会在运行时记录：                     │
  │  · 每个分支的执行次数                            │
  │  · 每个函数的调用次数                            │
  │  · 每个基本块的执行频率                          │
  │  · 间接调用的目标（用于去虚拟化）               │
  └──────────────────────┬──────────────────────────┘
                         │ 运行插桩程序
                         ▼
  阶段 2：收集 Profile 数据
  ┌─────────────────────────────────────────────────┐
  │ 运行插桩程序（使用代表性工作负载）               │
  │                                                  │
  │ GCC：生成 .gcda 文件                             │
  │ LLVM：生成 .profraw 文件                         │
  │                                                  │
  │ ⚠️ 关键：工作负载必须有代表性！                  │
  │  · 不要用单元测试作为 profile 输入               │
  │  · 应该用生产环境的实际负载                      │
  └──────────────────────┬──────────────────────────┘
                         │ 处理 profile 数据
                         ▼
  阶段 3：优化构建（Optimized Build）
  ┌─────────────────────────────────────────────────┐
  │ 源码 + profile 数据 → 编译（-fprofile-use）      │
  │                                                  │
  │ 编译器利用 profile 数据：                        │
  │  · 优化分支布局（热路径紧凑）                   │
  │  · 指导内联决策（热函数积极内联）               │
  │  · 函数布局（热函数放在一起）                   │
  │  · 条件去虚拟化                                  │
  │  · 函数分裂（热/冷分离）                        │
  └─────────────────────────────────────────────────┘
```

---

## LLVM PGO

### 步骤 1：插桩构建

```bash
# 生成插桩版本
clang++ -fprofile-generate -O2 main.cpp utils.cpp -o app_instrumented

# 插桩到指定目录
clang++ -fprofile-generate=/tmp/pgo_profiles -O2 main.cpp -o app_instrumented

# 插桩后的可执行文件体积增加 ~15-30%
# 运行速度降低 ~10-30%（取决于分支密度）
```

### 步骤 2：收集 Profile

```bash
# 运行插桩程序
./app_instrumented --real-workload

# 生成 .profraw 文件（默认在当前目录）
# 或者在 -fprofile-generate=DIR 指定的目录下

# 如果有多个 .profraw（例如多进程、测试套件）
llvm-profdata merge -output=merged.profdata *.profraw

# 查看 profile 内容
llvm-profdata show merged.profdata
# 输出：函数调用次数、分支频率等

# 查看具体函数的 profile
llvm-profdata show --function=process merged.profdata
```

### 步骤 3：优化构建

```bash
# 使用 profile 数据编译
clang++ -fprofile-use=merged.profdata -O2 main.cpp utils.cpp -o app_optimized

# 如果 profile 文件在不同路径，需要 remapping
clang++ -fprofile-use=merged.profdata \
        -fprofile-remapping-file=remap.txt \
        -O2 main.cpp -o app_optimized

# 查看 PGO 引导的优化决策
clang++ -fprofile-use=merged.profdata -O2 \
        -Rpass=pgo-instrumentation \
        -Rpass=inline main.cpp -c
```

---

## GCC PGO

```bash
# 步骤 1：插桩构建
g++ -fprofile-generate -O2 main.cpp utils.cpp -o app_instrumented

# 步骤 2：收集 profile
./app_instrumented --real-workload
# 生成 .gcda 文件

# 步骤 3：优化构建
g++ -fprofile-use -O2 main.cpp utils.cpp -o app_optimized

# 自动采样（不需要插桩运行）
g++ -fprofile-generate -fprofile-update=atomic -O2 main.cpp -o app_instrumented
# atomic 更新模式：线程安全，但有少量额外开销

# 查看 GCC 的 PGO 优化效果
g++ -fprofile-use -O2 -fdump-tree-profile_estimate main.cpp
```

---

## Profile 数据的格式转换

```bash
# AutoFDO 工具链：perf → LLVM profile
# 步骤 1：用 perf 采样（不需要插桩）
perf record -b -e cycles:u ./app --real-workload
# -b：记录分支信息（LBR，需要 Intel Haswell+）

# 步骤 2：转换为 LLVM profile 格式
create_llvm_prof --binary=./app --out=profile.afdo perf.data

# 步骤 3：使用 AutoFDO profile 编译
clang++ -fprofile-sample-use=profile.afdo -O2 main.cpp -o app_afdo

# GCC AutoFDO
create_gcov --binary=./app --gcov=profile.gcov --profile=perf.data
g++ -fauto-profile=profile.gcov -O2 main.cpp -o app_afdo
```

---

## 分支预测提示

PGO 最直接的优化是分支布局——将热路径放在连续的内存地址上：

```cpp
void process(int input) {
    if (is_normal(input)) {       // PGO: 99% 走这个分支
        fast_path(input);
    } else {                      // PGO: 1% 走这个分支
        slow_path(input);
    }
}
```

```
无 PGO 的代码布局：
  ┌───────────────────────────────────────────────┐
  │ 代码段：                                      │
  │   LBB0: is_normal(input)                      │
  │   LBB1: fast_path(input)   ; 默认 fallthrough │
  │   LBB2: jmp end                               │
  │   LBB3: slow_path(input)   ; 跳转目标         │
  │   LBB4: end                                   │
  └───────────────────────────────────────────────┘

  PGO 后的代码布局：
  ┌───────────────────────────────────────────────┐
  │ .text.hot（热段）：                           │
  │   LBB0: is_normal(input)                      │
  │   LBB1: fast_path(input)                      │
  │   LBB2: end                                   │
  │                                               │
  │ .text.unlikely（冷段）：                      │
  │   LBB3: slow_path(input)                      │
  └───────────────────────────────────────────────┘

  效果：
  · 热路径代码紧凑 → 更好的 I-cache 利用率
  · 冷路径分离 → 不污染热路径的 cache line
  · 分支预测更准确（fallthrough 总是快于 taken branch）
```

---

## 函数分裂（Function Splitting）

PGO 可以将一个函数的热部分和冷部分分离到不同的段：

```cpp
void handle_request(Request& req) {
    validate(req);           // 热路径（99%）
    process(req);            // 热路径
    if (unlikely_error()) {  // 冷路径（<1%）
        log_error(req);
        retry(req);
        cleanup(req);
    }
    respond(req);            // 热路径
}
```

```
函数分裂后：
  .text.hot:
    handle_request.hot:    ; 只包含热路径代码
      validate()
      process()
      respond()

  .text.unlikely:
    handle_request.cold:   ; 冷路径代码单独放置
      log_error()
      retry()
      cleanup()
```

```bash
# LLVM 函数分裂
clang++ -fprofile-use=merged.profdata -O2 \
        -mllvm -hot-cold-split=true main.cpp

# GCC 函数分裂（-freorder-blocks-and-partition）
g++ -fprofile-use -O2 -freorder-blocks-and-partition main.cpp
```

---

## 函数布局优化

PGO 还可以优化函数的排列顺序——将热函数放在一起，冷函数放在末尾：

```
无 PGO 的函数布局（按编译顺序）：
  .text:  main() → validate() → error_handler() → process() → retry()
  问题：error_handler 和 retry 是冷函数，夹在热函数中间

PGO 后的函数布局：
  .text.hot: main() → process() → validate()   ← 热函数聚集
  .text:     parse_args()                       ← 中性函数
  .text.unlikely: error_handler() → retry()     ← 冷函数聚集

  效果：
  · 热函数的地址相邻 → TLB 和 I-cache 友好
  · 减少 I-cache line 浪费在冷代码上
```

```bash
# 查看函数布局
nm --numeric-sort app_optimized | grep ' T '  # 按地址排列的函数
# 或者用 perf 工具查看 I-cache 利用率
perf stat -e L1-icache-load-misses ./app_optimized
```

---

## AutoFDO（采样式 PGO）

AutoFDO 不需要插桩，而是使用硬件性能计数器采样：

```
传统 PGO vs AutoFDO：

  传统 PGO：
    插桩编译 → 运行 → .gcda/.profraw → 优化编译
    ✅ 精确计数
    ❌ 需要额外的插桩运行
    ❌ 插桩有 ~10-30% 性能开销

  AutoFDO：
    正常编译 → perf 采样 → 转换 profile → 优化编译
    ✅ 零插桩开销（在生产环境直接采样）
    ✅ 使用真实生产负载
    ❌ 采样有噪声（精度略低）
    ❌ 需要 debug info（-g）用于映射源码位置
```

```bash
# AutoFDO 完整流程
# 1. 编译（需要 debug info + 优化）
clang++ -O2 -g main.cpp utils.cpp -o app

# 2. 在生产环境采样
perf record -b -e cycles:u -o perf.data -- ./app --production-load
# -b：记录分支信息（Branch Stack / LBR）
# -e cycles:u：只采样用户态

# 3. 转换 profile
create_llvm_prof \
    --binary=./app \
    --profile=perf.data \
    --out=profile.afdo \
    --format=extbinary

# 4. 使用 AutoFDO profile 重新编译
clang++ -fprofile-sample-use=profile.afdo -O2 -g \
        main.cpp utils.cpp -o app_optimized
```

---

## BOLT：链接后优化器

BOLT（Binary Optimization and Layout Tool）在二进制层面进行布局优化，独立于编译器：

```
BOLT 工作流程：
  正常编译 + 链接 → 可执行文件
                         │
                         ▼
  perf record -b → perf.data
                         │
                         ▼
  llvm-bolt app -data perf.data -o app_bolt
                         │
                         ▼
  优化后的可执行文件

  BOLT 做什么：
  · 函数重排（基于 profile 的热函数聚集）
  · 基本块重排（热路径连续）
  · 冷代码分离
  · 间接调用优化
  · 函数内基本块重排
```

```bash
# BOLT 完整流程
# 1. 编译（需要 relocations）
clang++ -O2 -Wl,--emit-relocs main.cpp -o app
# 或
clang++ -O2 -Wl,-q main.cpp -o app

# 2. 采样
perf record -e cycles:u -j any,u -o perf.data -- ./app

# 3. 转换 perf 数据为 BOLT 格式
perf2bolt -p perf.data -o perf.fdata ./app

# 4. 运行 BOLT
llvm-bolt ./app -o ./app_bolt \
    -data=perf.fdata \
    -reorder-blocks=ext-tsp \
    -reorder-functions=hfsort+ \
    -split-functions \
    -split-all-cold
```

---

## PGO + LTO 组合

PGO + LTO 是性能最优的组合：

```bash
# LLVM: PGO + ThinLTO
clang++ -flto=thin -fprofile-generate -O2 a.cpp b.cpp -o app_instrumented
./app_instrumented --workload
llvm-profdata merge -output=default.profdata *.profraw
clang++ -flto=thin -fprofile-use=default.profdata -O2 a.cpp b.cpp -o app_final

# GCC: PGO + LTO
g++ -flto -fprofile-generate -O2 a.cpp b.cpp -o app_instrumented
./app_instrumented --workload
g++ -flto -fprofile-use -O2 a.cpp b.cpp -o app_final
```

```
PGO + LTO 的协同效应：
  ┌─────────────────────────────────────────────────┐
  │ PGO 知道哪些函数是热的                          │
  │ LTO 能跨模块内联                                │
  │                                                  │
  │ 组合效果：                                       │
  │  · 热路径的跨模块调用被积极内联                │
  │  · 冷路径的函数调用保留（不内联，避免膨胀）    │
  │  · 全程序优化 + Profile 引导 = 最优决策         │
  │                                                  │
  │ 典型收益：                                       │
  │  · PGO alone:     +5-15%                        │
  │  · LTO alone:     +3-10%                        │
  │  · PGO + LTO:     +10-25%                       │
  └─────────────────────────────────────────────────┘
```

---

## 延伸阅读

- [内联](/topics/compiler-optimizations/inlining) — PGO 数据指导内联决策
- [去虚拟化](/topics/compiler-optimizations/devirtualization) — PGO 支持推测性去虚拟化
- [LTO](/topics/compiler-optimizations/lto) — PGO + LTO 组合
- [向量化](/topics/compiler-optimizations/vectorization) — PGO 帮助向量化代价模型
- [C++ 编译器优化全景](/topics/compiler-optimizations) — 整体优化管线
- [性能优化](/topics/performance) — PGO 在实际项目中的应用
- LLVM AutoFDO：https://github.com/google/autofdo
- BOLT：https://github.com/llvm/llvm-project/tree/main/bolt
