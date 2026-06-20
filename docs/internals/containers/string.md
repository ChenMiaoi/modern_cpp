---
title: "std::string 实现分析"
topic: internals
feature: string
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/bits/basic_string.h"
source_llvm: "references/impl/llvm-project/libcxx/include/string"
---

# std::string 实现分析

> `std::string` 是 C++ 中使用最频繁的容器之一，其实现涉及 SSO（Small String Optimization）、内存分配、ABI 兼容性等多个复杂话题。本文基于 GCC (libstdc++) 和 LLVM (libc++) 的源码，深入分析 string 的内部实现。

---

## 一、核心数据结构

### 1.1 GCC (libstdc++) 的 string 布局

GCC 使用经典的**指针+大小+容量**布局：

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/basic_string.h

// 当 _GLIBCXX_USE_CXX11_ABI=1 时（默认）
struct _Alloc_hider : allocator_type {
    pointer _M_p;  // 指向实际存储
};

// basic_string 的数据成员
_Alloc_hider _M_dataplus;                // 指针 + allocator (EBO)
size_type _M_string_length;              // 字符串长度
union {
    char _M_local_buf[_S_local_capacity]; // SSO 缓冲区
    size_type _M_allocated_capacity;      // 堆模式下的容量
};
```

```
GCC string 的内存布局（64位系统）：

SSO 模式（短字符串）：
┌─────────────────────────────────────────────────────────────┐
│ _M_dataplus._M_p ───────────────────────────────────────────→│
│ _M_string_length: 5                                        │
│ _M_local_buf: "Hello\0..........."                          │
└─────────────────────────────────────────────────────────────┘
sizeof(string) = 32 字节

堆模式（长字符串）：
┌─────────────────────────────────────────────────────────────┐
│ _M_dataplus._M_p ───────────────────────────────────────────→│
│ _M_string_length: 1000                                     │
│ _M_allocated_capacity: 1024                                │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 LLVM (libc++) 的 string 布局

LLVM 使用 **compressed_pair** 和 union 存储：

```cpp
// 源码路径：references/impl/llvm-project/libcxx/include/string

// LLVM 的 string 使用 union 存储两种模式
struct __long {
    pointer __data_;      // 指向堆上数据
    size_type __cap_;     // 容量
    size_type __size_;    // 长度
};

struct __short {
    union {
        unsigned char __size_;   // 长度（低 7 位）
        value_type __lx_;        // 首字符
    };
    value_type __data_[__min_cap];  // SSO 缓冲区
};

compressed_pair<size_type, __storage> __r_;
```

```
LLVM string 的内存布局（64位系统）：

SSO 模式（短字符串）：
┌─────────────────────────────────────────────────────────────┐
│ __r_.__first_: size_ (包含 length + short flag)             │
│ __r_.__second_.__s_.__data_: "Hello\0..........."          │
└─────────────────────────────────────────────────────────────┘
sizeof(string) = 24 字节

堆模式（长字符串）：
┌─────────────────────────────────────────────────────────────┐
│ __r_.__first_: size_ (包含 length + long flag)              │
│ __r_.__second_.__l_.__data_ ────────────────────────────────→│
│ __r_.__second_.__l_.__cap_: 1024                           │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、SSO（Small String Optimization）

### 2.1 什么是 SSO

SSO 是一种优化技术，将短字符串直接存储在 string 对象内部，避免堆分配：

```
SSO 的优势：
  · 避免堆分配（短字符串场景）
  · 更好的缓存局部性
  · 更快的构造/析构
  · 更小的内存开销

SSO 的限制：
  · 只适用于短字符串
  · 字符串长度有限制
  · 不能与自定义分配器一起使用
```

### 2.2 GCC 的 SSO 实现

```cpp
// GCC 的 SSO 缓冲区大小
static const size_type _S_local_capacity = 15 / sizeof(_CharT);

// 对于 char，SSO 缓冲区大小 = 15 字节
// 可以存储最多 15 个字符（包括 null 终止符）
```

```
GCC SSO 模式的判断：

if (指针指向对象内部) {
    // SSO 模式：数据在 _M_local_buf 中
    return _M_local_buf;
} else {
    // 堆模式：数据在堆上
    return _M_dataplus._M_p;
}
```

### 2.3 LLVM 的 SSO 实现

```cpp
// LLVM 的 SSO 缓冲区大小
static const size_type __min_cap = (sizeof(__short) - 1) / sizeof(value_type);

// 对于 char，SSO 缓冲区大小 = 22 字节
// 可以存储最多 22 个字符
```

```
LLVM SSO 模式的判断：

if (size 的最低位 == 1) {
    // SSO 模式：数据在 __short.__data_ 中
    size >>= 1;  // 获取真实长度
    return __data_;
} else {
    // 堆模式：数据在堆上
    return __data_;
}
```

### 2.4 SSO 容量对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ SSO 缓冲区大小         │ 15 字节              │ 22 字节              │
│ 可存储字符数           │ 15 个 char           │ 22 个 char           │
│ sizeof(string)         │ 32 字节              │ 24 字节              │
│ 判断方式               │ 指针比较             │ 位标志               │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 三、COW（Copy-On-Write）

### 3.1 GCC 的旧 COW 实现

GCC 5 之前使用 COW（写时复制）：

```cpp
// 旧 ABI（_GLIBCXX_USE_CXX11_ABI=0）
// string 使用引用计数的 COW 策略

// 构造时
string s1 = "hello";  // 引用计数 = 1
string s2 = s1;       // 引用计数 = 2（共享数据）

// 修改时触发 COW
s2[0] = 'H';          // 分离：s2 获得独立副本
                       // 引用计数：s1=1, s2=1
```

### 3.2 COW 的问题

```
COW 的问题：

1. 线程不安全：
   · 引用计数的原子操作有开销
   · COW 触发时需要同步

2. 性能问题：
   · 每次修改都需要检查引用计数
   · 迭代器失效规则复杂
   · 异常安全性问题

3. ABI 兼容性：
   · COW string 和 SSO string 布局不同
   · 不能混合使用
```

### 3.3 新 ABI 的改进

```cpp
// 新 ABI（_GLIBCXX_USE_CXX11_ABI=1，默认）
// 使用 SSO 替代 COW

// 优势：
// · 线程安全（每个 string 独立）
// · 性能更好（短字符串无需堆分配）
// · 异常安全更强
```

---

## 四、字符串操作的实现

### 4.1 push_back 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/basic_string.h:1778

void push_back(_CharT __c) {
    const size_type __size = this->size();
    if (__size + 1 > this->capacity()) {
        // 容量不足，需要扩容
        // _M_mutate(__pos, __len1, __s, __len2) 会：
        // 1. 计算新容量（通常 2 倍）
        // 2. 分配新内存
        // 3. 复制 [0, __pos) 到新内存
        // 4. 复制 __s 到新内存
        // 5. 复制 [__pos + __len1, end) 到新内存
        // 6. 释放旧内存
        this->_M_mutate(__size, size_type(0), 0, size_type(1));
    }
    // 在末尾添加字符
    traits_type::assign(this->_M_data()[__size], __c);
    this->_M_set_length(__size + 1);
}
```

### 4.2 append 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/basic_string.h

// append(const string& str)
basic_string& append(const basic_string& __str) {
    return this->append(__str._M_data(), __str.size());
}

// append(const char* s, size_type n)
basic_string& append(const _CharT* __s, size_type __n) {
    __glibcxx_requires_string_len(__s, __n);
    _M_check_length(size_type(0), __n, "basic_string::append");
    const size_type __len = __n + this->size();
    if (__len > this->capacity() || _M_rep()->_M_is_shared()) {
        // 需要扩容或分离共享数据
        _M_mutate(this->size(), size_type(0), __s, __n);
    } else {
        // 直接在末尾追加
        traits_type::copy(this->_M_data() + this->size(), __s, __n);
        _M_set_length(__len);
    }
    return *this;
}
```

### 4.3 _M_mutate 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/basic_string.tcc

void basic_string::_M_mutate(size_type __pos, size_type __len1, 
                             const _CharT* __s, size_type __len2) {
    // 1. 计算新容量
    const size_type __old_capacity = this->capacity();
    const size_type __new_size = this->size() + __len2 - __len1;
    size_type __new_capacity = _M_next_capacity(__new_size);
    
    // 2. 分配新内存
    pointer __new_data = _S_allocate_at_least(_M_get_allocator(), __new_capacity + 1);
    
    // 3. 复制数据
    // 复制 [0, __pos)
    traits_type::copy(__new_data, _M_data(), __pos);
    // 复制 __s（要插入的内容）
    if (__s)
        traits_type::copy(__new_data + __pos, __s, __len2);
    // 复制 [__pos + __len1, end)
    traits_type::copy(__new_data + __pos + __len2,
                      _M_data() + __pos + __len1,
                      this->size() - __pos - __len1 + 1);  // +1 for null terminator
    
    // 4. 释放旧内存（如果不是本地存储）
    if (!_M_is_local())
        _M_rep()->_M_dispose(_M_get_allocator());
    
    // 5. 更新指针
    _M_data(__new_data);
    _M_capacity(__new_capacity);
    _M_set_length(__new_size);
}
```

---

## 五、异常安全

### 5.1 strong exception guarantee

string 的大多数操作提供**强异常保证**：

```
异常安全的实现：

1. 操作前检查容量：
   if (需要扩容) {
       // 先分配新内存
       // 在新内存中操作（如果抛异常，旧数据未被修改）
       // 成功后交换
   }

2. COW 模式下的异常安全：
   if (引用计数 > 1) {
       // 先分离（copy-on-write）
       // 然后修改
   }
```

### 5.2 LLVM 的异常安全

```cpp
// LLVM 使用 exception_guard 确保异常安全
auto __guard = std::__make_exception_guard(__destroy_vector(*this));
if (__n > 0) {
    __vallocate(__n);
    __construct_at_end(__n);
}
__guard.__complete();  // 成功后取消 guard
```

---

## 六、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ sizeof(string)         │ 32 字节              │ 24 字节              │
│ SSO 缓冲区大小         │ 15 字节              │ 22 字节              │
│ 旧 ABI (COW)           │ 支持                 │ 不支持               │
│ 新 ABI (SSO)           │ 默认                 │ 默认                 │
│ allocator 压缩         │ EBO                  │ compressed_pair      │
│ 判断 SSO 的方式        │ 指针比较             │ 位标志               │
│ constexpr string       │ C++20                │ C++20                │
│ trivially_relocatable  │ 不支持               │ 支持                 │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 七、字符串 ABI 兼容性

### 7.1 libstdc++ 双 ABI

```cpp
// GCC 5 引入了 __cxx11 命名空间来处理 ABI 不兼容

// 旧 ABI（COW）：
// _ZNSsC1Ev → std::string::string()

// 新 ABI（SSO）：
// _ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEC1Ev
//           ↑ 关键区别：__cxx11 命名空间段

// 编译时选择：
// -D_GLIBCXX_USE_CXX11_ABI=1  → 新 ABI（默认）
// -D_GLIBCXX_USE_CXX11_ABI=0  → 旧 ABI
```

### 7.2 libc++ 内联命名空间

```cpp
// libc++ 使用内联命名空间实现 ABI 版本化

namespace std {
    inline namespace __1 {
        template<class CharT, class Traits, class Allocator>
        class basic_string;
    }
}

// 版本化：
// _LIBCPP_ABI_VERSION 1 → inline namespace __1（当前默认）
// _LIBCPP_ABI_VERSION 2 → inline namespace __2（实验性）
```

---

## 八、性能优化建议

```
string 性能优化指南：

1. 预分配空间：
   s.reserve(n);  // 避免多次扩容

2. 避免不必要的拷贝：
   · 使用 string_view 作为函数参数
   · 使用 move 语义
   · 使用 string 的视图操作

3. 使用 emplace 操作：
   s.insert(s.begin() + pos, count, ch);  // 比 insert + 循环快

4. 使用 SSO：
   · 短字符串优先使用 string
   · 避免不必要的堆分配

5. 使用 make_shared/shared_ptr 避免拷贝：
   · 传递 string 时使用 const string& 或 string_view
```

---

## 延伸阅读

- [std::vector 实现](/internals/containers/vector) — 另一个关键容器的实现
- [字符串 ABI 对比](/internals/comparison/string-abi) — libstdc++ vs libc++ 的差异
- [分配器模型](/internals/memory/allocator) — allocator 如何与 string 交互
- [std::string_view 实现](/standards/cpp17/string-view) — string_view 的设计
