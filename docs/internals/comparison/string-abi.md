---
title: "字符串 ABI 对比"
topic: internals
feature: comparison-string-abi
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/bits/basic_string.h"
source_llvm: "references/impl/llvm-project/libcxx/include/string"
---

# 字符串 ABI 对比

> libstdc++ 和 libc++ 的字符串 ABI 有显著差异，影响跨库兼容性。本文对比两者的 ABI 策略。

---

## 一、ABI 差异

### 1.1 libstdc++ 双 ABI（源码分析）

```
libstdc++ 双 ABI：

旧 ABI（COW）：
  · -D_GLIBCXX_USE_CXX11_ABI=0
  · 使用引用计数的写时复制
  · sizeof(string) = 32 字节
  · 线程不安全

新 ABI（SSO）：
  · -D_GLIBCXX_USE_CXX11_ABI=1（默认）
  · 使用小字符串优化
  · sizeof(string) = 32 字节
  · 线程安全

命名空间区分：
  · 旧：std::basic_string
  · 新：std::__cxx11::basic_string
```

### 1.2 libc++ 内联命名空间（源码分析）

```
libc++ 内联命名空间：

版本 1（默认）：
  · _LIBCPP_ABI_VERSION=1
  · inline namespace __1
  · sizeof(string) = 24 字节

版本 2（实验性）：
  · _LIBCPP_ABI_VERSION=2
  · inline namespace __2
  · 不同的内部布局

命名空间区分：
  · 版本 1：std::__1::basic_string
  · 版本 2：std::__2::basic_string
```

---

## 二、内存布局对比

```
内存布局对比：

libstdc++ (新 ABI)：
  · sizeof(string) = 32 字节
  · SSO 缓冲区 = 15 字节
  · 三指针布局

libc++：
  · sizeof(string) = 24 字节
  · SSO 缓冲区 = 22 字节
  · compressed_pair 布局
```

---

## 三、兼容性

```
兼容性：

libstdc++ 旧 ABI：
  · -D_GLIBCXX_USE_CXX11_ABI=0
  · 与旧版本兼容

libstdc++ 新 ABI：
  · -D_GLIBCXX_USE_CXX11_ABI=1（默认）
  · 与 libc++ 不兼容

libc++：
  · _LIBCPP_ABI_VERSION=1（默认）
  · 与 libstdc++ 不兼容
```

---

## 延伸阅读

- [std::string 实现](/internals/containers/string) — string 的详细实现
- [C++ ABI 深度解析](/topics/abi) — ABI 的详细解释
