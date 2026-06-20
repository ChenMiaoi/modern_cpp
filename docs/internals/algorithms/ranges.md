---
title: "Ranges 框架实现分析"
topic: internals
feature: ranges
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/bits/ranges_base.h"
source_llvm: "references/impl/llvm-project/libcxx/include/__ranges/"
---

# Ranges 框架实现分析

> Ranges 是 C++20 引入的范围库，提供惰性求值的视图和组合操作。本文基于 GCC 和 LLVM 的源码，分析 Ranges 框架的内部实现。

---

## 一、核心概念

### 1.1 什么是 Ranges

Ranges 是对迭代器和容器的抽象，提供更简洁、更安全的范围操作：

```cpp
// 传统迭代器方式
std::vector<int> v = {1, 2, 3, 4, 5};
std::vector<int> result;
for (auto it = v.begin(); it != v.end(); ++it) {
    if (*it % 2 == 0) {
        result.push_back(*it * 2);
    }
}

// Ranges 方式
auto result = v | std::views::filter([](int x) { return x % 2 == 0; })
                | std::views::transform([](int x) { return x * 2; });
```

### 1.2 Ranges 的优势

```
Ranges 的优势：

1. 惰性求值：
   · 不创建中间容器
   · 只在需要时计算
   · 更少的内存分配

2. 组合性：
   · 使用管道运算符组合操作
   · 声明式编程风格
   · 更易于理解

3. 安全性：
   · 编译期检查范围有效性
   · 避免迭代器失效
   · 更好的错误信息
```

---

## 二、核心数据结构

### 2.1 range 概念（源码分析）

```cpp
// 源码路径：references/impl/llvm-project/libcxx/include/__ranges/concepts.h

// range 概念的定义
template<class T>
concept range = requires(T& t) {
    ranges::begin(t);  // 必须有 begin
    ranges::end(t);    // 必须有 end
};

// borrowed_range：不会悬垂的范围
template<class T>
concept borrowed_range = range<T> && is_reference_v<T>;

// sized_range：已知大小的范围
template<class T>
concept sized_range = range<T> && requires(T& t) {
    ranges::size(t);
};

// output_range：可以输出的范围
template<class T, class U>
concept output_range = range<T> && output_iterator<iterator_t<T>, U>;
```

### 2.2 view 概念（源码分析）

```cpp
// 源码路径：references/impl/llvm-project/libcxx/include/__ranges/view_interface.h

// view 概念的定义
template<class T>
concept view = ranges::range<T>
    && movable<T>
    && !ranges::enable_borrowed_range<remove_cvref_t<T>>
    && requires(T t) {
        { ranges::size(t) } -> same_as<range_size_t<T>>;
    };

// enable_view：启用 view 支持
template<class T>
inline constexpr bool enable_view = derived_from<T, view_base>;

// view_base：view 的基类
struct view_base { };
```

### 2.3 view_interface（源码分析）

```cpp
// 源码路径：references/impl/llvm-project/libcxx/include/__ranges/view_interface.h

// view_interface 提供通用操作
template<class _Derived>
class view_interface {
public:
    // 检查是否为空
    constexpr bool empty() {
        return ranges::begin(static_cast<_Derived&>(*this)) ==
               ranges::end(static_cast<_Derived&>(*this));
    }
    
    // 显式转换为 bool
    constexpr explicit operator bool() {
        return !empty();
    }
    
    // 获取第一个元素
    constexpr decltype(auto) front() {
        return *ranges::begin(static_cast<_Derived&>(*this));
    }
    
    // 获取最后一个元素
    constexpr decltype(auto) back() {
        auto tmp = static_cast<_Derived&>(*this);
        return *ranges::prev(ranges::end(tmp));
    }
    
    // 获取大小
    constexpr auto size() {
        auto& __self = static_cast<_Derived&>(*this);
        auto __diff = ranges::end(__self) - ranges::begin(__self);
        return static_cast<range_size_t<_Derived>>(__diff);
    }
    
    // 下标访问
    constexpr decltype(auto) operator[](range_difference_t<_Derived> __idx) {
        return ranges::begin(static_cast<_Derived&>(*this))[__idx];
    }
};
```

---

## 三、GCC (libstdc++) 的实现

### 3.1 view_interface

```cpp
// GCC 的 view_interface 提供通用操作
template<typename _Tp>
class view_interface {
public:
    constexpr bool empty() {
        return ranges::begin(static_cast<_Tp&>(*this)) ==
               ranges::end(static_cast<_Tp&>(*this));
    }
    
    constexpr explicit operator bool() {
        return !empty();
    }
    
    constexpr decltype(auto) front() {
        return *ranges::begin(static_cast<_Tp&>(*this));
    }
    
    constexpr decltype(auto) back() {
        auto tmp = static_cast<_Tp&>(*this);
        return *ranges::prev(ranges::end(tmp));
    }
};
```

### 3.2 filter_view

```cpp
// GCC 的 filter_view 实现
template<ranges::view _Vp, typename _Predicate>
class filter_view : public view_interface<filter_view<_Vp, _Predicate>> {
    _Vp __base_ = _Vp();
    _Predicate __pred_ = _Predicate();
    
    // 迭代器
    class __iterator {
        filter_view* __parent_;
        ranges::iterator_t<_Vp> __current_;
        
        void __satisfy() {
            while (__current_ != ranges::end(__parent_->__base_) &&
                   !std::invoke(__parent_->__pred_, *__current_)) {
                ++__current_;
            }
        }
    };
};
```

---

## 四、LLVM (libc++) 的实现

### 4.1 view_interface

LLVM 也使用类似的 view_interface：

```cpp
// LLVM 的 view_interface
template <class _Derived>
class view_interface {
public:
    constexpr bool empty() {
        return ranges::begin(static_cast<_Derived&>(*this)) ==
               ranges::end(static_cast<_Derived&>(*this));
    }
    
    constexpr explicit operator bool() {
        return !empty();
    }
};
```

### 4.2 transform_view

```cpp
// LLVM 的 transform_view 实现
template <view _Vp, __decayed_invocable<_Functor> _Functor>
class transform_view : public view_interface<transform_view<_Vp, _Functor>> {
    _Vp __base_ = _Vp();
    _Functor __func_ = _Functor();
    
    // 迭代器
    class __iterator {
        transform_view* __parent_;
        ranges::iterator_t<_Vp> __current_;
        
        // 转换函数
        auto __visit_iter() {
            return std::invoke(__parent_->__func_, *__current_);
        }
    };
};
```

---

## 五、管道运算符

### 5.1 view_adaptor

```cpp
// 管道运算符的实现
template<typename _Tp>
struct __range_adaptor_closure {
    // 支持管道操作
    template<ranges::viewable_range _R>
    constexpr auto operator|(_R&& __r) {
        return std::forward<_R>(__r) | std::declval<_Tp&>();
    }
};
```

### 5.2 组合操作

```
管道运算符的执行流程：

auto result = v | views::filter(pred) | views::transform(func);

执行过程：
1. views::filter(pred) 返回一个闭包对象
2. v | filter(pred) 调用闭包的 operator|
3. 返回一个 filter_view
4. filter_view | transform(func) 调用另一个闭包
5. 返回一个 transform_view
6. 最终结果是一个懒求值的视图
```

---

## 六、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ view_interface         │ 完整                 │ 完整                 │
│ filter_view            │ 支持                 │ 支持                 │
│ transform_view         │ 支持                 │ 支持                 │
│ take_view              │ 支持                 │ 支持                 │
│ drop_view              │ 支持                 │ 支持                 │
│ zip_view (C++23)       │ 支持                 │ 支持                 │
│ enumerate_view (C++23) │ 支持                 │ 支持                 │
│ views::all             │ 支持                 │ 支持                 │
│ ranges::to             │ C++23                │ C++23                │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 七、性能特征

```
Ranges 的性能：

编译期开销：
  · 模板实例化可能增加编译时间
  · 但运行时性能更好

运行时性能：
  · 惰性求值：避免不必要的计算
  · 无中间容器：减少内存分配
  · 内联优化：编译器可以内联视图操作

与传统迭代器对比：
  · 相同的运行时性能
  · 更好的可读性
  · 更安全的代码

使用建议：
  · 优先使用 ranges 而不是原始迭代器
  · 对于性能关键代码，考虑手动优化
  · 使用 views::all 包装容器以避免拷贝
```

---

## 延伸阅读

- [排序算法实现](/internals/algorithms/sorting) — ranges::sort 的实现
- [迭代器体系](/internals/algorithms/iterators) — 迭代器的概念
- [std::vector 实现](/internals/containers/vector) — vector 与 ranges 的交互
