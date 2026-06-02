---
title: "References"
topic: references
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# References

This page compiles the most important reference materials for C++ learning and engineering practice. All resources that are legally available for free have been downloaded to the `references/` directory.

---

## ISO Standard Working Drafts (Downloaded)

All standard drafts are freely available from [WG21 Papers](https://open-std.org/jtc1/sc22/wg21/docs/papers/).

| Version | Document No. | Description | Size |
|---------|-------------|-------------|------|
| C++98/03 | N1396 | 2002 Working Draft (includes C++98 and TC1 corrections) | 992 KB |
| C++11 | N3337 | C++11 Final Working Draft (2012-01-16) | 4.9 MB |
| C++14 | N4296 | C++14 Working Draft (2014-10-07) | 12 MB |
| C++17 | N4659 | C++17 Final Working Draft (2017-03-21) | 6.2 MB |
| C++20 | N4861 | C++20 Final Working Draft (2020-04-10) | 6.8 MB |
| C++23 | N4950 | C++23 Final Working Draft (2023-05-10) | 7.7 MB |
| C++26 | N5008 | C++26 Current Working Draft (2025-03-15) | 8.9 MB |

> **Path**: `references/standards/`

### How to Read Standard Documents

The standard document is not a tutorial — it is a **normative reference**.

1. **Look up specific behavior**: use Ctrl+F to search for keywords
2. **Understand the clause structure**: §1–§6 General rules, §7–§22 Core language, §23–§33 Standard library
3. **Cross-reference with cppreference**: read the concise explanation first, then consult the standard for precise semantics

---

## Offline Reference Materials (Downloaded)

### cppreference Offline Documentation (2025-02-09)

Complete offline version of the C/C++ reference documentation, covering the latest C++26 features.

- **Path**: `references/cppreference/cppreference-doc-20250209/reference/en/index.html`
- **Usage**: open `index.html` directly in a browser
- **Source**: [cppreference.com](https://en.cppreference.com/) | [GitHub](https://github.com/PeterFeicht/cppreference-doc)

### C++ Core Guidelines (Stroustrup & Herb Sutter)

- **Path**: `references/guidelines/CppCoreGuidelines/CppCoreGuidelines.md`
- **Online**: [isocpp.github.io/CppCoreGuidelines](https://isocpp.github.io/CppCoreGuidelines/)

---

## Downloaded Books

The following book PDFs have been downloaded locally.

### Introduction & Overview

| Title | Author | Description | Path |
|-------|--------|-------------|------|
| **Modern C++ Tutorial: C++11/14/17/20** | Changkun Ou | Quick introduction to modern C++ | `books/Modern-Cpp-Tutorial-en-us.pdf` |
| **Think C++** | Allen Downey | Beginner-friendly, progressive approach | `books/think-cpp.pdf` |
| **21st Century C++** | Bjarne Stroustrup | The creator of C++ on modern C++ thinking | `books/21st-Century-Cpp-Stroustrup.pdf` |

### Modern C++ Comprehensive

| Title | Author | Description | Path |
|-------|--------|-------------|------|
| **Modern C++ Programming** (C++03–C++26) | Federico Busato | 30 MB comprehensive tutorial covering all versions | `books/Modern-CPP-Programming-Busato.pdf` |
| **Mastering Modern C++23** | SimplifyC++ | Complete guide to C++23 new features | `books/simplifycpp-mastering-modern-cpp23.pdf` |
| **C++ Best Practices** | Jason Turner | Engineering practice guide | `books/cpp-best-practices.pdf` |

### STL Specialization

| Title | Author | Description | Path |
|-------|--------|-------------|------|
| **Mastering STL** | SimplifyC++ | In-depth STL analysis | `books/simplifycpp-mastering-stl.pdf` |
| **Mastering STL in C++23** | SimplifyC++ | C++23 STL new features and best practices | `books/simplifycpp-mastering-stl-cpp23.pdf` |

### OOP & Design Patterns

| Title | Author | Description | Path |
|-------|--------|-------------|------|
| **OOP in Modern C++** | SimplifyC++ | Object-oriented design with modern C++ implementation | `books/simplifycpp-oop-modern-cpp.pdf` |

### Concurrency & Atomic Operations

| Title | Author | Description | Path |
|-------|--------|-------------|------|
| **Atomic Operations** | SimplifyC++ | Authoritative guide to atomic operations | `books/simplifycpp-atomic-operations.pdf` |

### Mini Booklets (Quick Reference)

Concise quick-reference booklets from SimplifyC++, ideal for quick lookups.

| Booklet | Description | Path |
|---------|-------------|------|
| **C++ Low-Level Optimization** | Performance optimization techniques and low-level details | `books/minibooklets/cpp-low-level-optimization.pdf` |
| **Atomic Operations Guide** | Complete atomic operations handbook | `books/minibooklets/atomic-operations-guide.pdf` |
| **Template Metaprogramming** | Core concepts of template metaprogramming | `books/minibooklets/template-metaprogramming.pdf` |
| **Modern C++ Best Practices** | Golden rules quick reference | `books/minibooklets/best-practices-modern-cpp.pdf` |
| **STL Quick Reference** | STL containers and algorithms cheat sheet | `books/minibooklets/stl-mini-booklet.pdf` |

---

## Cookbook Code Repositories (Downloaded)

The **companion code** for the following Packt Cookbook series books has been downloaded to `references/books/cookbook-code/`. The books themselves must be purchased, but the code repositories are publicly available and can be studied and run directly.

| Title | Author | Code Path | Purchase Link |
|-------|--------|-----------|---------------|
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
| **AwesomePerfCpp** (resource list) | Bartłomiej Filipek | `cookbook-code/AwesomePerfCpp/` | [GitHub](https://github.com/fenbf/AwesomePerfCpp) |

---

## How to Obtain Paid Books

### Internet Archive Legal Lending (Recommended)

The following books can be legally borrowed through **Controlled Digital Lending (CDL)** on [Internet Archive](https://archive.org/). Register a free account to read online (each loan is 1 hour, renewable).

| Title | Author | Internet Archive Link |
|-------|--------|----------------------|
| **Effective Modern C++** | Scott Meyers | [Borrow](https://archive.org/details/effectivemodernc0000meye) |
| **More Effective C++** | Scott Meyers | [Borrow](https://archive.org/details/moreeffectivec3500meye) |
| **C++ Primer** (early edition) | Stanley B. Lippman | [Borrow](https://archive.org/details/cplusplusprimer00lipp) |
| **The C++ Standard Library** (1st ed.) | Nicolai M. Josuttis | [Borrow](https://archive.org/details/cstandardlibrary00josu) |
| **C++ Templates** (1st ed.) | David Vandevoorde et al. | [Borrow](https://archive.org/details/ctemplatescomple0000vand) |

> **How to use**: visit the link → click "Borrow" → log in or register → read online. No download needed; read in your browser.

### Publisher Official Sample Chapters (Downloaded)

Official publisher sample PDFs for the following books have been downloaded to `references/books/samples/`, containing the table of contents and select chapters.

| Title | Author | Sample Content | Path |
|-------|--------|----------------|------|
| **The C++ Standard Library** (2nd ed.) | Nicolai M. Josuttis | TOC + select chapters | `samples/Josuttis-CppStandardLibrary-sample.pdf` |
| **C++ Templates: The Complete Guide** (2nd ed.) | Vandevoorde et al. | TOC + select chapters | `samples/Vandevoorde-CppTemplates-sample.pdf` |
| **C++ Primer** (5th ed.) | Lippman et al. | TOC + select chapters | `samples/Lippman-CppPrimer-sample.pdf` |
| **Modern C++ Design** | Andrei Alexandrescu | TOC + select chapters | `samples/Alexandrescu-ModernCppDesign-sample.pdf` |

### Other Legal Access Options

- **O'Reilly Learning**: subscription-based, includes full e-books for the vast majority of C++ books. Many universities/companies have institutional subscriptions. [oreilly.com](https://www.oreilly.com/)
- **Public Libraries**: via the OverDrive/Libby app, borrow e-books for free with a library card. [overdrive.com](https://www.overdrive.com/)
- **University Libraries**: most university libraries have e-book access to publishers such as O'Reilly, Springer, Packt, etc.
- **Leanpub**: some books (such as Josuttis's C++17/20/23 Complete Guide series) are sold on Leanpub at reasonable prices. [leanpub.com](https://leanpub.com/)

---

## Recommended Books (Paid)

The following books are universally recognized classics and authoritative works in C++, organized by topic. Purchase as needed.

### Introduction Classics

| Title | Author | Why Recommended |
|-------|--------|-----------------|
| **A Tour of C++** (3rd ed., C++20) | Bjarne Stroustrup | An overview from the creator of C++ to quickly build a global understanding |
| **Programming: Principles and Practice Using C++** (3rd ed.) | Bjarne Stroustrup | A systematic textbook for beginners |
| **C++ Primer** (5th ed.) | Stanley B. Lippman et al. | The most comprehensive introductory reference (C++11) |
| **Head First C++** | David Griffiths & Dawn Griffiths | A visually rich introductory book |

### Effective Series & Engineering Practice

| Title | Author | Why Recommended |
|-------|--------|-----------------|
| **Effective C++** (3rd ed.) | Scott Meyers | 55 classic tips — essential reading for C++ engineers |
| **Effective Modern C++** | Scott Meyers | 42 best practices for C++11/14 — must-read |
| **Effective STL** | Scott Meyers | 50 tips for using STL |
| **More Effective C++** | Scott Meyers | 35 advanced tips (exception safety, efficiency) |
| **C++ Coding Standards** | Herb Sutter & Andrei Alexandrescu | 101 coding guidelines |
| **Exceptional C++** | Herb Sutter | Problem–solution deep dives into C++ |
| **More Exceptional C++** | Herb Sutter | Sequel, covering exception safety and generics |
| **Exceptional C++ Style** | Herb Sutter | Third book, focused on engineering style |
| **C++ Gotchas** | Stephen Dewhurst | Common pitfalls and solutions |

### STL & Generic Programming

| Title | Author | Why Recommended |
|-------|--------|-----------------|
| **The C++ Standard Library** (2nd ed.) | Nicolai M. Josuttis | Authoritative STL reference (C++11) |
| **STL Tutorial and Reference Guide** | David R. Musser et al. | Classic STL tutorial |
| **The C++ Standard Template Library** | P. J. Plauger et al. | STL implementation details |
| **Generic Programming and the STL** | Matthew H. Austern | Generic programming theory and STL design |
| **C++ STL Programmer's Guide** | Mark Nelson | Practical STL guide |

### Templates & Metaprogramming

| Title | Author | Why Recommended |
|-------|--------|-----------------|
| **C++ Templates: The Complete Guide** (2nd ed.) | David Vandevoorde et al. | The template bible, covering C++17 |
| **Modern C++ Design** | Andrei Alexandrescu | Policy-driven design, a metaprogramming classic |
| **C++ Template Metaprogramming** | David Abrahams & Aleksey Gurtovoy | The theory behind Boost.MPL |
| **Advanced Metaprogramming in Classic C++** | Mario Russo | Springer publication, deep metaprogramming |

### Concurrency & Multithreading

| Title | Author | Why Recommended |
|-------|--------|-----------------|
| **C++ Concurrency in Action** (2nd ed.) | Anthony Williams | Authoritative guide to multithreaded programming (C++17) |
| **Pro TBB** | Michael Voss et al. | Intel TBB parallel programming |
| **The Art of Multiprocessor Programming** | Maurice Herlihy & Nir Shavit | Theoretical foundations of concurrent algorithms |

### Performance Optimization

| Title | Author | Why Recommended |
|-------|--------|-----------------|
| **Optimized C++** | Kurt Guntheroth | Systematic approach to performance optimization |
| **C++ High Performance** (2nd ed.) | Björn Andrist & Viktor Sehr | Modern C++ performance optimization |
| **Building Low Latency Applications with C++** | Sourav Ghosh | Low-latency system development (finance/HFT) |
| **Agner Fog Optimization Manuals** (free) | Agner Fog | CPU microarchitecture and assembly optimization |

### Modern C++ Advanced (by Version)

| Title | Author | Covers | Why Recommended |
|-------|--------|--------|-----------------|
| **Effective Modern C++** | Scott Meyers | C++11/14 | Must-read |
| **C++17 in Detail** | Bartłomiej Filipek | C++17 | In-depth feature analysis |
| **C++20 in Detail** | Bartłomiej Filipek | C++20 | In-depth feature analysis |
| **C++23 - The Complete Guide** | Nicolai M. Josuttis | C++23 | Authoritative reference |
| **C++17 - The Complete Guide** | Nicolai M. Josuttis | C++17 | Authoritative reference |
| **C++ Move Semantics** | Nicolai M. Josuttis | C++11+ | Complete treatment of move semantics |

### Design & Architecture

| Title | Author | Why Recommended |
|-------|--------|-----------------|
| **Design Patterns in Modern C++** (2nd ed.) | Dmitri Nesteruk | GoF patterns in modern C++ |
| **Large-Scale C++ Software Design** | John Lakos | Physical design for large projects (headers/linking) |
| **API Design for C++** | Martin Reddy | Library design practice |
| **Practical C++ Design** | Adnan Aziz | From programming to architecture |
| **Head First Design Patterns** | Eric Freeman et al. | Design patterns introduction (Java, but concepts are universal) |

### Deep Dives & Internals

| Title | Author | Why Recommended |
|-------|--------|-----------------|
| **Inside the C++ Object Model** | Stanley B. Lippman | Low-level object model implementation (vtable/memory layout) |
| **The C++ Programming Language** (4th ed.) | Bjarne Stroustrup | The C++ encyclopedia (C++11) |
| **C++ Under the Hood** | Mathieu Nayrolles | Compiler internals |

### CMake & Build Systems

| Title | Author | Why Recommended |
|-------|--------|-----------------|
| **CMake Cookbook** | Radovan Bast & Roberto Di Remigio | Hands-on CMake recipes |
| **Professional CMake** | Craig Scott | Authoritative CMake guide |
| **Modern CMake** (free online) | Multiple authors | [modern-cmake.github.io](https://modern-cmake.github.io/) |

### Boost Libraries

| Title | Author | Why Recommended |
|-------|--------|-----------------|
| **Boost C++ Libraries** (free online) | Boris Schäling | [theboostcpplibraries.com](https://theboostcpplibraries.com/) |
| **Boost Cookbook** | Antony Polukhin | Practical Boost library recipes |

---

## Online Resources

### Authoritative References

| Resource | URL | Description |
|----------|-----|-------------|
| cppreference | [en.cppreference.com](https://en.cppreference.com/) | The most authoritative online reference |
| C++ Reference (isocpp) | [isocpp.org](https://isocpp.org/) | ISO C++ official website |
| WG21 Papers | [open-std.org/jtc1/sc22/wg21](https://open-std.org/jtc1/sc22/wg21/docs/papers/) | Standards committee paper library |
| C++ Core Guidelines | [isocpp.github.io/CppCoreGuidelines](https://isocpp.github.io/CppCoreGuidelines/) | Coding guidelines |

### Learning & Interactive

| Resource | URL | Description |
|----------|-----|-------------|
| Learn C++ | [learncpp.com](https://www.learncpp.com/) | Systematic online tutorial |
| C++ Insights | [cppinsights.io](https://cppinsights.io/) | View compiler-expanded code |
| Compiler Explorer | [godbolt.org](https://godbolt.org/) | Online compiler |
| CppQuiz | [cppquiz.org](https://cppquiz.org/) | C++ language gotchas quiz |

### Blogs & In-Depth Articles

| Resource | URL | Description |
|----------|-----|-------------|
| C++ Stories | [cppstories.com](https://www.cppstories.com/) | In-depth articles by Bartłomiej Filipek |
| Herb Sutter's Blog | [herbsutter.com](https://herbsutter.com/) | Standards committee chair's blog |
| Fluent C++ | [fluentcpp.com](https://www.fluentcpp.com/) | Expressive C++ code |
| Modernes C++ | [modernescpp.com](https://www.modernescpp.com/) | Rainer Grimm's C++ tutorials |
| Abseil C++ Tips | [abseil.io/tips](https://abseil.io/tips/) | Google's internal C++ best practices |
| SimplifyC++ | [simplifycpp.org](https://www.simplifycpp.org/) | C++ tutorials and free books |

### Compiler Documentation

| Resource | URL |
|----------|-----|
| GCC | [gcc.gnu.org/onlinedocs](https://gcc.gnu.org/onlinedocs/) |
| Clang | [clang.llvm.org/docs](https://clang.llvm.org/docs/) |
| MSVC | [learn.microsoft.com/en-us/cpp](https://learn.microsoft.com/en-us/cpp/) |

### Performance Optimization Resources

| Resource | URL | Description |
|----------|-----|-------------|
| AwesomePerfCpp | [github.com/fenbf/AwesomePerfCpp](https://github.com/fenbf/AwesomePerfCpp) | C++ performance optimization resource list |
| Agner Fog Optimization Manuals | [agner.org/optimize](https://www.agner.org/optimize/) | Authoritative CPU microarchitecture reference (free) |

### Key Proposals & Papers

| Paper | No. | Topic |
|-------|-----|-------|
| [Move Semantics](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2006/n2118.html) | N2118 | Original move semantics proposal |
| [Rvalue Reference](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2006/n1952.html) | N1952 | Rvalue reference design |
| [Lambda](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2008/n2550.pdf) | N2550 | Original lambda proposal |
| [Memory Model](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2005/n1911.pdf) | N1911 | Original memory model proposal |
| [Concepts](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p2095r0.pdf) | P2095R0 | Final form of Concepts |
| [Reflection](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p2996r8.html) | P2996R8 | C++26 reflection proposal |
| [Contracts](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p2900r14.pdf) | P2900R14 | C++26 contracts proposal |

---

> This reference library is continuously updated. If there are important omissions, please open an issue on GitHub.
