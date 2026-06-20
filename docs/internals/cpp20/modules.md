---
title: "Modules 实现分析"
topic: internals
feature: modules
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "N/A"
source_llvm: "N/A"
---

# Modules 实现分析

> C++20 Modules 是一种新的代码组织方式，替代传统的头文件包含。本文分析 Modules 的编译器实现。

---

## 一、核心概念

### 1.1 什么是 Modules

Modules 提供模块化的代码组织：

```cpp
// 模块接口文件
export module math;

export int add(int a, int b) {
    return a + b;
}

// 模块使用
import math;
int result = add(1, 2);
```

### 1.2 Modules vs Headers

```
Modules vs Headers：

Headers：
  · 每次包含都重新编译
  · 宏污染
  · 顺序依赖

Modules：
  · 只编译一次
  · 无宏污染
  · 无顺序依赖
```

---

## 二、编译流程

### 2.1 BMI（Binary Module Interface）（源码分析）

```
Modules 编译流程：

1. 编译模块接口（.cppm / .ixx）
   · 解析模块声明
   · 生成 BMI（.gcm / .pcm）
   · BMI 包含：
     · 导出的声明
     · 类型信息
     · 模板实例化信息

2. 编译模块实现（.cpp）
   · 导入 BMI
   · 编译实现代码
   · 生成模块对象文件

3. 链接
   · 链接所有对象文件
   · 解析模块依赖
   · 生成可执行文件
```

### 2.2 GCC 的模块实现（源码分析）

```cpp
// GCC 的模块支持

// 模块接口文件（math.cppm）
export module math;

// 导出函数
export int add(int a, int b) {
    return a + b;
}

// 导出类
export class Calculator {
public:
    int multiply(int a, int b) { return a * b; }
};

// 私有实现
namespace {
    int helper(int x) { return x * 2; }
}
```

### 2.3 模块分区（源码分析）

```cpp
// 模块分区接口（math.parts.cppm）
export module math:parts;

export int add(int a, int b) {
    return a + b;
}

// 模块接口（math.cppm）
export module math;

// 导入分区
import :parts;

// 重新导出
export using math::add;

// 添加额外功能
export int multiply(int a, int b) {
    return a * b;
}
```

### 2.4 头单元（源码分析）

```cpp
// 头单元：将头文件作为模块导入
import <vector>;
import <string>;
import <iostream>;

// 等价于：
// #include <vector>
// #include <string>
// #include <iostream>
// 但更快，因为只编译一次
```

---

## 三、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC                  │ Clang/LLVM           │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ 模块接口               │ 支持                 │ 支持                 │
│ 模块分区               │ 支持                 │ 支持                 │
│ 头单元                 │ 支持                 │ 支持                 │
│ BMI 格式               │ .gcm                 │ .pcm                 │
│ 增量编译               │ 支持                 │ 支持                 │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 延伸阅读

- [模板实例化](/internals/templates/instantiation) — 模板的编译过程
- [GCC 编译器内部](/internals/compiler/gcc-internals) — GCC 的编译流程
- [Clang/LLVM 编译器内部](/internals/compiler/clang-internals) — LLVM 的编译流程
