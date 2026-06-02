---
title: "参考资料"
topic: references
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# 参考资料

本页面汇总了 C++ 学习和工程实践中最重要的参考资料。所有可合法免费获取的资源均已下载到 `references/` 目录。

---

## ISO 标准工作草案（已下载）

所有标准草案均可免费获取自 [WG21 Papers](https://open-std.org/jtc1/sc22/wg21/docs/papers/)。

| 版本 | 文档编号 | 说明 | 大小 |
|------|---------|------|------|
| C++98/03 | N1396 | 2002 工作草案（含 C++98 与 TC1 修正） | 992 KB |
| C++11 | N3337 | C++11 最终工作草案（2012-01-16） | 4.9 MB |
| C++14 | N4296 | C++14 工作草案（2014-10-07） | 12 MB |
| C++17 | N4659 | C++17 最终工作草案（2017-03-21） | 6.2 MB |
| C++20 | N4861 | C++20 最终工作草案（2020-04-10） | 6.8 MB |
| C++23 | N4950 | C++23 最终工作草案（2023-05-10） | 7.7 MB |
| C++26 | N5008 | C++26 当前工作草案（2025-03-15） | 8.9 MB |

> **路径**：`references/standards/`

### 如何阅读标准文档

标准文档不是教程——它是**规范性参考**（normative reference）。

1. **查找特定行为**：用 Ctrl+F 搜索关键词
2. **理解条款结构**：§1-§6 通用规则，§7-§22 核心语言，§23-§33 标准库
3. **对照 cppreference**：先看简明解释，再回标准查精确语义

---

## 离线参考资料（已下载）

### cppreference 离线文档（2025-02-09）

完整的 C/C++ 参考文档离线版，覆盖 C++26 最新特性。

- **路径**：`references/cppreference/cppreference-doc-20250209/reference/en/index.html`
- **使用**：浏览器直接打开 `index.html`
- **来源**：[cppreference.com](https://en.cppreference.com/) | [GitHub](https://github.com/PeterFeicht/cppreference-doc)

### C++ Core Guidelines（Stroustrup & Herb Sutter）

- **路径**：`references/guidelines/CppCoreGuidelines/CppCoreGuidelines.md`
- **在线版**：[isocpp.github.io/CppCoreGuidelines](https://isocpp.github.io/CppCoreGuidelines/)

---

## 已下载书籍

以下书籍 PDF 已下载到本地。

### 入门与概览

| 书名 | 作者 | 说明 | 路径 |
|------|------|------|------|
| **Modern C++ Tutorial: C++11/14/17/20** | 欧长坤 (Changkun Ou) | 快速上手现代 C++ | `books/Modern-Cpp-Tutorial-en-us.pdf` |
| **Think C++** | Allen Downey | 面向初学者，渐进式方法 | `books/think-cpp.pdf` |
| **21st Century C++** | Bjarne Stroustrup | C++ 之父的现代 C++ 思考 | `books/21st-Century-Cpp-Stroustrup.pdf` |

### 现代 C++ 综合

| 书名 | 作者 | 说明 | 路径 |
|------|------|------|------|
| **Modern C++ Programming** (C++03~C++26) | Federico Busato | 30MB 全面教程，覆盖所有版本 | `books/Modern-CPP-Programming-Busato.pdf` |
| **Mastering Modern C++23** | SimplifyC++ | C++23 新特性完整指南 | `books/simplifycpp-mastering-modern-cpp23.pdf` |
| **C++ Best Practices** | Jason Turner | 工程实践指南 | `books/cpp-best-practices.pdf` |

### STL 专项

| 书名 | 作者 | 说明 | 路径 |
|------|------|------|------|
| **Mastering STL** | SimplifyC++ | STL 深度剖析 | `books/simplifycpp-mastering-stl.pdf` |
| **Mastering STL in C++23** | SimplifyC++ | C++23 STL 新特性与最佳实践 | `books/simplifycpp-mastering-stl-cpp23.pdf` |

### OOP 与设计模式

| 书名 | 作者 | 说明 | 路径 |
|------|------|------|------|
| **OOP in Modern C++** | SimplifyC++ | 面向对象设计与现代 C++ 实现 | `books/simplifycpp-oop-modern-cpp.pdf` |

### 并发与原子操作

| 书名 | 作者 | 说明 | 路径 |
|------|------|------|------|
| **Atomic Operations** | SimplifyC++ | 原子操作权威指南 | `books/simplifycpp-atomic-operations.pdf` |

### Mini Booklets（速查手册）

SimplifyC++ 提供的精炼速查手册，适合快速查阅。

| 手册 | 说明 | 路径 |
|------|------|------|
| **C++ 低层优化** | 性能优化技巧与底层细节 | `books/minibooklets/cpp-low-level-optimization.pdf` |
| **原子操作指南** | 原子操作完全手册 | `books/minibooklets/atomic-operations-guide.pdf` |
| **模板元编程** | 模板元编程核心概念 | `books/minibooklets/template-metaprogramming.pdf` |
| **现代 C++ 最佳实践** | 黄金原则速查 | `books/minibooklets/best-practices-modern-cpp.pdf` |
| **STL 速查** | STL 容器与算法速查 | `books/minibooklets/stl-mini-booklet.pdf` |

---
## Cookbook 代码仓库（已下载）
以下 Packt 出版的 Cookbook 系列书籍的**配套代码**已下载到 `references/books/cookbook-code/`。书籍本身需购买，但代码仓库公开可用，可直接学习和运行。
| 书名 | 作者 | 代码路径 | 购买链接 |
|------|------|---------|---------|
| **C++17 STL Cookbook** | Jacek Galowicz | `cookbook-code/Cpp17-STL-Cookbook/` | [Packt](https://www.packtpub.com/en-us/product/cpp-17-stl-cookbook-9781787120495) |
| **C++20 STL Cookbook** | Bill Weinman | `cookbook-code/CPP-20-STL-Cookbook/` | [Packt](https://www.packtpub.com/en-us/product/c-20-stl-cookbook-9781809028839) |
| **C++23 STL Cookbook** (2nd ed.) | Bill Weinman | `cookbook-code/C-23-STL-Cookbook-Second-Edition/` | [Packt](https://www.packtpub.com/en-us/product/c-23-stl-cookbook-9781803242248) |
| **Modern C++ Programming Cookbook** (3rd ed.) | Marius Bancila | `cookbook-code/Modern-Cpp-Programming-Cookbook-Third-Edition/` | [Packt](https://www.packtpub.com/en-us/product/modern-c-programming-cookbook-9781835080542) |
| **Advanced C++ Programming Cookbook** | Dr. Rian Quinn | `cookbook-code/Advanced-CPP-Programming-CookBook/` | [Packt](https://www.packtpub.com/en-us/product/advanced-c-programming-cookbook-9781838559915) |
| **Hands-On System Programming with C++** | Dr. Rian Quinn | `cookbook-code/Hands-On-System-Programming-with-CPP/` | [Packt](https://www.packtpub.com/en-us/product/hands-on-system-programming-with-c-9781789137880) |
| **C++ High Performance** (2nd ed.) | Björn Andrist & Viktor Sehr | `cookbook-code/Cpp-High-Performance-Second-Edition/` | [Packt](https://www.packtpub.com/en-us/product/c-high-performance-9781804613146) |
| **Building Low Latency Applications with C++** | Sourav Ghosh | `cookbook-code/Building-Low-Latency-Applications-with-CPP/` | [Packt](https://www.packtpub.com/en-us/product/building-low-latency-applications-with-c-9781838828820) |
| **CMake Cookbook** | Radovan Bast & Roberto Di Remigio | `cookbook-code/CMake-Cookbook/` | [Packt](https://www.packtpub.com/en-us/product/cmake-cookbook-9781788470711) |
| **Design Patterns in Modern C++** (2nd ed.) | Dmitri Nesteruk | `cookbook-code/design-patterns-in-modern-cpp/` | [Apress](https://link.springer.com/book/10.1007/978-1-4842-7295-4) |
| **Boost C++ Cookbook** | Antony Polukhin | `cookbook-code/Boost-Cookbook/` | [Packt](https://www.packtpub.com/en-us/product/boost-c-cookbook-9781787282247) |
| **AwesomePerfCpp** (资源列表) | Bartłomiej Filipek | `cookbook-code/AwesomePerfCpp/` | [GitHub](https://github.com/fenbf/AwesomePerfCpp) |
---
## 如何获取付费书籍

### Internet Archive 合法借阅（推荐）

以下书籍在 [Internet Archive](https://archive.org/) 上可通过 **Controlled Digital Lending (CDL)** 合法借阅。注册免费账户即可在线阅读（每次借阅 1 小时，可续借）。

| 书名 | 作者 | Internet Archive 链接 |
|------|------|---------------------|
| **Effective Modern C++** | Scott Meyers | [借阅](https://archive.org/details/effectivemodernc0000meye) |
| **More Effective C++** | Scott Meyers | [借阅](https://archive.org/details/moreeffectivec3500meye) |
| **C++ Primer** (早期版) | Stanley B. Lippman | [借阅](https://archive.org/details/cplusplusprimer00lipp) |
| **The C++ Standard Library** (1st ed.) | Nicolai M. Josuttis | [借阅](https://archive.org/details/cstandardlibrary00josu) |
| **C++ Templates** (1st ed.) | David Vandevoorde et al. | [借阅](https://archive.org/details/ctemplatescomple0000vand) |

> **使用方法**：访问链接 → 点击 "Borrow" → 登录或注册 → 在线阅读。无需下载，浏览器内阅读。

### 出版商官方样本章节（已下载）

以下书籍的出版社官方样本 PDF 已下载到 `references/books/samples/`，包含目录和部分章节内容。

| 书名 | 作者 | 样本内容 | 路径 |
|------|------|---------|------|
| **The C++ Standard Library** (2nd ed.) | Nicolai M. Josuttis | 目录 + 部分章节 | `samples/Josuttis-CppStandardLibrary-sample.pdf` |
| **C++ Templates: The Complete Guide** (2nd ed.) | Vandevoorde et al. | 目录 + 部分章节 | `samples/Vandevoorde-CppTemplates-sample.pdf` |
| **C++ Primer** (5th ed.) | Lippman et al. | 目录 + 部分章节 | `samples/Lippman-CppPrimer-sample.pdf` |
| **Modern C++ Design** | Andrei Alexandrescu | 目录 + 部分章节 | `samples/Alexandrescu-ModernCppDesign-sample.pdf` |

### 其他合法获取途径

- **O'Reilly Learning**：订阅制，包含绝大多数 C++ 书籍的完整电子版。很多大学/公司有机构订阅。[oreilly.com](https://www.oreilly.com/)
- **公共图书馆**：通过 OverDrive/Libby 应用，可用图书馆卡免费借阅电子书。[overdrive.com](https://www.overdrive.com/)
- **大学图书馆**：大多数大学图书馆有 O'Reilly、Springer、Packt 等出版社的电子书访问权限。
- **Leanpub**：部分书籍（如 Josuttis 的 C++17/20/23 Complete Guide 系列）在 Leanpub 上以合理价格出售。[leanpub.com](https://leanpub.com/)

---

## 推荐书籍（付费）
以下书籍是 C++ 领域公认的经典和权威著作，按主题分类，建议按需购买。

### 入门经典

| 书名 | 作者 | 推荐理由 |
|------|------|---------|
| **A Tour of C++** (3rd ed., C++20) | Bjarne Stroustrup | C++ 之父的概览，快速建立全局认知 |
| **Programming: Principles and Practice Using C++** (3rd ed.) | Bjarne Stroustrup | 面向初学者的系统性教材 |
| **C++ Primer** (5th ed.) | Stanley B. Lippman et al. | 最详尽的入门参考书（C++11） |
| **Head First C++** | David Griffiths & Dawn Griffiths | 图文并茂的入门书 |

### Effective 系列与工程实践

| 书名 | 作者 | 推荐理由 |
|------|------|---------|
| **Effective C++** (3rd ed.) | Scott Meyers | 55 条经典建议，C++ 工程师必读 |
| **Effective Modern C++** | Scott Meyers | 42 条 C++11/14 最佳实践，必读 |
| **Effective STL** | Scott Meyers | 50 条 STL 使用建议 |
| **More Effective C++** | Scott Meyers | 35 条进阶建议（异常安全、效率） |
| **C++ Coding Standards** | Herb Sutter & Andrei Alexandrescu | 101 条编码规范 |
| **Exceptional C++** | Herb Sutter | 问题-解答式深入 C++ |
| **More Exceptional C++** | Herb Sutter | 续篇，覆盖异常安全与泛型 |
| **Exceptional C++ Style** | Herb Sutter | 第三本，聚焦工程风格 |
| **C++ Gotchas** | Stephen Dewhurst | 常见陷阱与解决方案 |

### STL 与泛型编程

| 书名 | 作者 | 推荐理由 |
|------|------|---------|
| **The C++ Standard Library** (2nd ed.) | Nicolai M. Josuttis | STL 权威参考（C++11） |
| **STL Tutorial and Reference Guide** | David R. Musser et al. | STL 经典教程 |
| **The C++ Standard Template Library** | P. J. Plauger et al. | STL 实现细节 |
| **Generic Programming and the STL** | Matthew H. Austern | 泛型编程理论与 STL 设计 |
| **C++ STL Programmer's Guide** | Mark Nelson | STL 实用指南 |

### 模板与元编程

| 书名 | 作者 | 推荐理由 |
|------|------|---------|
| **C++ Templates: The Complete Guide** (2nd ed.) | David Vandevoorde et al. | 模板圣经，覆盖 C++17 |
| **Modern C++ Design** | Andrei Alexandrescu | 策略驱动设计，模板元编程经典 |
| **C++ Template Metaprogramming** | David Abrahams & Aleksey Gurtovoy | Boost.MPL 背后的理论 |
| **Advanced Metaprogramming in Classic C++** | Mario Russo | Springer 出版，深度元编程 |

### 并发与多线程

| 书名 | 作者 | 推荐理由 |
|------|------|---------|
| **C++ Concurrency in Action** (2nd ed.) | Anthony Williams | 多线程编程权威指南（C++17） |
| **Pro TBB** | Michael Voss et al. | Intel TBB 并行编程 |
| **The Art of Multiprocessor Programming** | Maurice Herlihy & Nir Shavit | 并发算法理论基础 |

### 性能优化

| 书名 | 作者 | 推荐理由 |
|------|------|---------|
| **Optimized C++** | Kurt Guntheroth | 性能优化系统性方法 |
| **C++ High Performance** (2nd ed.) | Björn Andrist & Viktor Sehr | 现代 C++ 性能优化 |
| **Building Low Latency Applications with C++** | Sourav Ghosh | 低延迟系统开发（金融/HFT） |
| **Agner Fog 优化手册** (免费) | Agner Fog | CPU 微架构与汇编优化 |

### 现代 C++ 进阶（按版本）

| 书名 | 作者 | 覆盖版本 | 推荐理由 |
|------|------|---------|---------|
| **Effective Modern C++** | Scott Meyers | C++11/14 | 必读 |
| **C++17 in Detail** | Bartłomiej Filipek | C++17 | 特性深度剖析 |
| **C++20 in Detail** | Bartłomiej Filipek | C++20 | 特性深度剖析 |
| **C++23 - The Complete Guide** | Nicolai M. Josuttis | C++23 | 权威参考 |
| **C++17 - The Complete Guide** | Nicolai M. Josuttis | C++17 | 权威参考 |
| **C++ Move Semantics** | Nicolai M. Josuttis | C++11+ | 移动语义完整剖析 |

### 设计与架构

| 书名 | 作者 | 推荐理由 |
|------|------|---------|
| **Design Patterns in Modern C++** (2nd ed.) | Dmitri Nesteruk | GoF 模式的现代 C++ 实现 |
| **Large-Scale C++ Software Design** | John Lakos | 大型项目的物理设计（头文件/链接） |
| **API Design for C++** | Martin Reddy | 库设计实践 |
| **Practical C++ Design** | Adnan Aziz | 从编程到架构 |
| **Head First Design Patterns** | Eric Freeman et al. | 设计模式入门（Java 但概念通用） |

### 深度与底层

| 书名 | 作者 | 推荐理由 |
|------|------|---------|
| **Inside the C++ Object Model** | Stanley B. Lippman | 对象模型的底层实现（虚表/内存布局） |
| **The C++ Programming Language** (4th ed.) | Bjarne Stroustrup | C++ 百科全书（C++11） |
| **C++ Under the Hood** | Mathieu Nayrolles | 编译器内部实现 |

### CMake 与构建系统

| 书名 | 作者 | 推荐理由 |
|------|------|---------|
| **CMake Cookbook** | Radovan Bast & Roberto Di Remigio | CMake 实战食谱 |
| **Professional CMake** | Craig Scott | CMake 权威指南 |
| **Modern CMake** (在线免费) | 多位作者 | [modern-cmake.github.io](https://modern-cmake.github.io/) |

### Boost 库

| 书名 | 作者 | 推荐理由 |
|------|------|---------|
| **Boost C++ Libraries** (在线免费) | Boris Schäling | [theboostcpplibraries.com](https://theboostcpplibraries.com/) |
| **Boost Cookbook** | Antony Polukhin | Boost 库实用食谱 |

---
## 在线资源

### 权威参考

| 资源 | URL | 说明 |
|------|-----|------|
| cppreference | [en.cppreference.com](https://en.cppreference.com/) | 最权威的在线参考 |
| C++ Reference (isocpp) | [isocpp.org](https://isocpp.org/) | ISO C++ 官方网站 |
| WG21 Papers | [open-std.org/jtc1/sc22/wg21](https://open-std.org/jtc1/sc22/wg21/docs/papers/) | 标准委员会论文库 |
| C++ Core Guidelines | [isocpp.github.io/CppCoreGuidelines](https://isocpp.github.io/CppCoreGuidelines/) | 编码规范 |

### 学习与交互

| 资源 | URL | 说明 |
|------|-----|------|
| Learn C++ | [learncpp.com](https://www.learncpp.com/) | 系统性在线教程 |
| C++ Insights | [cppinsights.io](https://cppinsights.io/) | 查看编译器展开后的代码 |
| Compiler Explorer | [godbolt.org](https://godbolt.org/) | 在线编译器 |
| CppQuiz | [cppquiz.org](https://cppquiz.org/) | C++ 语言陷阱测验 |

### 博客与深度文章

| 资源 | URL | 说明 |
|------|-----|------|
| C++ Stories | [cppstories.com](https://www.cppstories.com/) | Bartłomiej Filipek 的深度文章 |
| Herb Sutter's Blog | [herbsutter.com](https://herbsutter.com/) | 标准委员会主席博客 |
| Fluent C++ | [fluentcpp.com](https://www.fluentcpp.com/) | 表达力强的 C++ 代码 |
| Modernes C++ | [modernescpp.com](https://www.modernescpp.com/) | Rainer Grimm 的 C++ 教程 |
| Abseil C++ Tips | [abseil.io/tips](https://abseil.io/tips/) | Google 内部 C++ 最佳实践 |
| SimplifyC++ | [simplifycpp.org](https://www.simplifycpp.org/) | C++ 教程与免费书籍 |

### 编译器文档

| 资源 | URL |
|------|-----|
| GCC | [gcc.gnu.org/onlinedocs](https://gcc.gnu.org/onlinedocs/) |
| Clang | [clang.llvm.org/docs](https://clang.llvm.org/docs/) |
| MSVC | [learn.microsoft.com/en-us/cpp](https://learn.microsoft.com/en-us/cpp/) |

### 性能优化资源

| 资源 | URL | 说明 |
|------|-----|------|
| AwesomePerfCpp | [github.com/fenbf/AwesomePerfCpp](https://github.com/fenbf/AwesomePerfCpp) | C++ 性能优化资源列表 |
| Agner Fog 优化手册 | [agner.org/optimize](https://www.agner.org/optimize/) | CPU 微架构权威参考（免费） |

### 重要提案与论文

| 论文 | 编号 | 主题 |
|------|------|------|
| [Move Semantics](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2006/n2118.html) | N2118 | 移动语义原始提案 |
| [Rvalue Reference](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2006/n1952.html) | N1952 | 右值引用设计 |
| [Lambda](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2008/n2550.pdf) | N2550 | Lambda 原始提案 |
| [Memory Model](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2005/n1911.pdf) | N1911 | 内存模型原始提案 |
| [Concepts](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p2095r0.pdf) | P2095R0 | Concepts 最终形态 |
| [Reflection](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p2996r8.html) | P2996R8 | C++26 反射提案 |
| [Contracts](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p2900r14.pdf) | P2900R14 | C++26 契约提案 |

---

> 本资料库将持续更新。如有重要遗漏，请在 GitHub 提出 Issue。