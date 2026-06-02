---
title: "Topics"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# Topics

This section organizes C++ core knowledge areas from a cross-version perspective. Each topic integrates the evolution of related features across multiple standard versions.

## Topic List

| Topic | Description |
|-------|-------------|
| [C++ Jargon Encyclopedia](/topics/cpp-jargon/) | 120+ professional terms systematically explained: value categories, overload resolution, SFINAE, type erasure, exception safety, UB, and more |
| [Memory Model and Concurrency](/topics/memory-model) | From C++11's memory model to C++26's Senders/Receivers |
| [Template Metaprogramming](/topics/template-metaprogramming) | From SFINAE to Concepts — the evolution of template techniques |
| [RAII and Resource Management](/topics/raii) | From constructors/destructors to smart pointers to coroutine scope guards |
| [Compile-Time Computation](/topics/compile-time-computation) | `constexpr` → `consteval` → `constinit` → reflection |
| [Compiler Optimizations](/topics/compiler-optimizations) | Full optimization pipeline: inlining, SROA, loop vectorization, LTO, PGO |
| [ABI Deep Dive](/topics/abi) | Name mangling, vtable layout, exception ABI, calling conventions, symbol visibility, ABI versioning strategies |
| [C++ Design Patterns](/topics/design-patterns) | Classic design patterns implemented with modern C++ |
| [Value Categories Deep Dive](/topics/value-categories-deep-dive) | From C to C++17: five value categories, materialization, move semantics, perfect forwarding, copy elision |
| [Performance Optimization](/topics/performance) | Move semantics, small object optimization, cache-friendly design |
| [Object Lifetime](/topics/lifetime) | Storage duration, subobjects, dangling pointers and references, implicit object creation, constexpr lifetime |
| [Toolchain and Ecosystem](/topics/toolchain) | Compilers, build systems, package managers, sanitizers |

## Reading Recommendations

If you are a **C++ beginner**:
1. First read through all features of [C++11](/standards/cpp11/)
2. Then browse C++14 → C++17 → C++20 in version order
3. Then pick topics of interest for deeper study

If you are an **experienced C++ developer**:
1. Start from the version you currently use and quickly learn about new features in subsequent versions
2. Focus on the topics section to build a cross-version knowledge system
3. Keep an eye on the cutting-edge developments in C++26/29
