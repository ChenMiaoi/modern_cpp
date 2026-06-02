---
title: "libstdc++ (GCC) Source-Level Deep Analysis"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# libstdc++ (GCC) Source-Level Deep Analysis

> Source path: `references/impl/gcc/libstdc++-v3/include/`

GCC's C++ standard library implementation, the de facto standard in the Linux ecosystem. Core design philosophy: **ABI stability takes priority over aggressive optimization**.

## Table of Contents

| Chapter | Content |
|---------|---------|
| [string ABI Migration](/libraries/libstdcxx/string-abi) | COW → SSO migration, dual ABI coexistence, `__cxx11` namespace |
| [vector and _Hashtable](/libraries/libstdcxx/vector-hashtable) | vector growth strategy, unordered_map node-based _Hashtable |
| [shared_ptr / RB-tree / function](/libraries/libstdcxx/shared-ptr-tree-function) | Control block, `_Rb_tree` sentinel node, function pointer SBO |

## Three-Implementation Comparison

| Component | libstdc++ (GCC) | libc++ (LLVM) | MSVC STL |
|-----------|----------------|--------------|----------|
| sizeof(string) | 32 | 24 | 32 |
| string SSO | 15 | 22 | 15 |
| string COW | yes (ABI v1) | no | no |
| vector relocate | move+destroy | **memcpy** | move+destroy |
| function polymorphism | **function pointer** | vtable | function pointer |
| unordered_map | **node-based _Hashtable** | chaining | chaining |
| ABI stability | **strongest** (abi_tag) | versioned | strong |

**Choose libstdc++ for**: Linux servers, strongest ABI compatibility, conservative and stable implementation strategy, HPC.
