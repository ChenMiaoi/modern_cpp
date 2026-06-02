---
title: "C++03"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# C++03

C++03（ISO/IEC 14882:2003）是 C++98 的一次缺陷修复版（bug-fix release），没有引入新的语言特性，但修复了标准文本中的若干缺陷报告（Defect Reports）。

## 定位

C++03 被视为 C++98 的"修订版"，两者之间的差异对日常编程几乎不可见。真正的下一次重大变革要等到 C++11（原计划叫 C++0x）。

## 主要修正

- **值初始化（Value Initialization）**：明确了 `T()` 语法的行为，区分了默认初始化、值初始化和零初始化
- **`std::string` 与 `std::allocator` 的若干修正**
- **模板两阶段名称查找**的规则细化
- **异常规范（exception specifications）**的语义澄清

## 为什么重要

虽然 C++03 本身没有新特性，但理解它的修正确保你对以下问题有正确的认知：

- 为什么 `new T()` 和 `new T` 在内置类型上行为不同
- 模板实例化时名称查找的两阶段规则
- `auto_ptr` 在 C++03 语义下的精确行为

## 延伸阅读

- [变更与修正](/standards/cpp03/changes)
