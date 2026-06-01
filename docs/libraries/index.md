# STL 与标准库实现深度剖析

本部分深入分析与 C++ 标准库（STL）直接相关的开源库——不是简单介绍 API，而是剖析**设计原理**、**实现细节**、**与标准库的对比**，以及它们对 C++ 标准演进的影响。

## 主题定位

这些库分为三个层次：

1. **标准库实现**：libc++（LLVM）、libstdc++（GCC）——直接实现 C++ 标准，理解它们就是理解你日常使用的 `std::vector`、`std::string`、`std::format` 到底是怎么工作的
2. **标准的前身与补充**：Abseil、Folly、Boost、EASTL、fmt/spdlog、range-v3——这些库要么催生了标准特性（`std::string_view` ← `folly::Range`，`std::format` ← fmt），要么在标准库不足时提供高性能替代品
3. **STL 替代设计**：EASTL——质疑标准 STL 的设计假设，为特定领域（游戏引擎）重新设计容器和分配器

许多现代 C++ 特性的灵感都来源于这些库：

| 标准特性 | 来源 | 标准版本 |
|---------|------|---------|
| `std::string_view` | `folly::Range` / `absl::string_view` | C++17 |
| `std::optional` | `boost::optional` / `folly::Optional` | C++17 |
| `std::variant` | `boost::variant` | C++17 |
| `std::filesystem` | `boost::filesystem` | C++17 |
| `std::format` | fmt 库 | C++20 |
| `std::expected` | `absl::StatusOr` | C++23 |
| `std::flat_map` | `absl::flat_hash_map` (SwissTable) | C++23 |
| `std::execution` | Folly Executor 模型 | C++26 |
| Ranges | range-v3 | C++20 |

---

## 标准库实现

### [libc++ (LLVM)](/libraries/llvm/)

LLVM 的 C++ 标准库实现，以最紧凑的内存布局和最新 C++ 标准支持著称。

| 章节 | 主题 |
|------|------|
| [vector 与 string](/libraries/llvm/vector-string) | 三指针 vector、24 字节 SSO string、split_buffer、memcpy relocate |
| [function 与 shared_ptr](/libraries/llvm/function-shared-ptr) | 24 字节 SBO function、make_shared 单次分配、控制块布局 |
| [map/set 与 variant](/libraries/llvm/map-variant) | 红黑树 `__tree`、哨兵节点、函数指针表 visit |
| [Ranges 与 unique_ptr](/libraries/llvm/ranges-unique-ptr) | 管道运算符 CRTP、filter_view 缓存、compressed_pair unique_ptr |

### [libstdc++ (GCC)](/libraries/libstdcxx/)

GCC 的 C++ 标准库实现，Linux 生态的事实标准，以 ABI 稳定性著称。

| 章节 | 主题 |
|------|------|
| [string ABI 迁移](/libraries/libstdcxx/string-abi) | COW → SSO 迁移、双 ABI 共存、`__cxx11` 命名空间 |
| [vector 与 SwissTable](/libraries/libstdcxx/vector-swisstable) | vector 增长策略、GCC 11+ SwissTable unordered_map |
| [shared_ptr/RB-tree/function](/libraries/libstdcxx/shared-ptr-tree-function) | 控制块设计、`_Rb_tree` 哨兵、函数指针 SBO |

---

## 标准的前身与高性能替代

### [Abseil (Google)](/libraries/abseil/)

Google 内部 C++ 代码库的开源版本，"Live at Head"理念。

| 章节 | 主题 |
|------|------|
| [SwissTable 哈希表](/libraries/abseil/swisstable) | H1/H2 分割、控制字节布局、SSE2 探测、probe_seq、墓碑优化、SOO |
| [Status 与 StatusOr](/libraries/abseil/status) | tagged union、RAII 语义、错误传播链 |
| [Cord 大文本](/libraries/abseil/cord) | B-tree 结构、外部存储、零拷贝拼接 |
| [字符串与哈希工具](/libraries/abseil/strings-hash) | string_view、StrCat、absl::Hash、SpreadingTree |
| [Time 与基础工具](/libraries/abseil/time-utility) | 时区、Duration、Span、optional |

### [Folly (Meta)](/libraries/folly/)

Meta 的 C++ 基础库，Facebook 的工程实践结晶。

| 章节 | 主题 |
|------|------|
| [fbstring 三级存储](/libraries/folly/fbstring) | Small/Medium/Large 三级、COW 引用计数、页面跨越优化 |
| [F14 哈希表](/libraries/folly/f14) | Chunk-based 布局、SIMD tag 探测、与 SwissTable 对比 |
| [IOBuf 零拷贝缓冲区](/libraries/folly/iobuf) | 循环链表、引用计数、自定义释放 |
| [Future/Promise 异步](/libraries/folly/future-promise) | Core 共享状态、状态机、SemiFuture、Executor |
| [Synchronized 与工具](/libraries/folly/synchronized-tools) | 类型安全锁守卫、Function SBO、ConcurrentHashMap |

### [Boost](/libraries/boost/)（Top 50 热门库）

C++ 生态中最古老、影响最深远的库集合。170+ 个独立库，按领域分组分析前 50 个最热门的库。

#### [总览与导航](/libraries/boost/)

### [EASTL (EA)](/libraries/eastl/)

EA 为游戏引擎重新设计的 STL 替代。

| 章节 | 主题 |
|------|------|
| [分配器模型](/libraries/eastl/allocator) | 非模板实例化、双 allocate 重载、dummy_allocator |
| [fixed_* 容器](/libraries/eastl/fixed-containers) | 内嵌缓冲区、溢出机制、aligned_buffer |
| [侵入式容器与哈希表](/libraries/eastl/intrusive-hashtable) | intrusive_list、hashtable、红黑树 |

### [fmt / spdlog](/libraries/fmt-spdlog/)

`std::format` 的参考实现与零配置日志库。

| 章节 | 主题 |
|------|------|
| [fmt 格式化引擎](/libraries/fmt-spdlog/fmt-engine) | 编译期格式串检查、类型擦除 tagged union、Dragonbox 浮点 |
| [fmt 高级特性](/libraries/fmt-spdlog/fmt-advanced) | 自定义类型、编译期格式化、printf 兼容、ranges/chrono |
| [spdlog 架构](/libraries/fmt-spdlog/spdlog) | Logger/Sink/Formatter 三层、异步模式、日志级别裁剪 |

### [range-v3](/libraries/range-v3/)

C++20 Ranges 的前身，Eric Niebler 的杰作。

| 章节 | 主题 |
|------|------|
| [管道运算符机制](/libraries/range-v3/pipe-operator) | CRTP 基类、`__pipeable`、`__compose`、`__bind_back` |
| [View 实现](/libraries/range-v3/views) | filter_view、transform_view、take_view、惰性求值 |
| [Actions 与 Sentinels](/libraries/range-v3/actions-sentinels) | eager 操作、sentinel 概念、迭代器适配 |
