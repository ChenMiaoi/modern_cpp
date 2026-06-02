---
title: "libc++ (LLVM) 源码级深度剖析"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# libc++ (LLVM) 源码级深度剖析

> 源码路径：`references/impl/llvm-project/libcxx/include/`

LLVM 的 C++ 标准库实现，以最紧凑的内存布局和最新 C++ 标准支持著称。核心设计理念：**极简内存布局 + trivially relocatable memcpy**。

## 目录

| 章节 | 内容 |
|------|------|
| [vector 与 string](/libraries/llvm/vector-string) | 三指针 vector、24 字节 SSO string（22 字节内联）、split_buffer、memcpy relocate |
| [function 与 shared_ptr](/libraries/llvm/function-shared-ptr) | 24 字节 SBO function（虚函数表）、make_shared 单次分配 |
| [map/set 与 variant](/libraries/llvm/map-variant) | 红黑树 `__tree`、哨兵节点 `__end_node_`、variant visit 函数指针表 |
| [Ranges 与 unique_ptr](/libraries/llvm/ranges-unique-ptr) | 管道运算符 CRTP、filter_view 缓存、compressed_pair unique_ptr |

## 三实现综合对比

| 组件 | libc++ (LLVM) | libstdc++ (GCC) | MSVC STL |
|------|--------------|-----------------|----------|
| sizeof(string) | **24** | 32 | 32 |
| string SSO | **22** | 15 | 15 |
| vector 增长 | 2× | ~2× | 1.5× |
| vector relocate | **memcpy** | move+destroy | move+destroy |
| unique_ptr | 8B | 8B | 8B |
| trivial_abi | **支持** | 不支持 | 不支持 |
| function SBO | 24B (虚函数) | 24B (函数指针) | 不同 |
| unordered_map | 链式 | 节点式 _Hashtable | 链式 |

**选 libc++**：macOS/iOS/Android、最紧凑内存布局、trivially relocatable memcpy、最新 C++ 标准。
