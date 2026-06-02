---
title: "C++20 Modules（模块）"
topic: unknown
feature: modules
standard: N/A
status_checked_at: 2026-06-02
---
# C++20 Modules（模块）

## 概述

C++20 Modules 旨在取代传统头文件 + 源文件分离模式，通过显式导出接口和编译上下文隔离，解决 `#include` 的三大顽疾：**宏污染**、**顺序依赖**和**重复编译开销**。

传统 `#include` 是文本替换——同一头文件在每个翻译单元中被重新解析。Modules 将编译单元分为 **interface unit**（接口单元）和 **implementation unit**（实现单元），接口信息只编译一次，使用者通过 `import` 获取编译后的 BMI（Binary Module Interface）缓存。Modules 在 C++20 中定稿（P1103R3），截至 2025 年 MSVC 和 Clang 已具备生产级支持，GCC 仍有限制。

## 基本语法：export module 与 import

```cpp
// math_utils.cppm  (模块接口单元)
export module math_utils;          // 声明模块名

export int add(int a, int b) {     // export 标记导出的声明
    return a + b;
}

int internal_helper(int x) {       // 无 export：模块外部不可见
    return x * 2;
}
```

```cpp
// main.cpp
import math_utils;
import <iostream>;

int main() {
    std::cout << add(1, 2) << '\n';
    // internal_helper(5);  // 编译错误：未导出
}
```

`import` 与 `#include` 核心区别：

| 特性 | `#include` | `import` |
|------|-----------|----------|
| 机制 | 文本替换 | 引用编译后 BMI |
| 宏传播 | 是 | 否 |
| 编译开销 | O(N × M) | O(N)，仅解析 BMI |
| 依赖顺序 | 敏感 | 不敏感 |
| 符号隔离 | 无 | 模块边界自动隔离 |

## Module Partition（模块分区）

大型模块可拆分为 partition，独立编译但共享模块名：

```cpp
// math:arithmetic.cppm
export module math:arithmetic;
export int add(int a, int b) { return a + b; }
export int sub(int a, int b) { return a - b; }
```

```cpp
// math:geometry.cppm
export module math:geometry;
export double circle_area(double r) { return 3.14159265358979 * r * r; }
```

```cpp
// math.cppm — 主模块接口单元，聚合 partition
export module math;
export import :arithmetic;   // 重新导出 partition 的声明
export import :geometry;
```

使用者只需 `import math;`。外部无法直接 `import math:arithmetic;`，除非主模块显式 `export import`。

## Interface Unit vs Implementation Unit

```cpp
// network.cppm  (接口单元)
export module network;

export class Socket {
public:
    Socket();
    void connect(const char* host, int port);
    void send(const char* data, size_t len);
    ~Socket();
};
```

```cpp
// network.cpp  (实现单元)
module network;              // 无 export

Socket::Socket() { /* ... */ }
void Socket::connect(const char* host, int port) { /* ... */ }
void Socket::send(const char* data, size_t len) { /* ... */ }
Socket::~Socket() { /* ... */ }
```

实现单元用 `module <name>;`（无 `export`），其中的定义不应标记 `export`。

## Global Module Fragment 与 Module Purview

当模块需要调用遗留头文件时，使用 Global Module Fragment；从 `export module` 到翻译单元结尾是 **module purview**：

```cpp
module;                          // Global Module Fragment 开始

#include <cstdio>                // 归属全局模块，不会泄漏给 import 方
#include <cstdint>

export module file_io;           // module purview 开始

export int read_file(const char* path, char* buf, size_t max) {
    FILE* f = std::fopen(path, "rb");  // cstdio 的符号在此可用
    if (!f) return -1;
    auto n = std::fread(buf, 1, max, f);
    std::fclose(f);
    return static_cast<int>(n);
}

export struct Pixel {            // 只有 purview 中 export 的声明可被外部访问
    uint8_t r, g, b, a;         // uint8_t 来自 cstdint，但不属于本模块
};
```

`module;` 和 `export module` 之间是 Global Module Fragment，`#include` 的内容归属全局模块而非本模块。不要在 module purview 中直接 `#include`——应放入 Global Module Fragment 或改用 `import <header>;`。

## 编译速度与 BMI 缓存

```
传统模型:  header.h 在每个 .cpp 中全量解析 → O(N×M)
Modules:   header.cppm → 编译 → .bmi 缓存；各 .cpp 读取 BMI → O(N)
```

大型项目可获 **30%–70% 全量构建缩减**。增量构建更显著——改动实现单元不触发接口重新编译。

## 编译器支持现状（2025）

| 特性 | MSVC (19.38+) | Clang (18+) | GCC (14+) |
|------|:---:|:---:|:---:|
| `export module` / `import` | 完整 | 完整 | 完整 |
| Module partition | 完整 | 完整 | 部分 |
| Global module fragment | 完整 | 完整 | 实验性 |
| `import std;` | 完整 | 部分 | 实验性 |
| BMI 缓存 / 增量编译 | 完整 | 完整 | 有限 |

MSVC 支持最成熟；Clang 适合 Linux/macOS；GCC 的 Modules 仍不建议用于生产。

## 从头文件迁移到 Modules

**阶段 1——基础库先行**：将无外部依赖的工具库转为 module。

```cpp
export module utils.string;
export std::string trim(const std::string& s);
export std::vector<std::string> split(const std::string& s, char delim);
```

**阶段 2——桥接过渡**：在遗留头文件中 import 新模块。

```cpp
// compat_header.h
#pragma once
import math_utils;
inline int legacy_add(int a, int b) { return add(a, b); }
```

**阶段 3——自底向上替换**：从叶子节点开始，逐层将 `#include` 替换为 `import`，每次替换后验证。

迁移要点：宏不能跨模块传递，用 `constexpr` 替代；构建系统需感知 Modules（CMake 3.28+ `CXX_SCAN_FOR_MODULES`）；BMI 缓存目录应纳入 `.gitignore`。

## 最佳实践

1. 一个模块文件对应一个逻辑功能单元，而非机械地把每个 `.h` 转 `.cppm`。
2. 优先用 `constexpr` / `inline` 代替宏常量——宏无法跨模块传播。
3. 用 partition 组织大型模块，避免单文件过大影响增量编译。
4. 实现单元不 `export`，保持接口最小化。
5. 测试与生产使用相同编译器版本——BMI 格式不跨版本兼容。

## 常见陷阱

1. **Header Unit ≠ Module**：`import <vector>;` 是头文件单元，等同预处理后的 `#include`，不具备宏隔离能力。
2. **purview 中勿 `#include`**：头文件内容会纳入模块，引发 ODR 问题。应使用 Global Module Fragment 或 `import <header>;`。
3. **匿名 namespace 行为不同**：模块实现单元中匿名 namespace 符号仅在该翻译单元唯一，但跨编译器行为不一致。
4. **BMI 非 ABI 稳定**：更换编译器版本或修改接口后必须重新编译所有依赖方。
5. **`export import` vs `import`**：`export import :part;` 使 partition 对外部可见；普通 `import :part;` 仅模块内部可用。
