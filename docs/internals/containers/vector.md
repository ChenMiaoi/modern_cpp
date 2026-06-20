---
title: "std::vector 实现分析"
topic: internals
feature: vector
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/bits/stl_vector.h"
source_llvm: "references/impl/llvm-project/libcxx/include/__vector/vector.h"
---

# std::vector 实现分析

> `std::vector` 是 C++ 中最常用的容器，其实现看似简单——一个动态数组——但细节决定成败。本文基于 GCC (libstdc++) 和 LLVM (libc++) 的源码，深入分析 vector 的内部实现。

---

## 一、核心数据结构

### 1.1 GCC (libstdc++) 的 vector 布局

GCC 使用经典的**三指针**布局：

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/stl_vector.h

struct _Vector_impl_data {
    pointer _M_start;           // 指向第一个元素
    pointer _M_finish;          // 指向最后一个元素的下一个位置
    pointer _M_end_of_storage;  // 指向已分配存储的末尾
};

struct _Vector_impl
    : public _Tp_alloc_type,   // EBO：空 allocator 不占空间
      public _Vector_impl_data
{ };
```

```
GCC vector 的内存布局：

┌─────────────────────────────────────────────────────────────┐
│ _M_start ──────────────┐                                   │
│ _M_finish ─────────────┼────────┐                          │
│ _M_end_of_storage ─────┼────────┼────────┐                 │
└────────────────────────┼────────┼────────┼─────────────────┘
                         ▼        ▼        ▼
    ┌────────────────┬──────────┬────────────────────────┐
    │  元素 0        │  元素 1  │  未使用空间 (capacity) │
    └────────────────┴──────────┴────────────────────────┘
    ←── size() ────→  ←───── capacity() ─────────────────→

关键公式：
  size()     = _M_finish - _M_start
  capacity() = _M_end_of_storage - _M_start
  空闲空间   = capacity() - size()
```

### 1.2 LLVM (libc++) 的 vector 布局

LLVM 使用 **compressed_pair** 存储 allocator 和内部状态：

```cpp
// 源码路径：references/impl/llvm-project/libcxx/include/__vector/vector.h

template <class _Tp, class _Allocator>
class vector {
    using _SplitBuffer = std::__split_buffer<_Tp, _Allocator, __split_buffer_pointer_layout>;

    // 内部状态通过 _SplitBuffer 管理
    // allocator 通过 compressed_pair 与数据一起存储
};
```

LLVM 的 `__split_buffer`（用于 vector 的内部存储）使用指针布局：

```cpp
// 源码路径：references/impl/llvm-project/libcxx/include/__split_buffer

class __split_buffer_pointer_layout {
    pointer __front_cap_;  // 已分配内存的起始位置
    pointer __begin_;      // 第一个元素
    pointer __end_;        // 最后一个元素的下一个位置
    pointer __back_cap_;   // 已分配内存的末尾
    compressed_pair<allocator_type, pointer> __alloc_;  // allocator + 指针
};
```

```
LLVM vector 的内存布局：

┌─────────────────────────────────────────────────────────────┐
│ __begin_ ─────────────┐                                     │
│ __end_ ───────────────┼────────┐                            │
│ __back_cap_ ──────────┼────────┼────────┐                   │
└───────────────────────┼────────┼────────┼───────────────────┘
                        ▼        ▼        ▼
    ┌────────────────┬──────────┬────────────────────────┐
    │  元素 0        │  元素 1  │  未使用空间            │
    └────────────────┴──────────┴────────────────────────┘
```

### 1.3 对象大小对比

```
sizeof(vector<int>)：
  · GCC (libstdc++): 24 字节（3 个指针 × 8 字节）
  · LLVM (libc++):   24 字节（3 个指针 × 8 字节）
  · 空 allocator 通过 EBO 压缩为 0 字节

两者大小相同，但内部指针命名不同：
  GCC: _M_start, _M_finish, _M_end_of_storage
  LLVM: __begin_, __end_, __back_cap_
```

---

## 二、扩容策略

### 2.1 GCC 的扩容策略

GCC 使用 **1.5x 扩容因子**：

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/stl_vector.h

// _M_allocate_at_least 分配至少 __n 个元素的空间
// 具体策略：通常按 2 倍增长（但实现可能调整）
size_type _M_check_len(size_type __n, const char* __s) const {
    if (max_size() - size() < __n)
        __throw_length_error(__s);
    const size_type __len = size() + std::max(size(), __n);
    return (__len < size() || __len > max_size()) ? max_size() : __len;
}
```

```
GCC vector 的扩容过程：

初始状态：size=3, capacity=4
┌──────┬──────┬──────┬──────┐
│  A   │  B   │  C   │      │
└──────┴──────┴──────┴──────┘
start   finish            end_of_storage

push_back(D) 触发扩容：
1. 分配新内存（capacity = 8，2倍增长）
2. 移动旧元素到新内存
3. 构造新元素
4. 释放旧内存

扩容后：size=4, capacity=8
┌──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┐
│  A   │  B   │  C   │  D   │      │      │      │      │
└──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┘
start                            finish                  end_of_storage
```

### 2.2 LLVM 的扩容策略

LLVM 也使用 **2x 扩容因子**：

```cpp
// 源码路径：references/impl/llvm-project/libcxx/include/__vector/vector.h

// __recommend 计算推荐的容量
// 通常按 2 倍增长
static size_type __recommend(size_type __new_size) {
    const size_type __ms = max_size();
    if (__new_size > __ms)
        __throw_length_error("vector");
    const size_type __cap = capacity();
    if (__cap >= __ms / 2)
        return __ms;
    return std::max(2 * __cap, __new_size);
}
```

### 2.3 扩容策略对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ 扩容因子               │ 2x                   │ 2x                   │
│ 最大容量检查           │ max_size()           │ max_size()           │
│ 异常安全               │ 强保证               │ 强保证               │
│ 扩容时元素移动         │ std::move_if_noexcept│ std::move_if_noexcept│
│ 已废弃迭代器失效       │ 所有迭代器失效       │ 所有迭代器失效       │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 三、push_back 实现

### 3.1 GCC 的 push_back（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/stl_vector.h:1458

void push_back(const value_type& __x) {
    if (this->_M_impl._M_finish != this->_M_impl._M_end_of_storage) {
        // 有空间：直接在末尾构造
        _Alloc_traits::construct(this->_M_impl, this->_M_impl._M_finish, __x);
        ++this->_M_impl._M_finish;
    } else {
        // 无空间：调用 _M_realloc_append
        _M_realloc_append(__x);
    }
}
```

### 3.2 GCC 的 _M_realloc_insert（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/vector.tcc:433

void vector::_M_realloc_insert(iterator __position, const _Tp& __x) {
    // 1. 计算新容量
    const size_type __len1 = _M_check_len(1u, "vector::_M_realloc_insert");
    
    // 2. 保存旧指针
    pointer __old_start = this->_M_impl._M_start;
    pointer __old_finish = this->_M_impl._M_finish;
    const size_type __elems_before = __position - begin();
    
    // 3. 分配新内存（使用 allocate_at_least 优化）
    _Alloc_result __r = this->_M_allocate_at_least(__len1);
    pointer __new_start = __r.__ptr;
    pointer __new_finish = __new_start;
    
    // 4. 在新位置构造新元素
    _Alloc_traits::construct(this->_M_impl,
                             __new_start + __elems_before, __x);
    
    // 5. 移动前半部分元素
    __new_finish = std::__uninitialized_move_if_noexcept_a(
        __old_start, __position.base(),
        __new_start, _M_get_Tp_allocator());
    
    ++__new_finish;  // 跳过新插入的元素
    
    // 6. 移动后半部分元素
    __new_finish = std::__uninitialized_move_if_noexcept_a(
        __position.base(), __old_finish,
        __new_finish, _M_get_Tp_allocator());
    
    // 7. 销毁旧元素，释放旧内存
    std::_Destroy(__old_start, __old_finish, _M_get_Tp_allocator());
    _M_deallocate(__old_start, __old_finish - __old_start);
    
    // 8. 更新指针
    this->_M_impl._M_start = __new_start;
    this->_M_impl._M_finish = __new_finish;
    this->_M_impl._M_end_of_storage = __new_start + __len;
}
```

### 3.3 _M_check_len 的容量计算

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/stl_vector.h

size_type _M_check_len(size_type __n, const char* __s) const {
    if (max_size() - size() < __n)
        __throw_length_error(__s);
    const size_type __len = size() + std::max(size(), __n);
    return (__len < size() || __len > max_size()) ? max_size() : __len;
}
// 解释：新容量 = max(size, 1) * 2，即 2 倍增长
```

### 3.2 LLVM 的 push_back

```cpp
// 源码路径：references/impl/llvm-project/libcxx/include/__vector/vector.h

_LIBCPP_CONSTEXPR_SINCE_CXX20 void push_back(const value_type& __x) {
    if (__end_ != __get_cap()) {
        // 有空间：直接构造
        __construct_one_at_end(__x);
    } else {
        // 无空间：扩容后构造
        __emplace_back_slow_path(__x);
    }
}
```

### 3.3 push_back 流程图

```
push_back 流程：

                    ┌─────────────────┐
                    │ push_back(value) │
                    └────────┬────────┘
                             │
                    ┌────────▼────────┐
                    │ finish ==        │
                    │ end_of_storage?  │
                    └────────┬────────┘
                             │
              ┌──────────────┴──────────────┐
              │ Yes                         │ No
              ▼                             ▼
    ┌─────────────────┐           ┌─────────────────┐
    │ 扩容            │           │ 直接构造        │
    │ 1. 分配新内存   │           │ construct(finish,│
    │ 2. 移动旧元素   │           │     value)      │
    │ 3. 构造新元素   │           │ ++finish        │
    │ 4. 释放旧内存   │           └─────────────────┘
    │ 5. 更新指针     │
    └─────────────────┘
```

---

## 四、insert 实现

### 4.1 在中间位置插入

在中间位置插入需要移动后续元素：

```cpp
// 简化的 insert 逻辑
iterator insert(const_iterator __position, const value_type& __x) {
    size_type __offset = __position - begin();
    
    if (_M_impl._M_finish == _M_impl._M_end_of_storage) {
        // 需要扩容
        _M_realloc_insert(__position, __x);
    } else {
        // 有空间：移动后续元素
        if (__position == end()) {
            // 在末尾插入：直接构造
            _Alloc_traits::construct(_M_impl, _M_impl._M_finish, __x);
            ++_M_impl._M_finish;
        } else {
            // 在中间插入：移动 + 构造
            _Alloc_traits::construct(_M_impl, _M_impl._M_finish,
                                     std::move(*(_M_impl._M_finish - 1)));
            ++_M_impl._M_finish;
            std::move_backward(__position, end() - 2, end() - 1);
            const_cast<reference>(*__position) = __x;
        }
    }
    return begin() + __offset;
}
```

---

## 五、vector\<bool\> 特化

### 5.1 GCC 的 vector\<bool\>

GCC 的 `vector<bool>` 是一个**位压缩**特化：

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/stl_bvector.h

// vector<bool> 使用 _Bit_iterator 和 _Bit_reference
// 每个 bool 只占 1 位（而不是 1 字节）

// 内部使用 unsigned long 数组存储位
typedef unsigned long _Bit_type;
static const int _S_word_bit = __CHAR_BIT__ * sizeof(_Bit_type);

// _Bit_reference 代理引用
struct _Bit_reference {
    _Bit_type* _M_p;
    unsigned int _M_offset;
    
    operator bool() const { return (*_M_p >> _M_offset) & 1; }
    _Bit_reference& operator=(bool __x) {
        if (__x) *_M_p |= (1UL << _M_offset);
        else     *_M_p &= ~(1UL << _M_offset);
        return *this;
    }
};
```

### 5.2 LLVM 的 vector\<bool\>

LLVM 的 `vector<bool>` 也使用位压缩：

```cpp
// 源码路径：references/impl/llvm-project/libcxx/include/__vector/vector_bool.h

// 使用 __bit_reference 和 __bit_iterator
// 与 GCC 类似的位压缩策略
```

### 5.3 vector\<bool\> 的问题

```
vector<bool> 的陷阱：

1. 不是真正的容器：
   · reference 不是真正的引用（是代理对象）
   · data() 不可用（位不能取地址）
   · 迭代器不是真正的随机访问迭代器

2. 性能问题：
   · 每次访问需要位操作（测试/设置/重置）
   · 随机访问需要计算字节偏移和位偏移

3. 替代方案：
   · deque<bool>（每个 bool 占 1 字节，但分段存储）
   · vector<char>（每个 bool 占 1 字节）
   · bitset（固定大小，编译期已知）
```

---

## 六、异常安全

### 6.1 strong exception guarantee

vector 的 `push_back` 和 `insert` 提供**强异常保证**：

```
强异常保证的实现：

1. 如果元素类型的操作（拷贝/移动构造）不抛异常：
   · 直接在已有空间构造

2. 如果可能抛异常：
   · 先分配新内存
   · 在新内存中构造新元素（如果抛异常，旧元素未被修改）
   · 移动旧元素到新内存（使用 move_if_noexcept）
   · 释放旧内存

关键：移动使用 move_if_noexcept，避免在移动过程中抛异常
```

### 6.2 GCC 的异常安全实现

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/stl_uninitialized.h

// __uninitialized_move_if_noexcept_a
// 只有当移动构造函数是 noexcept 时才使用移动，否则使用拷贝
template<typename _InputIterator, typename _ForwardIterator, typename _Allocator>
_ForwardIterator
__uninitialized_move_if_noexcept_a(_InputIterator __first, _InputIterator __last,
                                   _ForwardIterator __result, _Allocator& __alloc) {
    typedef typename iterator_traits<_InputIterator>::value_type _Tp;
    typedef typename is_nothrow_move_constructible<_Tp>::type _NoexMove;
    return __uninitialized_move_a(__first, __last, __result, __alloc, _NoexMove());
}
```

---

## 七、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ 内部数据结构           │ _Vector_impl_data    │ __split_buffer       │
│ 指针命名               │ _M_start/finish/     │ __begin_/end_/       │
│                        │ _M_end_of_storage    │ __back_cap_          │
│ 扩容因子               │ 2x                   │ 2x                   │
│ allocator 存储         │ EBO 压缩             │ compressed_pair      │
│ vector<bool> 特化      │ _Bit_iterator        │ __bit_iterator       │
│ ASan 支持              │ 有（_GLIBCXX_SANITIZE）│ 有（sanitizers）    │
│ C++20 constexpr        │ 支持                 │ 支持                 │
│ trivially_relocatable  │ 不支持               │ 支持（C++23）        │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 八、性能优化建议

```
性能优化指南：

1. 预分配空间：
   v.reserve(n);  // 避免多次扩容

2. 避免在中间插入：
   · 使用 deque 或 list 代替
   · 或批量插入（一次分配足够空间）

3. 使用 emplace_back 代替 push_back：
   v.emplace_back(args...);  // 避免临时对象

4. 移动语义：
   v.push_back(std::move(x));  // 避免拷贝

5. shrink_to_fit：
   v.shrink_to_fit();  // 释放多余空间

6. 不要依赖 vector<bool> 的位压缩：
   · 使用 vector<char> 或 deque<bool>
```

---

## 延伸阅读

- [std::string 实现](/internals/containers/string) — 另一个关键容器的实现
- [std::deque 实现](/internals/containers/deque) — 分段连续存储的容器
- [分配器模型](/internals/memory/allocator) — allocator 如何与容器交互
- [内存操作基础设施](/internals/memory/operations) — uninitialized_copy/fill 等底层操作
