---
title: "libstdc++ (GCC) 源码级深度剖析"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# libstdc++ (GCC) 源码级深度剖析

> 源码路径：`references/impl/gcc/libstdc++-v3/include/`

GCC 的 C++ 标准库实现，Linux 生态的事实标准。核心设计理念：**ABI 稳定性优先于激进优化**。

## 目录

| 章节 | 内容 |
|------|------|
| [string ABI 迁移](/libraries/libstdcxx/string-abi) | COW → SSO 迁移、双 ABI 共存、`__cxx11` 命名空间 |
| [vector 与 _Hashtable](/libraries/libstdcxx/vector-hashtable) | vector 增长策略、unordered_map 节点式 _Hashtable |
| [shared_ptr / RB-tree / function](/libraries/libstdcxx/shared-ptr-tree-function) | 控制块、`_Rb_tree` 哨兵节点、函数指针 SBO |

## 三实现综合对比

| 组件 | libstdc++ (GCC) | libc++ (LLVM) | MSVC STL |
|------|----------------|--------------|----------|
| sizeof(string) | 32 | 24 | 32 |
| string SSO | 15 | 22 | 15 |
| string COW | 有（ABI v1） | 无 | 无 |
| vector relocate | move+destroy | **memcpy** | move+destroy |
| function 多态 | **函数指针** | 虚函数 | 函数指针 |
| unordered_map | **节点式 _Hashtable** | 链式 | 链式 |
| ABI 稳定性 | **最强**（abi_tag） | 版本化 | 强 |

**选 libstdc++**：Linux 服务器、最强 ABI 兼容、保守稳定的实现策略、HPC。
