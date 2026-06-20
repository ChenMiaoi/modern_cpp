---
title: "智能指针对比"
topic: internals
feature: comparison-smart-ptrs
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/bits/shared_ptr_base.h"
source_llvm: "references/impl/llvm-project/libcxx/include/__memory/shared_ptr.h"
---

# 智能指针对比

> libstdc++ 和 libc++ 在智能指针实现上有显著差异。本文对比两者的实现策略。

---

## 一、unique_ptr 对比

### 1.1 GCC (libstdc++) 的 unique_ptr（源码分析）

```
libstdc++ unique_ptr：

内部结构：
  · 使用 tuple<pointer, deleter> 存储
  · EBO 优化空删除器
  · 移动语义控制

关键源码：
  · unique_ptr.h：__uniq_ptr_impl 定义
  · __uniq_ptr_data：移动构造/赋值控制

性能特点：
  · sizeof(unique_ptr<T>) = sizeof(T*)
  · 零开销抽象
```

### 1.2 LLVM (libc++) 的 unique_ptr（源码分析）

```
libc++ unique_ptr：

内部结构：
  · 使用 compressed_pair 存储
  · 支持 trivially_relocatable
  · 支持 array cookie 检测

关键源码：
  · __memory/unique_ptr.h：unique_ptr 类定义
  · compressed_pair：EBO 实现

性能特点：
  · 与 libstdc++ 类似
  · 但更现代的实现
```

---

## 二、shared_ptr 对比

```
shared_ptr 对比：

libstdc++：
  · 控制块使用虚函数
  · 支持双字原子优化
  · 锁策略模板参数

libc++：
  · 控制块使用虚函数
  · compressed_pair 存储
  · 支持 trivial_abi
```

---

## 三、weak_ptr 对比

```
weak_ptr 对比：

libstdc++：
  · 与 shared_ptr 共享控制块
  · 弱引用计数独立

libc++：
  · 与 shared_ptr 共享控制块
  · 弱引用计数独立
```

---

## 四、性能对比

```
性能对比：

make_shared：
  · 两者都支持
  · 单次分配优化

控制块大小：
  · libstdc++：约 32 字节
  · libc++：约 24 字节

线程安全：
  · 两者都保证
```

---

## 延伸阅读

- [std::unique_ptr 实现](/internals/memory/unique-ptr) — unique_ptr 的详细实现
- [std::shared_ptr 实现](/internals/memory/shared-ptr) — shared_ptr 的详细实现
- [std::weak_ptr 实现](/internals/memory/weak-ptr) — weak_ptr 的详细实现
