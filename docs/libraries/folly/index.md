---
title: "Folly (Meta) 源码级深度剖析"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# Folly (Meta) 源码级深度剖析

> Folly 是 Meta（前 Facebook）的 C++ 基础库。源码位于 `references/impl/folly/folly/`。

Folly 的核心组件覆盖了字符串、哈希表、零拷贝缓冲区、异步框架和线程安全容器——每一个都比标准库的对应实现更深思熟虑。

## 目录

| 章节 | 源码路径 | 内容 |
|------|---------|------|
| [fbstring 三级存储](/libraries/folly/fbstring) | `FBString.h` | Small/Medium/Large 三级、COW 引用计数、页面跨越优化 |
| [F14 哈希表](/libraries/folly/f14) | `container/F14*.h` | Chunk-based 布局、SIMD tag 探测、与 SwissTable 对比 |
| [IOBuf 零拷贝缓冲区](/libraries/folly/iobuf) | `IOBuf.h` | 循环双向链表、引用计数、自定义释放回调 |
| [Future/Promise 异步框架](/libraries/folly/future-promise) | `futures/Future.h`, `Promise.h` | Core 共享状态、四状态机、SemiFuture/Future、Executor |
| [Synchronized 与工具](/libraries/folly/synchronized-tools) | `Synchronized.h`, `Function.h` | 类型安全锁守卫、move-only Function SBO |

## 与标准库对比总览

| 组件 | Folly | 标准库 | 核心差异 |
|------|-------|--------|---------|
| string | fbstring 三级 (Small/Medium/Large COW) | std::string 两级 (Small/Long) | SSO=23 vs 15/22 |
| hash_map | F14 (chunk + SIMD) | unordered_map（节点式链表） | F14 缓存更友好 |
| byte_buffer | IOBuf (引用计数 + 链式) | vector\<char\> (拷贝语义) | 零拷贝 |
| async | Future/Promise + Executor | std::future (阻塞) | 全 continuation |
| thread_safe | Synchronized\<T\> (RAII) | 手动 mutex + lock_guard | 编译期强制加锁 |
| callable | folly::Function (SBO, move-only) | std::function (SBO, copy-only) | 支持 move-only |
