---
title: "std::function 实现分析"
topic: internals
feature: function
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/bits/std_function.h"
source_llvm: "references/impl/llvm-project/libcxx/include/__functional/function.h"
---

# std::function 实现分析

> `std::function` 是 C++ 中最常用的类型擦除工具，可以存储任何可调用对象。本文基于 GCC 和 LLVM 的源码，分析 function 的内部实现。

---

## 一、核心概念：类型擦除

### 1.1 什么是类型擦除

类型擦除是一种技术，允许在运行时处理不同类型的具体对象，而不需要知道它们的具体类型：

```
类型擦除的实现方式：

1. 虚函数（传统的多态）
   · 基类定义虚函数接口
   · 派生类实现具体类型
   · 运行时通过 vptr 分发

2. 函数指针（std::function 的方式）
   · 存储函数指针表
   · 每个操作对应一个函数指针
   · 运行时通过函数指针分发

3. 模板（编译期多态）
   · 不是真正的类型擦除
   · 编译期确定类型
```

### 1.2 std::function 的设计

```
std::function 的核心组件：

1. 存储空间（_Any_data）
   · 小对象优化（SBO）：直接存储在 function 对象内部
   · 大对象：堆上分配，存储指针

2. 管理器（_Manager）
   · 创建：构造可调用对象
   · 销毁：析构可调用对象
   · 克隆：拷贝可调用对象
   · 类型查询：获取 type_info

3. 调用器（_Invoker）
   · 调用可调用对象
   · 处理参数转发
```

---

## 二、GCC (libstdc++) 的实现

### 2.1 存储空间（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/std_function.h:77

// 用于存储不可拷贝的类型
union _Nocopy_types {
    void*       _M_object;           // 指向堆上对象
    const void* _M_const_object;     // 指向 const 对象
    void (*_M_function_pointer)();    // 函数指针
    void (_Undefined_class::*_M_member_pointer)();  // 成员函数指针
};

// 原始存储空间（用于小对象优化）
union _Any_data {
    void* _M_access() noexcept { return &_M_pod_data[0]; }
    const void* _M_access() const noexcept { return &_M_pod_data[0]; }
    
    template<typename _Tp>
    _Tp& _M_access() noexcept { return *static_cast<_Tp*>(_M_access()); }
    
    template<typename _Tp>
    const _Tp& _M_access() const noexcept { return *static_cast<const _Tp*>(_M_access()); }
    
    _Nocopy_types _M_unused;
    char _M_pod_data[sizeof(_Nocopy_types)];  // 原始存储空间
};

// 管理器操作枚举
enum _Manager_operation {
    __get_type_info,      // 获取 type_info
    __get_functor_ptr,    // 获取可调用对象指针
    __clone_functor,      // 克隆可调用对象
    __destroy_functor     // 销毁可调用对象
};
```

### 2.2 小对象优化（SBO）（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/std_function.h:122

// 判断是否可以使用小对象优化
template<typename _Functor>
class _Base_manager {
    // 判断是否可以存储在本地（小对象优化）
    static const bool __stored_locally =
        (__is_location_invariant<_Functor>::value  // 位置不变性
         && sizeof(_Functor) <= _M_max_size        // 大小限制
         && __alignof__(_Functor) <= _M_max_align  // 对齐限制
         && (_M_max_align % __alignof__(_Functor) == 0));  // 对齐兼容

    // 本地存储类型
    using _Local_storage = integral_constant<bool, __stored_locally>;

    // 获取可调用对象指针
    static _Functor* _M_get_pointer(const _Any_data& __source) noexcept {
        if constexpr (__stored_locally) {
            // 小对象：直接从存储空间获取
            const _Functor& __f = __source._M_access<_Functor>();
            return const_cast<_Functor*>(std::__addressof(__f));
        } else {
            // 大对象：从指针获取
            return __source._M_access<_Functor*>();
        }
    }

    // 创建可调用对象（小对象版本）
    template<typename _Fn>
    static void _M_create(_Any_data& __dest, _Fn&& __f, true_type) {
        ::new (__dest._M_access()) _Functor(std::forward<_Fn>(__f));
    }

    // 创建可调用对象（大对象版本）
    template<typename _Fn>
    static void _M_create(_Any_data& __dest, _Fn&& __f, false_type) {
        __dest._M_access<_Functor*>() = new _Functor(std::forward<_Fn>(__f));
    }

    // 销毁可调用对象（小对象版本）
    static void _M_destroy(_Any_data& __victim, true_type) {
        __victim._M_access<_Functor>().~_Functor();
    }

    // 销毁可调用对象（大对象版本）
    static void _M_destroy(_Any_data& __victim, false_type) {
        delete __victim._M_access<_Functor*>();
    }
};
```

```
小对象优化的条件：

1. 位置不变性（__is_location_invariant）
   · trivially copyable 的类型
   · 或用户特化的类型

2. 大小限制
   · sizeof(Functor) <= sizeof(_Nocopy_types)
   · 通常 = sizeof(void*) × 4 = 32 字节（64位系统）

3. 对齐限制
   · alignof(Functor) <= alignof(_Nocopy_types)
   · 且对齐兼容

满足条件：
  · 直接存储在 _Any_data 中（栈上）
  · 无堆分配

不满足条件：
  · 在堆上分配
  · 存储指针到 _Any_data
```

### 2.3 管理器操作

```cpp
// 管理器操作枚举
enum _Manager_operation {
    __get_type_info,      // 获取 type_info
    __get_functor_ptr,    // 获取可调用对象指针
    __clone_functor,      // 克隆可调用对象
    __destroy_functor     // 销毁可调用对象
};

// 管理器函数
static bool _M_manager(_Any_data& __dest, const _Any_data& __source,
                       _Manager_operation __op) {
    switch (__op) {
    case __get_type_info:
        __dest._M_access<const type_info*>() = &typeid(_Functor);
        break;
    case __get_functor_ptr:
        __dest._M_access<_Functor*>() = _M_get_pointer(__source);
        break;
    case __clone_functor:
        _M_init_functor(__dest, *_M_get_pointer(__source));
        break;
    case __destroy_functor:
        _M_destroy(__dest, _Local_storage());
        break;
    }
    return false;
}
```

---

## 三、LLVM (libc++) 的实现

### 3.1 __value_func 结构

LLVM 使用 `__value_func` 实现小对象优化：

```cpp
// 源码路径：references/impl/llvm-project/libcxx/include/__functional/function.h

template <class _Sig>
class __value_func {
    static constexpr size_t __min_size = 3 * sizeof(void*);
    
    struct __base {
        virtual __base* __clone(void*) const = 0;
        virtual void destroy() = 0;
        virtual _LIBCPP_NO_CFI _Rp operator()(_ArgTypes&&...) const = 0;
        virtual const std::type_info& __target_type() const = 0;
    };
    
    template <class _FF>
    struct __func : __base {
        _FF __f_;
        // ...
    };
    
    union __storage {
        alignas(__alignof(void*)) char __buf_[__min_size];
        void* __long;
    };
};
```

### 3.2 小对象优化

```
LLVM 的 SBO 策略：

__min_size = 3 × sizeof(void*) = 24 字节（64位系统）

小对象（sizeof ≤ 24 字节）：
  · 存储在 __buf_ 中
  · 无堆分配

大对象（sizeof > 24 字节）：
  · 堆上分配
  · 存储指针到 __long

与 GCC 的对比：
  GCC: sizeof(_Nocopy_types) = 32 字节
  LLVM: __min_size = 24 字节
  LLVM 的 SBO 阈值更小
```

---

## 四、调用机制

### 4.1 调用流程

```
std::function 的调用流程：

f(args...) 的执行过程：

1. 检查是否为空
   if (!f) throw bad_function_call();

2. 通过函数指针调用 invoke
   _M_invoker(_M_functor, args...)

3. invoke 内部：
   _Functor* ptr = _M_get_pointer(_M_functor);
   return (*ptr)(args...);
```

### 4.2 函数指针存储

```
std::function 的内存布局：

空 function：
┌─────────────────────────────────────┐
│ _M_functor (32 字节)                 │  ← 未使用
├─────────────────────────────────────┤
│ _M_manager = nullptr                 │  ← 空管理器
├─────────────────────────────────────┤
│ _M_invoker = nullptr                 │  ← 空调用器
└─────────────────────────────────────┘

有值 function（小对象）：
┌─────────────────────────────────────┐
│ _M_functor (32 字节)                 │  ← 直接存储可调用对象
├─────────────────────────────────────┤
│ _M_manager = &管理器函数             │
├─────────────────────────────────────┤
│ _M_invoker = &调用函数               │
└─────────────────────────────────────┘

有值 function（大对象）：
┌─────────────────────────────────────┐
│ _M_functor (32 字节)                 │  ← 存储指针
│   _M_object → 堆上可调用对象         │
├─────────────────────────────────────┤
│ _M_manager = &管理器函数             │
├─────────────────────────────────────┤
│ _M_invoker = &调用函数               │
└─────────────────────────────────────┘
```

---

## 五、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ 小对象阈值             │ 32 字节              │ 24 字节              │
│ 存储方式               │ _Any_data            │ __storage            │
│ 管理器                 │ _Manager_type 函数指针│ __base 虚函数        │
│ 调用器                 │ _M_invoker 函数指针  │ __base::operator()   │
│ 类型擦除方式           │ 函数指针表           │ 虚函数               │
│ sizeof(function)       │ 48 字节              │ 32 字节              │
│ no_unique_address      │ 不支持               │ 支持                 │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 六、性能特征

```
std::function 的性能：

构造：
  · 小对象：O(1)（拷贝到栈上）
  · 大对象：O(1)（堆分配 + 拷贝）

调用：
  · 间接调用（函数指针或虚函数）
  · 比直接调用慢 2-3 倍

移动：
  · O(1)（指针交换）

拷贝：
  · 小对象：O(1)（拷贝到栈上）
  · 大对象：O(1)（堆分配 + 拷贝）

与 lambda 对比：
  · lambda：零开销（编译器优化）
  · function：有开销（类型擦除 + 间接调用）
  · 能用 lambda 就不要用 function
```

---

## 延伸阅读

- [std::any 实现](/internals/utilities/any) — 另一个类型擦除工具
- [std::variant 实现](/internals/utilities/variant) — 标签联合
- [std::optional 实现](/internals/utilities/optional) — 可选值
