---
title: "RTTI 与 dynamic_cast 实现分析"
topic: internals
feature: rtti
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "N/A"
source_llvm: "N/A"
---

# RTTI 与 dynamic_cast 实现分析

> RTTI（Run-Time Type Information）提供运行时类型信息，dynamic_cast 用于安全的向下转换。本文分析 GCC 和 LLVM 的 RTTI 实现。

---

## 一、核心概念

### 1.1 什么是 RTTI

RTTI 提供运行时类型信息：

```cpp
// typeid 获取类型信息
Base* p = new Derived();
const type_info& ti = typeid(*p);
cout << ti.name() << endl;  // 输出类型名

// dynamic_cast 安全转换
Derived* d = dynamic_cast<Derived*>(p);
if (d) {
    // 转换成功
}
```

---

## 二、核心数据结构

### 2.1 type_info

```
type_info 的结构：

┌─────────────────────────────────────┐
│ vptr（type_info 的虚函数表）         │
├─────────────────────────────────────┤
│ __typeinfo_name（类型名）            │
└─────────────────────────────────────┘

存储位置：
  · .rodata 段（只读数据）
  · 每个类型一个 type_info 对象
```

### 2.2 dynamic_cast 的实现

```
dynamic_cast 的实现：

1. 获取源类型的 type_info
2. 获取目标类型的 type_info
3. 比较 type_info 是否匹配
4. 计算指针偏移量（多重继承时）
5. 返回调整后的指针

性能：
  · 同类型：O(1)
  · 单继承：O(depth)
  · 交叉转换：O(N)
```

---

## 三、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC                  │ Clang/LLVM           │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ type_info              │ 支持                 │ 支持                 │
│ typeid                 │ 支持                 │ 支持                 │
│ dynamic_cast           │ 支持                 │ 支持                 │
│ -fno-rtti              │ 支持                 │ 支持                 │
│ type_info 名称         │ mangled name         │ mangled name         │
│ 比较运算符             │ ==                   │ ==                   │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 四、最佳实践

```
RTTI 使用指南：

1. 优先使用虚函数：
   · 比 dynamic_cast 更高效
   · 更面向对象

2. 谨慎使用 dynamic_cast：
   · 有运行时开销
   · 可能抛异常

3. 使用 -fno-rtti：
   · 减小二进制体积
   · 使用枚举 ID 替代

4. 使用 typeid 获取类型名：
   · 调试时有用
   · 日志记录
```

---

## 延伸阅读

- [虚函数表实现](/internals/runtime/vtable) — vtable 的实现
- [对象模型与内存布局](/internals/runtime/object-model) — 对象布局
- [C++ ABI 深度解析](/topics/abi) — ABI 的详细解释
