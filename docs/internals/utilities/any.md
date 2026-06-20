---
title: "std::any 实现分析"
topic: internals
feature: any
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/std/any"
source_llvm: "references/impl/llvm-project/libcxx/include/any"
---

# std::any 实现分析

> `std::any` 是 C++17 引入的类型擦除容器，可以存储任意类型的值。本文基于 GCC 和 LLVM 的源码，分析 any 的内部实现。

---

## 一、核心概念

### 1.1 什么是 any

any 是一个类型安全的容器，可以存储任何可拷贝构造的类型：

```cpp
any a = 42;           // 存储 int
a = "hello";          // 改为存储 const char*
a = string("world");  // 改为存储 string

// 获取值
int i = any_cast<int>(a);  // 如果类型不匹配，抛 bad_any_cast
```

### 1.2 any vs variant vs optional

```
any vs variant vs optional：

any：
  · 运行时类型擦除
  · 任意类型
  · 可能堆分配

variant：
  · 编译期类型列表
  · 固定类型集合
  · 无堆分配

optional：
  · 编译期类型
  · 可有值或无值
  · 无堆分配
```

---

## 二、核心数据结构

### 2.1 小对象优化（SBO）

any 使用小对象优化避免堆分配：

```
any 的内存布局：

小对象（sizeof ≤ 阈值）：
┌─────────────────────────────────────┐
│ 函数指针表（管理/销毁/克隆）         │
├─────────────────────────────────────┤
│ 数据存储（栈上）                     │
└─────────────────────────────────────┘

大对象（sizeof > 阈值）：
┌─────────────────────────────────────┐
│ 函数指针表                           │
├─────────────────────────────────────┤
│ 指针（指向堆上数据）                 │
└─────────────────────────────────────┘
```

### 2.2 GCC (libstdc++) 的实现

```cpp
// GCC 使用小/大对象策略

// 小对象：直接存储在 any 对象中
struct _Manager_internal {
    template<typename _Tp>
    static void _S_manage(_Any_data& __dest, const _Any_data& __source,
                          _Manager_operation __op) {
        switch (__op) {
        case __clone:
            ::new (__dest._M_access()) _Tp(__source._M_access<_Tp>());
            break;
        case __destroy:
            __dest._M_access<_Tp>().~_Tp();
            break;
        }
    }
};

// 大对象：堆上分配
struct _Manager_external {
    template<typename _Tp>
    static void _S_manage(_Any_data& __dest, const _Any_data& __source,
                          _Manager_operation __op) {
        switch (__op) {
        case __clone:
            __dest._M_access<_Tp*>() = new _Tp(*__source._M_access<_Tp*>());
            break;
        case __destroy:
            delete __dest._M_access<_Tp*>();
            break;
        }
    }
};
```

---

## 三、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ 小对象阈值             │ sizeof(void*) × 3    │ sizeof(void*) × 3    │
│ 存储方式               │ 函数指针表           │ 函数指针表           │
│ 类型擦除               │ typeid + 函数指针    │ typeid + 函数指针    │
│ has_value              │ 支持                 │ 支持                 │
│ reset                  │ 支持                 │ 支持                 │
│ swap                   │ 支持                 │ 支持                 │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 四、最佳实践

```
any 使用指南：

1. 优先使用 variant：
   · 类型已知时用 variant
   · 类型未知时才用 any

2. 使用 any_cast 安全访问：
   try {
       int i = any_cast<int>(a);
   } catch (bad_any_cast& e) {
       // 处理类型不匹配
   }

3. 使用 any_cast 指针版本：
   if (int* p = any_cast<int>(&a)) {
       // 安全访问
   }
```

---

## 延伸阅读

- [std::variant 实现](/internals/utilities/variant) — 标签联合
- [std::optional 实现](/internals/utilities/optional) — 可选值
- [std::function 实现](/internals/utilities/function) — 类型擦除的可调用对象
