---
title: "std::tuple 实现分析"
topic: internals
feature: tuple
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/tuple"
source_llvm: "references/impl/llvm-project/libcxx/include/__tuple/"
---

# std::tuple 实现分析

> `std::tuple` 是 C++11 引入的异构容器，可以存储不同类型元素。本文基于 GCC 和 LLVM 的源码，分析 tuple 的内部实现。

---

## 一、核心概念

### 1.1 什么是 tuple

tuple 是一个固定大小的异构容器：

```cpp
// tuple 的基本使用
tuple<int, string, double> t(42, "hello", 3.14);

// 访问元素
int i = get<0>(t);
string s = get<1>(t);
double d = get<2>(t);

// 结构化绑定（C++17）
auto [a, b, c] = t;
```

---

## 二、核心数据结构

### 2.1 递归继承

tuple 通常使用递归继承实现：

```
tuple<int, string, double> 的继承层次：

tuple_element<2, tuple<int, string, double>>
  ↑ 继承
tuple_element<1, tuple<int, string, double>>
  ↑ 继承
tuple_element<0, tuple<int, string, double>>
  ↑ 继承
tuple_leaf<0, int>
tuple_leaf<1, string>
tuple_leaf<2, double>
```

### 2.2 GCC (libstdc++) 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/tuple

// GCC 使用 _Tuple_impl 递归实现

// 递归情况
template<size_t _Idx, typename _Head, typename... _Tail>
struct _Tuple_impl<_Idx, _Head, _Tail...>
    : public _Tuple_impl<_Idx + 1, _Tail...> {
    _Head _M_head;
    
    // 构造函数
    template<typename _UHead, typename... _UTail>
    _Tuple_impl(_UHead&& __head, _UTail&&... __tail)
    : _Tuple_impl<_Idx + 1, _Tail...>(std::forward<_UTail>(__tail)...),
      _M_head(std::forward<_UHead>(__head)) {}
    
    // 拷贝构造
    _Tuple_impl(const _Tuple_impl& __other)
    : _Tuple_impl<_Idx + 1, _Tail...>(__other),
      _M_head(__other._M_head) {}
    
    // 移动构造
    _Tuple_impl(_Tuple_impl&& __other)
    : _Tuple_impl<_Idx + 1, _Tail...>(std::move(__other)),
      _M_head(std::move(__other._M_head)) {}
};

// 基础情况（空 tuple）
template<size_t _Idx>
struct _Tuple_impl<_Idx> {};

// get 的实现
template<size_t _Idx, typename... _Elements>
constexpr tuple_element_t<_Idx, tuple<_Elements...>>&
get(tuple<_Elements...>& __t) noexcept {
    // 通过递归继承找到对应索引的 _M_head
    using _Tuple_type = _Tuple_impl<_Idx, _Elements...>;
    return static_cast<_Tuple_type&>(__t)._M_head;
}

// const 版本
template<size_t _Idx, typename... _Elements>
constexpr const tuple_element_t<_Idx, tuple<_Elements...>>&
get(const tuple<_Elements...>& __t) noexcept {
    using _Tuple_type = _Tuple_impl<_Idx, _Elements...>;
    return static_cast<const _Tuple_type&>(__t)._M_head;
}
```

### 2.3 tuple_size 和 tuple_element（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/tuple

// tuple_size：获取 tuple 元素个数
template<typename... _Elements>
struct tuple_size<tuple<_Elements...>>
: public integral_constant<size_t, sizeof...(_Elements)> {};

// tuple_element：获取第 N 个元素的类型
template<size_t _Idx, typename... _Elements>
struct tuple_element<_Idx, tuple<_Elements...>> {
    static_assert(_Idx < sizeof...(_Elements));
    using type = typename _Nth_type<_Idx, _Elements...>::type;
};

// C++14 变量模板
template<size_t _Idx, typename... _Elements>
inline constexpr size_t tuple_size_v = tuple_size<tuple<_Elements...>>::value;

template<size_t _Idx, typename... _Elements>
using tuple_element_t = typename tuple_element<_Idx, tuple<_Elements...>>::type;
```

---

## 三、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ 实现方式               │ 递归继承             │ 递归继承             │
│ get 实现               │ 成员访问             │ 成员访问             │
│ constexpr 支持         │ C++14/17/20          │ C++14/17/20          │
│ tuple_size             │ 支持                 │ 支持                 │
│ tuple_element           │ 支持                 │ 支持                 │
│ apply                  │ C++17                │ C++17                │
│ 三向比较               │ C++20                │ C++20                │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 四、最佳实践

```
tuple 使用指南：

1. 返回多个值：
   tuple<int, string> func() { return {42, "hello"}; }

2. 使用结构化绑定：
   auto [i, s] = func();

3. 使用 apply 调用：
   apply([](auto... args) { ... }, t);

4. 使用 make_tuple 构造：
   auto t = make_tuple(42, "hello", 3.14);
```

---

## 延伸阅读

- [std::pair 实现](/internals/utilities/pair) — 二元组
- [std::variant 实现](/internals/utilities/variant) — 标签联合
- [std::optional 实现](/internals/utilities/optional) — 可选值
