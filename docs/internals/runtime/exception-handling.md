---
title: "异常处理 ABI 实现分析"
topic: internals
feature: exception-handling
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "N/A"
source_llvm: "N/A"
---

# 异常处理 ABI 实现分析

> C++ 异常处理基于栈展开机制，本文分析 GCC 和 LLVM 的异常处理 ABI 实现。

---

## 一、核心概念

### 1.1 异常处理流程

```
异常处理流程：

1. throw 表达式
   · 构造异常对象
   · 调用 __cxa_throw

2. 栈展开
   · 查找匹配的 catch 块
   · 调用局部对象的析构函数
   · 回退到 catch 块

3. catch 块
   · 执行 catch 代码
   · 异常对象被销毁
```

---

## 二、核心数据结构

### 2.1 .eh_frame

```
.eh_frame 结构：

┌─────────────────────────────────────┐
│ CIE（Common Information Entry）     │
│   · 版本信息                        │
│   · 指针编码                        │
│   · 返回地址寄存器                  │
├─────────────────────────────────────┤
│ FDE（Frame Description Entry）      │
│   · 函数起始地址                    │
│   · 函数大小                        │
│   · 展开指令                        │
│   · LSDA 指针                       │
└─────────────────────────────────────┘
```

### 2.2 LSDA

```
LSDA（Language Specific Data Area）：

┌─────────────────────────────────────┐
│ LPStart（landing pad 基地址）        │
├─────────────────────────────────────┤
│ TTBase（typeinfo 表）               │
├─────────────────────────────────────┤
│ Call Site Table                      │
│   · [start, length, landing_pad]    │
├─────────────────────────────────────┤
│ Action Table                         │
│   · [type_filter, next_action]      │
└─────────────────────────────────────┘
```

---

## 三、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC                  │ Clang/LLVM           │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ 异常处理 ABI           │ Itanium EH ABI       │ Itanium EH ABI       │
│ .eh_frame              │ 支持                 │ 支持                 │
│ LSDA                   │ 支持                 │ 支持                 │
│ 栈展开                 │ _Unwind_Resume       │ _Unwind_Resume       │
│ 异常对象分配           │ __cxa_allocate_exception│ __cxa_allocate_exception│
│ -fno-exceptions        │ 支持                 │ 支持                 │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 四、最佳实践

```
异常处理使用指南：

1. 使用 RAII：
   · 自动资源管理
   · 异常安全保证

2. 考虑 -fno-exceptions：
   · 减小二进制体积
   · 嵌入式系统

3. 使用 noexcept：
   · 标记不抛异常的函数
   · 优化移动操作

4. 注意异常安全：
   · 基本保证
   · 强保证
```

---

## 延伸阅读

- [对象模型与内存布局](/internals/runtime/object-model) — 对象布局
- [虚函数表实现](/internals/runtime/vtable) — vtable 的实现
- [C++ ABI 深度解析](/topics/abi) — ABI 的详细解释
