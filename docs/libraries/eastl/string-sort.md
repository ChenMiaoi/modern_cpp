---
title: EASTL String、Span 与排序
topic: libraries
feature: eastl-string-sort
standard: N/A
status_checked_at: 2026-06-02
exercises: []
solutions: []
---
# EASTL String、Span 与排序

> 源码路径：`references/impl/EASTL/include/EASTL/string.h`, `span.h`, `sort.h`

## eastl::string

EASTL 的 `basic_string` 与标准 `std::string` 类似但有关键差异：

- 无异常——`at()` 返回错误码而非抛出 `out_of_range`
- 分配器参数是实例（非模板参数）
- 内建调试命名（`set_name()`/`get_name()`）
- SSO 容量与实现相关

## eastl::span

EASTL 的 `span<T>` 提供连续内存的非拥有视图，与 `std::span` 类似。

## eastl::sort

EASTL 的排序算法针对游戏场景优化：

- 对小数组（≤ 20 元素）使用插入排序
- 对已排序数据检测并利用现有顺序
- 支持用户自定义比较器
- 无异常——比较器抛异常时行为未定义
