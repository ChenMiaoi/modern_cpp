---
title: "std::pair 实现分析"
topic: internals
feature: pair
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/bits/stl_pair.h"
source_llvm: "references/impl/llvm-project/libcxx/include/__utility/pair.h"
---

# std::pair 实现分析

> `std::pair` 是 C++ 最基础的异构容器，存储两个不同类型的值。本文基于 GCC 和 LLVM 的源码，分析 pair 的内部实现。

---

## 一、核心数据结构

### 1.1 pair 的存储布局

```cpp
// pair 的基本结构
template<typename _T1, typename _T2>
struct pair {
    _T1 first;
    _T2 second;
};

// EBO 优化
template<typename _T1, typename _T2>
struct pair : _Compressed_pair<_T1, _T2> {
    // 空基类优化
};
```

### 1.2 GCC (libstdc++) 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/stl_pair.h

template<typename _T1, typename _T2>
struct pair {
    _T1 first;
    _T2 second;
    
    // 默认构造
    constexpr pair() = default;
    
    // 显式构造
    constexpr pair(const _T1& __a, const _T2& __b)
    : first(__a), second(__b) {}
    
    // 移动构造
    constexpr pair(_T1&& __a, _T2&& __b)
    : first(std::move(__a)), second(std::move(__b)) {}
    
    // piecewise 构造
    template<typename... _Args1, typename... _Args2>
    constexpr pair(piecewise_construct_t,
                   tuple<_Args1...> __first,
                   tuple<_Args2...> __second);
    
    // 比较运算符
    constexpr auto operator<=>(const pair& __other) const {
        if (auto __cmp = first <=> __other.first; __cmp != 0)
            return __cmp;
        return second <=> __other.second;
    }
    
    constexpr bool operator==(const pair& __other) const {
        return first == __other.first && second == __other.second;
    }
};
```

### 1.2 GCC (libstdc++) 的实现

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/stl_pair.h

template<typename _T1, typename _T2>
struct pair {
    _T1 first;
    _T2 second;
    
    // 默认构造
    constexpr pair() = default;
    
    // 显式构造
    constexpr pair(const _T1& __a, const _T2& __b)
    : first(__a), second(__b) {}
    
    // 移动构造
    constexpr pair(_T1&& __a, _T2&& __b)
    : first(std::move(__a)), second(std::move(__b)) {}
    
    // piecewise 构造
    template<typename... _Args1, typename... _Args2>
    constexpr pair(piecewise_construct_t,
                   tuple<_Args1...> __first,
                   tuple<_Args2...> __second);
};
```

---

## 二、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ 基本存储               │ first + second       │ first + second       │
│ EBO 优化               │ 部分支持             │ compressed_pair      │
│ constexpr 支持         │ C++14/17/20          │ C++14/17/20          │
│ piecewise_construct    │ 支持                 │ 支持                 │
│ 三向比较               │ C++20                │ C++20                │
│ get<>                  │ 支持                 │ 支持                 │
│ tuple_size/tuple_element│ 支持                │ 支持                 │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 三、最佳实践

```
pair 使用指南：

1. 使用 make_pair 构造：
   auto p = make_pair(42, "hello");

2. 使用结构化绑定：
   auto [i, s] = p;

3. 使用 get<> 访问：
   int i = get<0>(p);

4. 自定义比较：
   auto cmp = [](const auto& a, const auto& b) {
       return a.first < b.first;
   };
```

---

## 延伸阅读

- [std::tuple 实现](/internals/utilities/tuple) — 多元素异构容器
- [std::map/set 实现](/internals/containers/map-set) — pair 在 map 中的应用
- [std::optional 实现](/internals/utilities/optional) — 可选值容器
