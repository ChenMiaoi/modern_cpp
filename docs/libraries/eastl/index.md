# EASTL (EA) 源码级深度剖析

> 源码路径：`references/impl/EASTL/include/EASTL/`

EASTL 是 Electronic Arts 于 2007 年开源的 STL 替代实现。针对游戏运行时环境重新设计：确定性内存行为、零异常开销、显式分配器控制。

## 核心设计理念

EASTL 的核心论点：标准 STL 的设计假设是"通用计算"，而游戏引擎的需求是"可预测的实时计算"。

## 目录

| 章节 | 源码路径 | 内容 |
|------|---------|------|
| [分配器模型](/libraries/eastl/allocator) | `allocator.h` | 非模板实例化、双 allocate 重载、dummy_allocator、调试命名 |
| [fixed_* 容器](/libraries/eastl/fixed-containers) | `fixed_vector.h`, `fixed_string.h` | 内嵌缓冲区、溢出机制、aligned_buffer |
| [侵入式容器与哈希表](/libraries/eastl/intrusive-hashtable) | `intrusive_list.h`, `hashtable.h` | 侵入式设计、开放寻址哈希、红黑树 |
| [String、Span 与排序](/libraries/eastl/string-sort) | `string.h`, `span.h`, `sort.h` | 三级存储 string、Span 视图、基数排序 |

## 与标准库对比

| 维度 | EASTL | std (标准) |
|------|-------|-----------|
| 分配器 | **非模板、实例化传递** | 模板参数绑定 |
| 异常 | 完全不使用 | 依赖异常 |
| SSO | 与实现相关 | 15-22 字节 |
| fixed 容器 | **原生支持** | 无（需 std::pmr） |
| 侵入式容器 | **原生支持** | 无 |
| 游戏引擎 | **设计目标** | 非目标 |
| 通用性 | 游戏/嵌入式 | 通用 |
