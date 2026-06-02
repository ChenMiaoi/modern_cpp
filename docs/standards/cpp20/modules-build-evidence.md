---
title: "Modules 构建实践：BMI 格式、依赖扫描、CMake 集成与编译器差异"
topic: cpp20
feature: modules-build
standard: C++20
status_checked_at: 2026-06-02
standard_refs:
  - draft: N4861
    clause: "[module.unit]"
  - draft: N4861
    clause: "[module.import]"
  - draft: N4861
    clause: "[module.global]"
proposals:
  - P1103R3
  - P1185R2
  - P1689R5
  - P1857R3
  - P1874R1
  - P2577R1
exercises: []
solutions: []
---

# Modules 构建实践（BMI、依赖扫描、CMake 集成与编译器差异）

## 概述

Modules 的正确使用不仅是语言层面的问题，更是构建系统层面的问题。从 `import` 语句到最终链接，中间涉及 BMI（Binary Module Interface）文件的生成与消费、依赖扫描器的拓扑排序、CMake/Ninja 的模块感知调度、编译器间的格式差异。本文提供从原理到可运行示例的完整构建证据，覆盖 BMI 格式细节、CMake 最小项目、Header Unit 与 Named Module 的区别、Global/Private Module Fragment、模板实例化行为、ABI vs BMI 稳定性、三大编译器的实践差异、Partition 组织模式以及迁移策略。

## BMI（Binary Module Interface）文件格式

### 什么是 BMI

BMI 是编译器在编译模块接口单元时生成的**中间产物**，包含了模块导出的所有声明、类型信息和内联定义的编译后表示。消费者（`import` 该模块的翻译单元）读取 BMI 而非源代码。

```
构建流程：

math.cppm → [编译器] → math.gcm / math.ifc / module.pcm
               ↓                      ↓
           math.o (实现)         BMI 缓存文件
                                     ↓
main.cpp → [import math] → 读取 BMI → main.o
                                              ↓
                              link → math.o + main.o → a.out
```

### 格式详情（编译器特定）

BMI 格式**不是标准规定的**，每个编译器有自己的实现，且**不跨版本兼容**。

**MSVC (.ifc)**：
```
IFC (Interface Container) 格式：
- 二进制序列化格式，包含：
  - 模块名称和分区信息
  - 导出声明的 AST 节点
  - 类型信息（类、枚举、typedef）
  - 内联函数体和模板定义
  - 依赖的其他模块引用
- MSVC IFC 格式文档：部分公开，位于 ifc-spec 项目
- 文件大小：通常 10KB-10MB 取决于模块复杂度
```

**Clang (.pcm)**：
```
PCM (Precompiled Module) 格式：
- 基于 LLVM 的位码（bitcode）格式
- 包含序列化的 AST 和类型表
- 存储在 -fprebuilt-module-path 指定的目录
- 使用 .pcm 扩展名，但内部格式是 LLVM bitcode
- Clang 模块缓存路径：-fmodules-cache-path=<dir>
```

**GCC (.gcm)**：
```
GCM (GCC Compiled Module) 格式：
- GCC 私有二进制格式
- 以模块名的反向路径存储：
  C++ → c++/C.gcm
  math_utils → math_utils.gcm
- 默认存储在 -fmodules-ts 的输出目录
- GCC 14+ 基本功能可用，但仍有稳定性问题
```

### BMI 的不可移植性

```
跨编译器兼容性：

MSVC .ifc  ←✗→  Clang .pcm   ←✗→  GCC .gcm
  ↑                 ↑                 ↑
  MSVC only         Clang only        GCC only

甚至同编译器不同版本也不兼容：
MSVC 19.38 .ifc  ←✗→  MSVC 19.39 .ifc
```

**实践约束**：所有开发者必须使用相同编译器的相同版本。CI/CD 中需要锁定编译器版本。

## 依赖扫描：CMake/Ninja 模块依赖扫描

### 为什么需要依赖扫描

传统头文件的依赖关系通过 `#include` 直接可见（文本替换）。模块的 `import` 依赖关系只有**解析源代码后**才能确定——编译器必须知道 `import math;` 中 `math` 对应哪个源文件。

```
传统构建：依赖关系在源代码中可见
main.cpp
  ├── #include "math.h"     ← 预处理器直接看到
  ├── #include "utils.h"
  └── ...

Modules 构建：依赖关系需要扫描
main.cpp
  ├── import math;           ← 需要知道 math 在哪
  ├── import std;            ← 需要知道 std 在哪
  └── ...
```

### P1689R5：标准依赖格式

P1689R5 定义了模块依赖信息的标准 JSON 格式：

```json
{
  "rules": [
    {
      "primary-output": "math.pcm",
      "provides": [
        { "logical-name": "math", "is-interface": true }
      ],
      "requires": [
        { "logical-name": "std" }
      ]
    },
    {
      "primary-output": "main.o",
      "requires": [
        { "logical-name": "math" },
        { "logical-name": "std" }
      ]
    }
  ]
}
```

### 扫描流程

```
CMake 模块构建流程（3 阶段）：

阶段 1: 依赖扫描 (P1689 扫描器)
  ├── 扫描所有 .cppm / .cpp 文件
  ├── 提取 import 和 export module 声明
  ├── 生成依赖图 (JSON)
  └── 输出拓扑排序结果

阶段 2: 拓扑排序编译
  ├── 先编译无依赖的模块接口
  ├── 再编译依赖已编译模块的接口
  ├── 最后编译实现单元和普通源文件
  └── 编译顺序保证：被依赖方先于依赖方

阶段 3: 链接
  └── 所有 .o 文件链接为最终目标
```

## Header Unit vs Named Module

```cpp
// Header Unit：将传统头文件作为模块导入
import <vector>;       // Header Unit
import <iostream>;

// Named Module：标准的新模块形式
import std;            // Named Module（C++23 import std）
import math_utils;     // 用户自定义 Named Module
```

| 维度 | Header Unit | Named Module |
|------|-------------|-------------|
| 宏传播 | 否（已预处理，但头文件内宏不传播） | 否 |
| 符号隔离 | 完全隔离 | 完全隔离 |
| 头文件兼容 | 直接包装现有头文件 | 需要重写接口 |
| 构建开销 | 比 `#include` 快（BMI缓存） | 最优（精确导出） |
| 工具支持 | MSVC 较好，Clang/GCC 实验性 | 各编译器主力支持 |
| 适用场景 | 迁移过渡期 | 新项目首选 |

```cpp
// Header Unit 的等价理解
import <vector>;
// 等价于（概念上）：
// 1. 预处理 <vector>（展开所有宏）
// 2. 将预处理后的声明编译为 BMI
// 3. import 语句引用该 BMI
// 关键：宏不传播到 import 方——但头文件内部的宏仍正常工作
```

## Global Module Fragment：宏与遗留头文件边界

```cpp
module;                          // ← Global Module Fragment 开始

// 在此区域可以使用宏、#include 遗留头文件
#define USE_LEGACY_API 1
#include <cstdio>
#include <cstdint>
#include "legacy_header.h"      // 旧代码中的头文件

export module modern_api;        // ← module purview 开始

// 此处可以使用 Global Module Fragment 中引入的符号
// 但宏和头文件内容不暴露给 import 方

export void modern_print(const char* msg) {
    std::printf("[MODERN] %s\n", msg);  // cstdio 的 printf 可用
}

export struct Config {
    uint8_t version;             // cstdint 的 uint8_t 可用
    char name[64];
};
```

**关键规则**：
- Global Module Fragment 中的 `#include` 内容归属**全局模块**，不属于当前模块
- 宏定义在 Global Module Fragment 中正常工作，但不传播到 `import` 方
- Global Module Fragment 中不能出现 `export` 声明
- Global Module Fragment 必须在 `export module` 之前

**错误用法**：

```cpp
export module bad;

#include <string>   // 错误！#include 在 purview 中 → ODR 问题
                    // <string> 的声明被纳入 bad 模块
                    // 其他模块 import <string> 时产生冲突
```

## Private Module Fragment：接口与实现分离

```cpp
// widget.cppm — 完整的模块接口 + 实现
export module widget;

export class Widget {
public:
    Widget(int id);
    void process();
    int id() const;
private:
    int id_;
};

// Private Module Fragment：此后的声明对外不可见
module :private;

// 以下实现不会被 import 方看到
// 变更此处代码不需要重新编译 import 方（仅重新链接）

Widget::Widget(int id) : id_(id) {}

void Widget::process() {
    // 复杂实现细节
}

int Widget::id() const { return id_; }
```

**Private Module Fragment 的价值**：
- **接口/实现在同一文件**：适合小型模块
- **增量编译友好**：修改 `module :private` 之后的代码不需要重新编译依赖方
- **限制**：Private Module Fragment 中不能有模板定义的隐式实例化点

## 模板在模块中的行为

### 导出模板定义

```cpp
// math_utils.cppm
export module math_utils;

// 模板定义必须对 import 方可见
// 因为 import 方可能需要实例化
export template <typename T>
T square(T x) {
    return x * x;  // 定义必须导出（或在接口中可见）
}

// 显式实例化导出
export template class std::vector<int>;  // 导出 vector<int> 的实例化

// 隐式实例化：import 方实例化时需要完整定义
// 模块的 BMI 中包含模板定义，因此 import 方可以直接实例化
```

### 实例化控制

```cpp
// widget.cppm（接口）
export module widget;
export template <typename T>
class Container {
    T* data_;
    size_t size_;
public:
    Container(size_t n);
    ~Container();
    // 声明在接口中
};

// widget_impl.cpp（实现单元）
module widget;
// 显式实例化定义——编译器在此生成代码
template class Container<int>;
template class Container<double>;
// 其他类型需要隐式实例化，可能导致链接问题
```

## ABI vs BMI：模块不提供 ABI 稳定性

```
ABI 与 BMI 的关系：

ABI（Application Binary Interface）：
  - 对象文件的符号布局、调用约定、名称修饰
  - 跨编译单元、跨编译器版本
  - C ABI 是稳定的（extern "C"），C++ ABI 不稳定

BMI（Binary Module Interface）：
  - 编译器内部的序列化 AST
  - 编译器私有格式，不跨版本
  - 仅用于编译时，不进入链接

结论：Modules 不改善 ABI 稳定性
  - 修改模块接口 → 必须重新编译所有 import 方
  - BMI 不兼容 → 更换编译器版本后全部重新编译
  - Modules 的目标是编译速度，不是 ABI 稳定
```

```cpp
// 实践影响
// 1. 库分发仍需传统头文件 + 预编译库的模式
// 2. Modules 适合单一项目内部使用
// 3. 跨项目依赖暂时仍需 #include 或 Header Unit

// 分发给外部用户的库（暂不支持 Modules）：
// mylib.h  ← 公开头文件
// mylib.lib / libmylib.a  ← 预编译库
// 用户 #include "mylib.h" + 链接 mylib.lib

// 项目内部（强烈推荐 Modules）：
// math.cppm → import math;  ← 仅限同一编译器版本
```

## CMake 最小项目示例

### CMakeLists.txt（CMake 3.28+）

```cmake
cmake_minimum_required(VERSION 3.28)
project(modules_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 开启模块依赖扫描
set(CMAKE_CXX_MODULE_STD 1)           # 支持 import std（CMake 3.30+）

# 模块接口文件声明为 CXX_MODULE_SOURCES
add_library(math_utils)
target_sources(math_utils
    PUBLIC FILE_SET CXX_MODULES FILES
        src/math_utils.cppm
    PRIVATE
        src/math_utils_impl.cpp       # 实现单元（可选）
)

# 可执行文件
add_executable(app src/main.cpp)
target_link_libraries(app PRIVATE math_utils)
```

### 模块接口文件

```cpp
// src/math_utils.cppm
export module math_utils;

export namespace math {
    constexpr int add(int a, int b) { return a + b; }
    constexpr int mul(int a, int b) { return a * b; }

    template <typename T>
    constexpr T clamp(T val, T lo, T hi) {
        return (val < lo) ? lo : (hi < val) ? hi : val;
    }
}
```

### 使用模块

```cpp
// src/main.cpp
import math_utils;
import <iostream>;

int main() {
    std::cout << math::add(1, 2) << '\n';
    std::cout << math::clamp(42, 0, 100) << '\n';
}
```

### 构建命令

```bash
# CMake 配置（需要支持模块的编译器）
cmake -B build -G Ninja \
    -DCMAKE_CXX_COMPILER=clang++-18    # Clang 18+
    # 或 -DCMAKE_CXX_COMPILER=cl       # MSVC
    # 或 -DCMAKE_CXX_COMPILER=g++-14   # GCC 14+

# 构建
cmake --build build
```

## MSVC / GCC / Clang 实践差异

### 编译选项

```
MSVC (cl.exe):
  /std:c++20                    启用 C++20
  /interface                    标记为模块接口单元（.ixx / .cppm）
  /internalPartition            标记为模块内部 partition
  /ifcOutput <dir>              BMI 输出目录
  /reference <name>=<ifc>       手动指定 BMI 路径（不推荐）

Clang (clang++):
  -std=c++20 -fmodules          启用模块
  -fprebuilt-module-path=<dir>  指定 BMI 搜索路径
  -fmodules-cache-path=<dir>    模块缓存目录
  --precompile                  仅生成 BMI（不编译为 .o）
  -fmodule-file=<name>=<pcm>   手动指定单个模块 BMI

GCC (g++):
  -std=c++20 -fmodules-ts       启用模块（实验性）
  -fmodule-mapper=<file>        通过文件指定模块映射
  -fmodule-header               将头文件作为 Header Unit 编译
  -x c++-system-header <h>      编译系统头文件为 Header Unit
```

### 文件扩展名约定

```
MSVC:    .ixx（模块接口）、.cppm（部分支持）、.cpp（实现/普通）
Clang:   .cppm（模块接口）、.cpp（实现/普通）
GCC:     .cppm（模块接口）、.cpp（实现/普通）
通用推荐: .cppm（跨编译器兼容性较好）
```

### 编译器支持矩阵（2025）

```
特性                    MSVC 19.38+  Clang 18+  GCC 14+
───────────────────────────────────────────────────────
export module / import     ✅           ✅         ✅
Module partition           ✅           ✅         ⚠️ 部分
Global module fragment     ✅           ✅         ⚠️ 实验性
Private module fragment    ✅           ✅         ⚠️ 实验性
Header Unit                ✅           ⚠️ 部分   ⚠️ 实验性
import std                 ✅           ⚠️ 部分   ⚠️ 实验性
BMI 增量编译               ✅           ✅         ❌
CMake 模块支持             ✅           ✅         ⚠️ 有限
生产可用性                 ✅           ✅         ❌ 不推荐
```

## Module Partition 组织模式

### 模式一：按层分 partition

```cpp
// network:socket.cppm — 底层网络
export module network:socket;
export class Socket { /* ... */ };

// network:http.cppm — HTTP 层（依赖 socket）
export module network:http;
import :socket;
export class HttpClient { /* ... */ };

// network.cppm — 主接口（聚合 + 重新导出）
export module network;
export import :socket;
export import :http;
```

### 模式二：按功能域分 partition

```cpp
// app:ui.cppm
export module app:ui;
export void render_dashboard();

// app:backend.cppm
export module app:backend;
export void start_server(int port);

// app:config.cppm
export module app:config;
export struct AppConfig { int port; bool debug; };

// app.cppm
export module app;
export import :config;
export import :ui;
export import :backend;
```

### 模式三：内部 partition（不导出）

```cpp
// engine:detail_math.cppm — 内部数学工具
export module engine:detail_math;
// 此 partition 可以被 engine 的其他 partition import
// 但不会被外部直接 import（除非主模块 export import）

// engine:render.cppm
export module engine:render;
import :detail_math;  // 内部使用

// engine.cppm
export module engine;
export import :render;
// 注意：没有 export import :detail_math → 外部不可见
```

**Partition 命名惯例**：
- `:` 前是模块名，后是分区名
- 以 `detail_` 前缀标记内部实现 partition
- 主模块选择性 `export import` — 控制公开 API 表面积

## 从头文件迁移到 Modules 的策略

### 渐进式迁移路线

```
阶段 0: 评估（1-2 周）
  ├── 盘点头文件依赖图
  ├── 识别叶子节点（无内部依赖的工具库）
  └── 确认编译器版本支持

阶段 1: 叶子节点转换（2-4 周）
  ├── 将工具库（string_utils, math_utils）转为 module
  ├── 在原有 #include 旁边保留兼容头文件
  ├── 验证编译正确性和编译速度
  └── 切换到 module 的使用方逐步迁移

阶段 2: 桥接层（4-8 周）
  ├── 核心库转为 module
  ├── 为未迁移的头文件创建 Header Unit
  ├── 在模块中 import 已转换的模块
  └── 保留过渡期的 #include 兼容

阶段 3: 全面迁移（持续）
  ├── 逐层替换 #include 为 import
  ├── 移除兼容头文件
  ├── 优化模块边界（减少不必要的 export）
  └── 持续监控编译速度和正确性
```

### 迁移代码示例

```cpp
// 阶段 0-1：保留两种接口
// utils.h（兼容头文件）
#pragma once
import utils_module;    // 头文件内部 import 模块

// 新代码直接 import
// import utils_module;

// 阶段 2：在模块中桥接遗留代码
module;
#include <legacy_header.h>  // 遗留代码仍在 Global Module Fragment
export module new_module;

export using legacy::OldClass;  // 选择性导出遗留符号
export void modern_api();
```

### 迁移注意事项

```
1. 宏问题
   ├── 无法跨模块传递宏 → 用 constexpr / inline 函数替代
   ├── 断言宏（assert）在 Global Module Fragment 中正常工作
   └── 配置宏用 constexpr 变量替代

2. 头文件顺序依赖
   ├── Modules 消除了顺序依赖（每模块独立编译）
   ├── 但遗留代码中 if A.h depends on B.h 先 include 的问题
   └── 在 Global Module Fragment 中仍需注意

3. 模板显式实例化
   ├── 模块中显式实例化的行为与头文件不同
   ├── 实现单元中的实例化不自动导出
   └── 需要在接口中显式声明 export template class ...

4. ODR（One Definition Rule）
   ├── 模块边界自动提供 ODR 保护
   ├── 同一实体在不同模块中定义 → 编译错误
   └── 匿名 namespace 在模块中的行为需特别注意

5. 构建系统改造
   ├── CMake 3.28+ 原生支持
   ├── Ninja 1.11+ 支持模块依赖图
   ├── 旧构建系统（Makefile, Bazel）需要额外适配
   └── 持续集成环境必须锁定编译器版本
```

## 总结

```
Modules 构建全链路
─────────────────────────────────────────────────
源代码:   .cppm (接口) / .cpp (实现)
    ↓
扫描器:   P1689 依赖图 → JSON
    ↓
调度器:   拓扑排序 → 编译顺序
    ↓
编译器:   接口 → BMI (.ifc/.pcm/.gcm)
          实现 → .o
          消费者 → import BMI → .o
    ↓
链接器:   .o → 最终二进制
─────────────────────────────────────────────────

关键约束：
  BMI 格式编译器私有、不跨版本
  所有开发者使用相同编译器+版本
  CMake 3.28+ / Ninja 1.11+ 是最低要求
  大型项目渐进迁移，叶子节点先行
```
