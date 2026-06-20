---
title: "std::variant 实现分析"
topic: internals
feature: variant
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/std/variant"
source_llvm: "references/impl/llvm-project/libcxx/include/variant"
---

# std::variant 实现分析

> `std::variant` 是 C++17 引入的标签联合（Tagged Union），可以在类型安全的方式下存储多种类型之一。本文基于 GCC 和 LLVM 的源码，分析 variant 的内部实现。

---

## 一、核心概念

### 1.1 什么是 variant

variant 是一种类型安全的联合体，可以在运行时存储多种类型中的一种：

```cpp
// variant 的基本使用
variant<int, double, string> v;
v = 42;           // 存储 int
v = 3.14;         // 存储 double
v = "hello";      // 存储 string

// 访问
cout << get<int>(v);        // 42
cout << get<double>(v);     // 3.14
cout << get<string>(v);     // "hello"

// 错误处理
get<int>(v);  // 如果 v 存储的是 double，抛 bad_variant_access
```

### 1.2 与 union 的区别

```
variant vs union：

union：
  · 不是类型安全的
  · 不知道当前存储的是哪种类型
  · 需要手动管理构造/析构
  · 可能产生未定义行为

variant：
  · 类型安全
  · 知道当前存储的类型（index()）
  · 自动管理构造/析构
  · 不会产生未定义行为
```

---

## 二、核心数据结构

### 2.1 存储布局

variant 的存储布局通常包含：
1. **标签（tag）**：记录当前存储的类型索引
2. **数据存储（storage）**：存储实际的值

```
variant<int, double, string> 的内存布局：

┌─────────────────────────────────────┐
│ index (4 字节)                       │  ← 当前类型索引（0, 1, 2）
├─────────────────────────────────────┤
│ storage (对齐到最大类型的大小)       │
│   ┌─────────────────────────────┐   │
│   │ 占用空间 = max(sizeof(int), │   │
│   │   sizeof(double),           │   │
│   │   sizeof(string))           │   │
│   └─────────────────────────────┘   │
└─────────────────────────────────────┘

对于 variant<int, double, string>：
  · index: 4 字节
  · storage: 32 字节（sizeof(string) = 32）
  · 总大小: 36 字节（+ 填充到 8 字节对齐 = 40 字节）
```

### 2.2 GCC (libstdc++) 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/std/variant

// GCC 使用 __aligned_membuf 存储数据
template<typename... _Types>
class variant {
    // 存储标签索引
    size_t _M_index;
    
    // 对齐存储空间
    __aligned_membuf<...> _M_storage;
    
    // 获取当前类型的指针
    template<size_t _Np>
    constexpr auto* _M_get() noexcept {
        return reinterpret_cast<
            variant_alternative_t<_Np, variant>*>(
            _M_storage._M_addr());
    }
    
    // 构造指定类型的值
    template<size_t _Np, typename _Tp, typename... _Args>
    constexpr void _M_emplace(_Args&&... __args) {
        // 销毁当前值（如果存在）
        if (_M_index != variant_npos) {
            std::destroy_at(_M_get<_M_index>());
        }
        // 构造新值
        ::new (_M_storage._M_addr()) _Tp(std::forward<_Args>(__args)...);
        _M_index = _Np;
    }
    
    // 交换两个 variant
    constexpr void _M_swap_common(variant& __rhs) {
        if (_M_index == __rhs._M_index) {
            // 类型相同：交换值
            using std::swap;
            swap(*_M_get<_M_index>(), *__rhs._M_get<_M_index>());
        } else {
            // 类型不同：移动构造
            variant __tmp = std::move(__rhs);
            __rhs = std::move(*this);
            *this = std::move(__tmp);
        }
    }
};
```

### 2.3 visit 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/std/variant

// visit 的实现使用 jump table
template<typename _Result, typename _Visitor, typename... _Variants>
constexpr decltype(auto)
__do_visit(_Visitor&& __visitor, _Variants&&... __variants) {
    // 获取 index
    size_t __index = __variants.index()...;
    
    // 使用 jump table 调用对应的访问函数
    using _Func = _Result_type(*)(_Visitor&&, _Variants&&...);
    static constexpr _Func __table[] = {
        &__visit_invoke<_Visitor, _Variants..., _Is>...
    };
    return __table[__index](std::forward<_Visitor>(__visitor),
                            std::forward<_Variants>(__variants)...);
}
```

### 2.3 LLVM (libc++) 的实现

```cpp
// LLVM 使用 __variant_internal:: __union 存储数据

template<class... _Types>
class variant {
    __variant_detail:: __union<_Types...> __data_;
    size_t __index_;
};
```

---

## 三、visit 机制

### 3.1 什么是 visit

visit 是一种访问 variant 中值的机制，通过访问者模式实现类型安全的多态访问：

```cpp
// visit 的基本使用
variant<int, double, string> v = 42;

visit(overloaded{
    [](int i) { cout << "int: " << i; },
    [](double d) { cout << "double: " << d; },
    [](const string& s) { cout << "string: " << s; }
}, v);
```

### 3.2 visit 的实现

visit 的实现通常使用**访问者表（visitor table）**：

```
visit 的实现原理：

1. 编译期生成访问者表
   visitor_table[index] → 对应类型的访问函数

2. 运行时通过 index 索引访问者表
   visitor_table[variant.index()](variant)

3. 间接调用访问函数
   调用具体的 lambda/函数对象
```

### 3.3 GCC 的实现

GCC 使用 **jump table** 实现 visit：

```cpp
// GCC 的 visit 实现
template<typename _Result, typename _Visitor, typename... _Variants>
constexpr decltype(auto)
__do_visit(_Visitor&& __visitor, _Variants&&... __variants) {
    // 获取 index
    size_t __index = __variants.index()...;
    
    // 通过 jump table 调用
    using _Func = _Result_type(*)(_Visitor&&, _Variants&&...);
    static constexpr _Func __table[] = {
        &__visit_invoke<_Visitor, _Variants..., _Is>...
    };
    return __table[__index](std::forward<_Visitor>(__visitor),
                            std::forward<_Variants>(__variants)...);
}
```

### 3.4 LLVM 的实现

LLVM 使用类似的 **jump table** 机制：

```cpp
// LLVM 的 visit 实现
template<class _Visitor, class... _Variants>
constexpr decltype(auto) visit(_Visitor&& __visitor, _Variants&&... __variants) {
    // 计算 index
    // 通过 jump table 调用
    return __variant::__visit2(
        __variant::__overloaded{std::forward<_Visitor>(__visitor)},
        std::forward<_Variants>(__variants)...);
}
```

---

## 四、异常安全

### 4.1 构造时的异常安全

```
variant 构造时的异常安全：

1. 如果新类型的构造函数不抛异常
   · 直接构造
   · 无额外开销

2. 如果新类型的构造函数可能抛异常
   · 先构造临时对象
   · 成功后移动到 storage
   · 失败时保持原状态

3. 异常安全保证
   · 强异常保证（如果移动构造函数是 noexcept）
   · 基本异常保证（其他情况）
```

---

## 五、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ 存储实现               │ __aligned_membuf     │ __union              │
│ 标签位置               │ 独立成员             │ 独立成员             │
│ visit 实现             │ jump table           │ jump table           │
│ constexpr 支持         │ C++20                │ C++20                │
│ valueless_by_exception │ 支持                 │ 支持                 │
│ monostate              │ 支持                 │ 支持                 │
│ variant_npos           │ -1                   │ -1                   │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 六、性能特征

```
variant 的性能：

构造：
  · O(1)（移动构造）
  · O(1)（拷贝构造）

析构：
  · O(1)

visit：
  · O(1)（jump table 查找 + 间接调用）

get：
  · O(1)（index 比较 + 强制转换）

内存开销：
  · sizeof(variant) = sizeof(index) + sizeof(storage) + 填充
  · 比 union 多一个 index

与 std::any 对比：
  · variant：编译期已知类型，无堆分配
  · any：运行时类型擦除，可能堆分配
  · variant 更快，any 更灵活
```

---

## 延伸阅读

- [std::any 实现](/internals/utilities/any) — 运行时类型擦除
- [std::optional 实现](/internals/utilities/optional) — 可选值
- [std::function 实现](/internals/utilities/function) — 类型擦除的可调用对象
