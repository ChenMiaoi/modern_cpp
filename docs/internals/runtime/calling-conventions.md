---
title: "调用约定实现分析"
topic: internals
feature: calling-conventions
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "N/A"
source_llvm: "N/A"
---

# 调用约定实现分析

> 调用约定定义了函数参数传递和返回值的规则。本文分析 GCC 和 LLVM 的调用约定实现。

---

## 一、核心概念

### 1.1 什么是调用约定

调用约定规定了函数调用时的参数传递方式：

```
调用约定的内容：

1. 参数传递：
   · 哪些参数使用寄存器
   · 哪些参数使用栈
   · 参数顺序

2. 返回值：
   · 返回值如何传递
   · 大对象如何返回

3. 栈帧：
   · 调用者还是被调用者清理栈
   · 返回地址保存位置
```

---

## 二、主要调用约定

### 2.1 SysV AMD64

```
SysV AMD64（Linux/macOS）：

整数参数：rdi, rsi, rdx, rcx, r8, r9
浮点参数：xmm0-xmm7
返回值：rax（整数），xmm0（浮点）
调用者保存：rax, rcx, rdx, rsi, rdi, r8, r9, r10, r11
被调用者保存：rbx, rbp, r12-r15
```

### 2.2 MSVC x64

```
MSVC x64（Windows）：

整数参数：rcx, rdx, r8, r9
浮点参数：xmm0, xmm1, xmm2, xmm3
返回值：rax（整数），xmm0（浮点）
Shadow space：32 字节
调用者保存：rax, rcx, rdx, r8, r9, r10, r11
被调用者保存：rbx, rbp, rdi, rsi, r12-r15
```

---

## 三、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC                  │ Clang/LLVM           │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ SysV AMD64             │ 支持                 │ 支持                 │
│ MSVC x64               │ Windows 支持         │ 支持                 │
│ 调用约定属性           │ __attribute__((cdecl))│ __attribute__((cdecl))│
│ 结构体传递             │ 支持                 │ 支持                 │
│ 隐藏参数               │ 支持                 │ 支持                 │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 四、最佳实践

```
调用约定使用指南：

1. 使用默认调用约定：
   · 编译器会优化
   · 不需要手动指定

2. 注意 ABI 兼容性：
   · 跨编译器调用需要相同约定
   · 使用 extern "C"

3. 使用属性控制：
   · __attribute__((cdecl))
   · __attribute__((stdcall))

4. 注意结构体传递：
   · 大结构体通过指针传递
   · 小结构体通过寄存器传递
```

---

## 延伸阅读

- [C++ ABI 深度解析](/topics/abi) — ABI 的详细解释
- [GCC 编译器内部](/internals/compiler/gcc-internals) — GCC 的实现
- [Clang/LLVM 编译器内部](/internals/compiler/clang-internals) — LLVM 的实现
