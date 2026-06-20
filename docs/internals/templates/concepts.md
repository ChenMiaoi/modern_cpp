---
title: "Concepts 实现分析"
topic: internals
feature: concepts
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/concepts"
source_llvm: "references/impl/llvm-project/libcxx/include/__concepts/"
---

# Concepts 实现分析

> Concepts 是 C++20 引入的模板约束机制，提供更清晰的模板接口和更好的错误信息。本文基于 GCC 和 LLVM 的源码，分析 Concepts 的内部实现。

---

## 一、核心概念

### 1.1 什么是 Concepts

Concepts 是对模板参数的约束，编译期检查类型是否满足要求：

```cpp
// 定义 concept
template<typename T>
concept Sortable = requires(T a, T b) {
    { a < b } -> convertible_to<bool>;
};

// 使用 concept
template<Sortable T>
void sort(vector<T>& v) { ... }

// 约束函数模板
void process(Sortable auto& v) { ... }
```

### 1.2 Concepts vs SFINAE

```
Concepts vs SFINAE：

SFINAE：
  · 间接约束（通过 enable_if）
  · 错误信息复杂
  · 需要额外的模板技巧

Concepts：
  · 直接约束（通过 requires）
  · 错误信息清晰
  · 更直观的语法
```

---

## 二、核心数据结构

### 2.1 requires 表达式（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/concepts

// requires 表达式的类型

// 1. 简单要求：表达式必须有效
template<typename T>
concept Sortable = requires(T a, T b) {
    { a < b } -> boolean_testable;  // a < b 必须有效且返回布尔可测试类型
    { a > b } -> boolean_testable;
    { a <= b } -> boolean_testable;
    { a >= b } -> boolean_testable;
};

// 2. 类型要求：类型必须存在
template<typename T>
concept Container = requires {
    typename T::value_type;      // T 必须有 value_type 类型
    typename T::iterator;        // T 必须有 iterator 类型
    typename T::const_iterator;  // T 必须有 const_iterator 类型
};

// 3. 复合要求：表达式必须有效，且返回类型满足约束
template<typename T>
concept Range = requires(T& t) {
    ranges::begin(t);   // begin(t) 必须有效
    ranges::end(t);     // end(t) 必须有效
};

// 4. 嵌套要求：嵌套的约束必须满足
template<typename T>
concept Integral = requires {
    requires is_integral_v<T>;  // T 必须是整数类型
};

// 5. compound_requirement：复合要求的完整形式
template<typename T>
concept Hashable = requires(T t) {
    { std::hash<T>{}(t) } -> std::convertible_to<size_t>;
    // 表达式有效，且返回类型可转换为 size_t
};
```

### 2.2 标准 concepts（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/concepts

// same_as：两个类型相同
template<typename _Tp, typename _Up>
concept same_as = __is_same_as(_Tp, _Up);

// derived_from：派生关系
template<typename _Tp, typename _Up>
concept derived_from = __is_base_of(_Up, _Tp) && __is_convertible(_Tp*, _Up*);

// convertible_to：可转换
template<typename _From, typename _To>
concept convertible_to = __is_convertible(_From, _To);

// integral：整数类型
template<typename _Tp>
concept integral = __is_integral(_Tp);

// floating_point：浮点类型
template<typename _Tp>
concept floating_point = __is_floating_point(_Tp);

// signed_integral：有符号整数类型
template<typename _Tp>
concept signed_integral = integral<_Tp> && __is_signed(_Tp);

// unsigned_integral：无符号整数类型
template<typename _Tp>
concept unsigned_integral = integral<_Tp> && !__is_signed(_Tp);

// movable：可移动类型
template<typename _Tp>
concept movable = move_constructible<_Tp> && assignable_from<_Tp&, _Tp> && swappable<_Tp>;

// copyable：可拷贝类型
template<typename _Tp>
concept copyable = copy_constructible<_Tp> && assignable_from<_Tp&, const _Tp&>;

// semiregular：半正则类型
template<typename _Tp>
concept semiregular = copyable<_Tp> && default_initializable<_Tp>;

// regular：正则类型
template<typename _Tp>
concept regular = semiregular<_Tp> && equality_comparable<_Tp>;
```

### 2.2 GCC (libstdc++) 的实现

```cpp
// GCC 使用 __is_same_as, __is_convertible 等内建函数

template<typename T, typename U>
concept same_as = __is_same_as(T, U);

template<typename From, typename To>
concept convertible_to = __is_convertible(From, To);
```

---

## 三、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ requires 表达式        │ 完整                 │ 完整                 │
│ concept 定义           │ 完整                 │ 完整                 │
│ subsumption            │ 完整                 │ 完整                 │
│ same_as                │ 编译器内建           │ 编译器内建           │
│ convertible_to         │ 编译器内建           │ 编译器内建           │
│ derived_from           │ 编译器内建           │ 编译器内建           │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 四、最佳实践

```
Concepts 使用指南：

1. 优先使用标准 concepts：
   · same_as, convertible_to, derived_from
   · integral, floating_point, signed_integral

2. 定义清晰的 concept：
   · 名字表达意图
   · 约束最小化
   · 组合现有 concepts

3. 使用 requires 子句：
   template<typename T>
   concept Sortable = requires(T a, T b) {
       { a < b } -> boolean_testable;
   };

4. 使用 abbreviated function templates：
   void process(Sortable auto& v);
```

---

## 延伸阅读

- [SFINAE 与 enable_if](/internals/templates/sfinae) — 传统模板约束
- [Type Traits 实现](/internals/templates/type-traits) — 类型属性查询
- [排序算法实现](/internals/algorithms/sorting) — concepts 在算法中的应用
