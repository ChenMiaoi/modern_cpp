---
title: "链接时优化（LTO）"
topic: topics
feature: compiler-opt-lto
standard: C++
status_checked_at: 2026-06-02
---

# 链接时优化（Link-Time Optimization, LTO）

> LTO 打破了翻译单元的边界，让编译器在链接阶段看到整个程序的 IR，进行跨模块内联、去虚拟化、死代码消除等全局优化。它是现代 C++ 性能工程的标配。

---

## 为什么需要 LTO

传统编译模型的局限：

```
传统编译（无 LTO）：
  a.cpp ──编译──→ a.o (机器码)  ──┐
                                    ├── 链接 → app
  b.cpp ──编译──→ b.o (机器码)  ──┘

  问题：
  · 编译器只能看到单个翻译单元的代码
  · a.cpp 中的函数无法内联到 b.cpp 中
  · a.o 中的死代码不会被消除（链接器只处理符号级别）
  · 跨模块的类型信息丢失 → 无法去虚拟化

LTO 编译：
  a.cpp ──编译──→ a.o (LLVM IR / GIMPLE)  ──┐
                                               ├── LTO → 全局优化 → app
  b.cpp ──编译──→ b.o (LLVM IR / GIMPLE)  ──┘

  优势：
  · 跨模块内联
  · 跨模块去虚拟化
  · 全局死代码消除
  · 跨模块常量传播
```

---

## Full LTO vs ThinLTO

### Full LTO

```
Full LTO 工作流程：
  ┌─────────────────────────────────────────────────┐
  │ 1. 所有 .o 文件（含 IR）合并为一个大模块       │
  │    LLVM: llvm-link a.o b.o → merged.bc         │
  │    GCC:  由 lto1 在链接时合并                   │
  │                                                  │
  │ 2. 对合并后的模块运行完整的优化管线              │
  │    · 全局内联                                    │
  │    · 全局死代码消除                              │
  │    · 全局常量传播                                │
  │                                                  │
  │ 3. 生成最终机器码                               │
  └─────────────────────────────────────────────────┘

  优点：优化效果最好（看到全部代码）
  缺点：
    · 编译时间长（O(N) 的模块大小）
    · 内存占用高（所有 IR 驻留内存）
    · 不能并行优化
```

### ThinLTO

```
ThinLTO 工作流程：
  ┌─────────────────────────────────────────────────┐
  │ 阶段 1：模块摘要（Module Summary / Summary Index）│
  │  · 每个 .o 只保留摘要信息（函数签名、调用图、    │
  │    类型信息），不保留完整 IR                     │
  │  · 摘要体积远小于完整 IR（通常 < 10%）          │
  └──────────────────────┬──────────────────────────┘
                         │ 摘要索引
                         ▼
  ┌─────────────────────────────────────────────────┐
  │ 阶段 2：全局决策（Import / Cross-Module Analysis）│
  │  · 基于摘要计算跨模块内联决策                    │
  │  · 确定哪些函数需要从其他模块导入                │
  │  · 确定死代码和符号可见性                        │
  └──────────────────────┬──────────────────────────┘
                         │ Import 列表
                         ▼
  ┌─────────────────────────────────────────────────┐
  │ 阶段 3：并行优化（Parallel Backend）             │
  │  · 每个模块独立优化                              │
  │  · 只导入需要的函数（非整个模块）               │
  │  · 完全可并行（利用多核）                        │
  └──────────────────────┬──────────────────────────┘
                         │
                         ▼
  ┌─────────────────────────────────────────────────┐
  │ 阶段 4：链接                                      │
  │  · 将优化后的 .o 文件链接为最终可执行文件        │
  └─────────────────────────────────────────────────┘

  优点：
    · 编译速度快（并行优化，O(1) 的 per-module 开销）
    · 内存占用低（每个模块独立处理）
    · 增量编译友好（只重新优化修改过的模块）

  缺点：
    · 跨模块信息不如 Full LTO 完整
    · 某些跨模块优化（如全局常量传播）效果略差
```

---

## 编译命令

### Clang / LLVM

```bash
# ThinLTO（推荐）
clang++ -flto=thin -O2 a.cpp b.cpp -o app

# Full LTO
clang++ -flto=full -O2 a.cpp b.cpp -o app

# 分步编译（先编译 IR，再链接优化）
clang++ -flto=thin -O2 -c a.cpp -o a.o    # a.o 含 LLVM bitcode
clang++ -flto=thin -O2 -c b.cpp -o b.o    # b.o 含 LLVM bitcode
clang++ -flto=thin -O2 a.o b.o -o app     # LTO 链接

# 查看 ThinLTO 的 import 决策
clang++ -flto=thin -O2 -Wl,--plugin-opt=thinlto-index-only a.o b.o -o app
# 只生成索引文件，不实际链接

# 控制 ThinLTO import 限制
clang++ -flto=thin -O2 -mllvm -thinlto-import-instr-limit=100 a.o b.o -o app
# 导入的指令数上限（默认 100）
```

### GCC

```bash
# Full LTO
g++ -flto -O2 a.cpp b.cpp -o app

# 带 fat LTO 对象（同时包含 IR 和机器码）
g++ -ffat-lto-objects -flto -O2 -c a.cpp -o a.o
# a.o 同时有 LTO IR 和机器码，可以用普通链接器

# ThinLTO（GCC 10+）
g++ -flto=auto -O2 a.cpp b.cpp -o app
# 自动选择线程数

# 控制 LTO 并行度
g++ -flto=4 -O2 a.cpp b.cpp -o app   # 使用 4 个线程
```

---

## ThinLTO Import 机制

ThinLTO 的核心是**按需导入**——只导入内联决策需要的函数：

```
模块 A (a.cpp)：                  模块 B (b.cpp)：
  void process() {                  inline int helper(int x) {
      int r = helper(42);               return x * 2 + 1;
      ...                            }
  }

ThinLTO 决策：
  1. 分析摘要：A 调用 B::helper()
  2. 评估：helper 体小且被频繁调用 → 值得内联
  3. Import：将 helper 的 IR 从 B 导入 A
  4. 内联：在 A 中将 helper 内联到 process()
  5. 后续优化：常量传播 → helper(42) = 85
```

```
导入限制：
  ┌─────────────────────────────────────────────────┐
  │ thinlto-import-instr-limit（默认 100）           │
  │  · 每个模块最多导入 100 条指令的函数             │
  │  · 防止导入大函数导致编译时间爆炸                │
  │                                                  │
  │ 可调整：                                         │
  │  · 提高限制 → 更好的跨模块优化，编译更慢        │
  │  · 降低限制 → 编译更快，优化效果略差            │
  │                                                  │
  │ PGO 数据辅助：                                   │
  │  · 热函数自动提高 import 限制                   │
  │  · 冷函数不导入                                  │
  └─────────────────────────────────────────────────┘
```

---

## 跨模块内联

LTO 最直接的收益是跨模块内联：

```cpp
// utils.cpp
namespace utils {
    inline int fast_hash(int x) {  // 即使标记 inline，无 LTO 时跨模块不内联
        x = ((x >> 16) ^ x) * 0x45d9f3b;
        x = ((x >> 16) ^ x) * 0x45d9f3b;
        x = (x >> 16) ^ x;
        return x;
    }
}

// main.cpp
#include "utils.h"
void process(std::vector<int>& data) {
    for (auto& x : data)
        x = utils::fast_hash(x);  // LTO 时可以内联
}
```

```bash
# 无 LTO：fast_hash 是函数调用
clang++ -O2 main.cpp utils.cpp -o app_no_lto

# 有 LTO：fast_hash 被内联，循环可能向量化
clang++ -flto=thin -O2 main.cpp utils.cpp -o app_lto

# 对比性能
hyperfine ./app_no_lto ./app_lto
```

---

## 死代码消除

LTO 可以消除跨模块的死代码：

```bash
# 查看 LTO 死代码消除效果
# 使用 nm 查看符号数量
nm app_no_lto | wc -l     # 无 LTO 的符号数
nm app_lto | wc -l         # 有 LTO 的符号数（通常更少）

# GCC 的 LTO 死代码消除
g++ -flto -O2 main.cpp unused.cpp -o app
# unused.cpp 中未被调用的函数在 LTO 阶段被消除
```

---

## 字符串合并

LTO 可以合并跨模块的相同字符串常量：

```
无 LTO：
  a.o: .rodata: "error: invalid input"  (独立一份)
  b.o: .rodata: "error: invalid input"  (独立一份)
  链接后：两份相同字符串

有 LTO：
  优化器发现两份相同字符串 → 合并为一份
  → 节省 .rodata 段空间
```

---

## CMake 集成

```cmake
# CMake 启用 LTO

# 方法 1：全局启用
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

# 方法 2：按目标启用
add_executable(app main.cpp utils.cpp)
set_property(TARGET app PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

# 方法 3：检查编译器是否支持 LTO
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported)
if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# 方法 4：使用 Ninja + ThinLTO 的最佳实践
# CMakeLists.txt
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto=thin")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto=thin")
```

```bash
# 构建命令
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
ninja -j$(nproc)  # ThinLTO 可以并行优化
```

---

## LTO 与调试信息

LTO 会影响调试体验：

```
LTO 对调试信息的影响：
  ┌─────────────────────────────────────────────────┐
  │ 问题：                                           │
  │  · 跨模块内联后，栈帧可能"丢失"               │
  │  · LTO 可能删除未使用的函数 → 断点失效         │
  │  · 类型信息在 import 后可能不完整               │
  │                                                  │
  │ 解决方案：                                       │
  │  · 使用 -flto=thin -g（保留调试信息）           │
  │  · ThinLTO 保留每个模块的独立 debug info        │
  │  · Full LTO 合并所有 debug info → 更大但更完整  │
  │  · 使用 -fno-lto 保留调试构建                   │
  └─────────────────────────────────────────────────┘
```

```bash
# LTO + 调试信息
clang++ -flto=thin -O2 -g main.cpp utils.cpp -o app_debug

# 分离调试信息
clang++ -flto=thin -O2 -g -gsplit-dwarf main.cpp utils.cpp -o app
# .dwo 文件包含调试信息，减小可执行文件体积
```

---

## 代码体积影响

LTO 对代码体积的影响是双向的：

```
LTO 的体积影响：
  ┌─────────────────────────────────────────────────┐
  │ 减小体积：                                       │
  │  · 跨模块死代码消除                              │
  │  · 字符串常量合并                                │
  │  · 全局内联后的常量传播 → 消除更多代码          │
  │                                                  │
  │ 增大体积：                                       │
  │  · 跨模块内联 → 函数体复制                      │
  │  · 更激进的内联决策                              │
  │                                                  │
  │ 通常净效果：-O2 + LTO 体积接近 -O2 无 LTO      │
  │ -Os + LTO 通常减小体积                          │
  └─────────────────────────────────────────────────┘
```

```bash
# 对比代码体积
clang++ -O2 main.cpp utils.cpp -o app_no_lto && size app_no_lto
clang++ -flto=thin -O2 main.cpp utils.cpp -o app_lto && size app_lto

# 查看具体段的大小
llvm-size --totals app_lto
```

---

## LTO 的构建时间开销

```
编译时间对比（典型项目，~100 个翻译单元）：

  模式            编译时间    链接时间    总时间
  ─────────────────────────────────────────────
  -O2 无 LTO     30s         2s          32s
  -O2 ThinLTO    32s         8s          40s   (+25%)
  -O2 FullLTO    32s         45s         77s   (+140%)

  ThinLTO 的链接时间增加来自：
  · 模块摘要生成
  · Import 决策计算
  · 并行后端优化

  Full LTO 的链接时间增加来自：
  · 所有 IR 合并为一个模块
  · 全局优化（单线程）
```

---

## 延伸阅读

- [内联](/topics/compiler-optimizations/inlining) — LTO 的核心收益是跨模块内联
- [去虚拟化](/topics/compiler-optimizations/devirtualization) — LTO 使全程序去虚拟化成为可能
- [PGO](/topics/compiler-optimizations/pgo) — PGO + LTO = 最佳性能组合
- [C++ 编译器优化全景](/topics/compiler-optimizations) — 整体优化管线
- [工具链与生态](/topics/toolchain) — 构建系统配置
- LLVM ThinLTO 设计文档：https://clang.llvm.org/docs/ThinLTO.html
