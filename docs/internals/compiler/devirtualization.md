---
title: "去虚拟化实现分析"
topic: internals
feature: devirtualization
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "N/A"
source_llvm: "N/A"
---

# 去虚拟化实现分析

> 去虚拟化是编译器优化技术，将虚函数调用替换为直接调用。本文分析 GCC 和 LLVM 的去虚拟化实现。

---

## 一、核心概念

### 1.1 什么是去虚拟化

去虚拟化将虚函数调用转换为直接调用：

```cpp
// 去虚拟化前
class Base { virtual void f(); };
class Derived : public Base { void f() override; };

void call(Base* p) {
    p->f();  // 虚函数调用
}

// 去虚拟化后（如果编译器知道 p 的实际类型）
Derived* d = static_cast<Derived*>(p);
d->f();  // 直接调用
```

### 1.2 GCC 的去虚拟化（源码分析）

```
GCC 去虚拟化：

1. 类型传播：
   · 跟踪指针的实际类型
   · 在已知类型时直接调用

2. LTO 去虚拟化：
   · 链接时优化
   · 跨翻译单元分析
   · 收集所有虚函数覆盖关系
   · 唯一实现时直接调用

3. final 类优化：
   · 类或函数标记为 final
   · 编译器知道没有子类
   · 可以直接调用
```

### 1.3 LLVM 的去虚拟化（源码分析）

```
LLVM 去虚拟化：

1. -fstrict-vtable-pointers：
   · 标注 vptr 不变性
   · 在对象构造完成后到析构开始前，vptr 不变
   · LLVM 插入 llvm.assume 来标注这一点

2. GlobalDCE pass：
   · 收集所有虚函数覆盖关系
   · 若某个虚函数只有一个可能的实现 → 替换为直接调用
   · 结合 LTO 效果最佳

3. 类型注解：
   · __attribute__((annotate("vtable_visibility", "all")))
   · 告诉编译器已知全部子类
   · 可以进行更激进的优化
```

---

## 四、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC                  │ Clang/LLVM           │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ 类型传播               │ 支持                 │ 支持                 │
│ LTO 去虚拟化           │ 支持                 │ 支持                 │
│ final 优化             │ 支持                 │ 支持                 │
│ -fstrict-vtable-pointers│ 不支持              │ 支持                 │
│ GlobalDCE              │ 类似实现             │ 支持                 │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 延伸阅读

- [虚函数表实现](/internals/runtime/vtable) — vtable 的实现
- [编译器优化管线](/internals/compiler/optimization-pipeline) — 优化流程
- [GCC 编译器内部](/internals/compiler/gcc-internals) — GCC 的实现
