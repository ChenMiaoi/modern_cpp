---
title: "专题"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# 专题

本部分从跨版本的视角梳理 C++ 的核心知识领域。每个专题整合多个标准版本中相关特性的演进脉络。

## 专题列表

| 专题 | 说明 |
|------|------|
| [C++ 术语黑话全书](/topics/cpp-jargon/) | 120+ 个专业术语系统性讲解：值类别、重载决议、SFINAE、类型擦除、异常安全、UB 等 |
| [内存模型与并发](/topics/memory-model) | 从 C++11 的内存模型到 C++26 的 Senders/Receivers |
| [模板元编程](/topics/template-metaprogramming) | 从 SFINAE 到 Concepts，模板技术的进化 |
| [RAII 与资源管理](/topics/raii) | 从构造/析构到智能指针到 Coroutine scope guard |
| [编译期计算](/topics/compile-time-computation) | `constexpr` → `consteval` → `constinit` → 反射 |
| [编译器优化](/topics/compiler-optimizations) | 完整优化管线：内联、SROA、循环向量化、LTO、PGO |
| [ABI 深度解析](/topics/abi) | 名称修饰、vtable 布局、异常 ABI、调用约定、符号可见性、ABI 版本化策略 |
| [C++ 设计模式](/topics/design-patterns) | 现代 C++ 实现经典设计模式 |
| [值类别深入解析](/topics/value-categories-deep-dive) | 从 C 到 C++17：五个值类别、实体化、移动语义、完美转发、拷贝消除 |
| [性能优化](/topics/performance) | 移动语义、小对象优化、缓存友好设计 |
| [对象生命周期](/topics/lifetime) | 存储期、子对象、悬垂指针引用、隐式对象创建、constexpr 生命周期 |
| [工具链与生态](/topics/toolchain) | 编译器、构建系统、包管理器、sanitizers |

## 阅读建议

如果你是 **C++ 初学者**：
1. 先通读 [C++11](/standards/cpp11/) 的所有特性
2. 然后按版本顺序浏览 C++14 → C++17 → C++20
3. 再挑选感兴趣的专题深入

如果你是有经验的 **C++ 开发者**：
1. 从你当前使用的版本开始，快速了解后续版本的新特性
2. 重点阅读专题部分，构建跨版本的知识体系
3. 关注 C++26/29 的前沿动态
