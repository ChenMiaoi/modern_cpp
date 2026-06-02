---
title: "libc++ (LLVM) Source-Level Deep Analysis"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# libc++ (LLVM) Source-Level Deep Analysis

> Source path: `references/impl/llvm-project/libcxx/include/`

LLVM's C++ standard library implementation, known for its most compact memory layout and latest C++ standard support. Core design philosophy: **minimal memory layout + trivially relocatable memcpy**.

## Table of Contents

| Chapter | Content |
|---------|---------|
| [vector and string](/libraries/llvm/vector-string) | Three-pointer vector, 24-byte SSO string (22 bytes inline), split_buffer, memcpy relocate |
| [function and shared_ptr](/libraries/llvm/function-shared-ptr) | 24-byte SBO function (vtable), make_shared single allocation |
| [map/set and variant](/libraries/llvm/map-variant) | Red-black tree `__tree`, sentinel node `__end_node_`, variant visit function pointer table |
| [Ranges and unique_ptr](/libraries/llvm/ranges-unique-ptr) | Pipe operator CRTP, filter_view cache, compressed_pair unique_ptr |

## Three-Implementation Comparison

| Component | libc++ (LLVM) | libstdc++ (GCC) | MSVC STL |
|-----------|--------------|-----------------|----------|
| sizeof(string) | **24** | 32 | 32 |
| string SSO | **22** | 15 | 15 |
| vector growth | 2× | ~2× | 1.5× |
| vector relocate | **memcpy** | move+destroy | move+destroy |
| unique_ptr | 8B | 8B | 8B |
| trivial_abi | **supported** | not supported | not supported |
| function SBO | 24B (vtable) | 24B (function pointer) | different |
| unordered_map | chaining | node-based _Hashtable | chaining |

**Choose libc++ for**: macOS/iOS/Android, most compact memory layout, trivially relocatable memcpy, latest C++ standard.
