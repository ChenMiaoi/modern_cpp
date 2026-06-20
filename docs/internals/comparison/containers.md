---
title: "容器实现对比"
topic: internals
feature: comparison-containers
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/bits/"
source_llvm: "references/impl/llvm-project/libcxx/include/"
---

# 容器实现对比

> libstdc++（GCC）和 libc++（LLVM）是两大主流 C++ 标准库实现。本文对比两者在容器实现上的差异。

---

## 一、vector 对比

### 1.1 GCC (libstdc++) 的 vector（源码分析）

```
libstdc++ vector：

内部结构：
  · 三指针布局：_M_start, _M_finish, _M_end_of_storage
  · EBO 压缩 allocator
  · 扩容因子：2x

关键源码：
  · stl_vector.h：_Vector_impl_data 定义
  · vector.tcc：_M_realloc_insert 实现

性能特点：
  · push_back: O(1) 摊销
  · insert: O(n)
  · 随机访问: O(1)
```

### 1.2 LLVM (libc++) 的 vector（源码分析）

```
libc++ vector：

内部结构：
  · compressed_pair 存储
  · 扩容因子：2x
  · 支持 trivially_relocatable

关键源码：
  · __vector/vector.h：vector 类定义
  · __split_buffer：内部缓冲区管理

性能特点：
  · 与 libstdc++ 类似
  · 但更小的对象大小（某些情况）
```

---

## 二、string 对比

```
string 对比：

libstdc++：
  · sizeof(string) = 32 字节
  · SSO 缓冲区 = 15 字节
  · 支持旧 ABI（COW）

libc++：
  · sizeof(string) = 24 字节
  · SSO 缓冲区 = 22 字节
  · 只支持新 ABI
```

---

## 三、map/set 对比

```
map/set 对比：

libstdc++：
  · _Rb_tree 实现
  · aligned_membuf 存储值
  · 支持 node_handle

libc++：
  · __tree 实现
  · compressed_pair 存储
  · 支持 node_handle
```

---

## 四、unordered_map 对比

```
unordered_map 对比：

libstdc++：
  · _Hashtable 实现
  · 质数桶数量
  · 支持 hash_code 缓存

libc++：
  · __hash_table 实现
  · 2 的幂次桶数量
  · 支持 hash_code 缓存
```

---

## 延伸阅读

- [std::vector 实现](/internals/containers/vector) — vector 的详细实现
- [std::string 实现](/internals/containers/string) — string 的详细实现
- [std::map/set 实现](/internals/containers/map-set) — map/set 的详细实现
