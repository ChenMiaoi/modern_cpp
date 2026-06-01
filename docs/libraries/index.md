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

## 库列表

### 标准库实现

| 库 | 维护方 | 核心主题 |
|---|--------|---------|
| [libc++](/libraries/llvm) | LLVM | LLVM 的 C++ 标准库实现：`std::string` SSO、`std::vector` 增长策略、Ranges、`<format>` |
| [libstdc++](/libraries/libstdcxx) | GCC | GCC 的 C++ 标准库实现：`std::string` COW 历史、SwissTable 集成、`<format>` 进展 |

### 标准的前身与高性能替代

| 库 | 维护方 | 核心主题 |
|---|--------|---------|
| [Abseil](/libraries/abseil) | Google | SwissTable 哈希表、`StatusOr`、`Cord` 大文本、时区 |
| [Folly](/libraries/folly) | Meta | Futures/Executor、IOBuf 零拷贝、fbstring SSO/COW、ConcurrentHashMap |
| [Boost](/libraries/boost) | Boost 社区 | Asio (Networking TS)、Hana (编译期编程)、Spirit.X3、容器库 |
| [EASTL](/libraries/eastl) | EA | 游戏引擎 STL 替代：fixed_* 容器、非模板分配器、侵入式容器 |
| [fmt / spdlog](/libraries/fmt-spdlog) | 开源社区 | `std::format` 的参考实现：编译期格式检查、Dragonbox 浮点、零分配日志 |
| [range-v3](/libraries/range-v3) | Eric Niebler | C++20 Ranges 的前身：Views/Actions、管道运算符、Sentinel |
