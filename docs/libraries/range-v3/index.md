---
title: "range-v3 源码级深度剖析"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# range-v3 源码级深度剖析

> range-v3 是 Eric Niebler 编写的 C++ 范围库，直接催生了 C++20 Ranges 标准。

## 目录

| 章节 | 源码路径 | 内容 |
|------|---------|------|
| [管道运算符机制](/libraries/range-v3/pipe-operator) | `__ranges/range_adaptor.h` | CRTP 基类、`__pipeable`、`__compose`、`__bind_back` |
| [View 实现](/libraries/range-v3/views) | `filter_view.h`, `transform_view.h` | 惰性求值、begin 缓存、迭代器适配 |
| [Actions 与 Sentinels](/libraries/range-v3/actions-sentinels) | `actions/`, sentinel 概念 | eager 操作、哨兵迭代器 |
