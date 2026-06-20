---
title: "std::bitset 实现分析"
topic: internals
feature: bitset
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/bitset"
source_llvm: "references/impl/llvm-project/libcxx/include/bitset"
---

# std::bitset 实现分析

> `std::bitset` 是固定大小的位集合，提供高效的位操作。本文基于 GCC 和 LLVM 的源码，分析 bitset 的内部实现。

---

## 一、核心数据结构

### 1.1 存储布局

```
bitset 的内存布局：

bitset<32> 的存储：
┌─────────────────────────────────────┐
│ unsigned long _M_w[1]               │  ← 32 位存储在一个 word 中
└─────────────────────────────────────┘

bitset<64> 的存储：
┌─────────────────────────────────────┐
│ unsigned long _M_w[1]               │  ← 64 位存储在一个 word 中
└─────────────────────────────────────┘

bitset<128> 的存储：
┌─────────────────────────────────────┐
│ unsigned long _M_w[2]               │  ← 128 位存储在两个 word 中
└─────────────────────────────────────┘
```

### 1.2 GCC (libstdc++) 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bitset

template<size_t _Nb>
class bitset {
    // 存储类型
    using _WordT = unsigned long;
    using _Words = _WordT[_Nb / (sizeof(_WordT) * __CHAR_BIT__)
                          + (_Nb % (sizeof(_WordT) * __CHAR_BIT__) != 0)];
    
    _Words _M_w;  // 存储数组
    
public:
    // 设置位
    bitset& set(size_t __pos, bool __val = true) {
        if (__val)
            _M_w[__pos / __SIZEOF_LONG__] |= (1UL << (__pos % __SIZEOF_LONG__));
        else
            _M_w[__pos / __SIZEOF_LONG__] &= ~(1UL << (__pos % __SIZEOF_LONG__));
        return *this;
    }
    
    // 测试位
    bool test(size_t __pos) const {
        return (_M_w[__pos / __SIZEOF_LONG__] >> (__pos % __SIZEOF_LONG__)) & 1UL;
    }
    
    // 翻转位
    bitset& flip(size_t __pos) {
        _M_w[__pos / __SIZEOF_LONG__] ^= (1UL << (__pos % __SIZEOF_LONG__));
        return *this;
    }
    
    // 统计 1 的个数
    size_t count() const noexcept {
        size_t __result = 0;
        for (size_t __i = 0; __i < _Nb / __SIZEOF_LONG__; ++__i) {
            __result += __builtin_popcountl(_M_w[__i]);
        }
        return __result;
    }
    
    // 检查是否有位被设置
    bool any() const noexcept {
        for (size_t __i = 0; __i < _Nb / __SIZEOF_LONG__; ++__i) {
            if (_M_w[__i]) return true;
        }
        return false;
    }
    
    // 检查是否所有位都被清除
    bool none() const noexcept {
        return !any();
    }
    
    // 返回大小
    size_t size() const noexcept { return _Nb; }
};
```

### 1.2 GCC (libstdc++) 的实现

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bitset

template<size_t _Nb>
class bitset {
    // 存储类型
    using _WordT = unsigned long;
    using _Words = _WordT[_Nb / (sizeof(_WordT) * __CHAR_BIT__)
                          + (_Nb % (sizeof(_WordT) * __CHAR_BIT__) != 0)];
    
    _Words _M_w;  // 存储数组
    
public:
    // 设置位
    bitset& set(size_t __pos, bool __val = true) {
        if (__val)
            _M_w[__pos / __SIZEOF_LONG__] |= (1UL << (__pos % __SIZEOF_LONG__));
        else
            _M_w[__pos / __SIZEOF_LONG__] &= ~(1UL << (__pos % __SIZEOF_LONG__));
        return *this;
    }
    
    // 测试位
    bool test(size_t __pos) const {
        return (_M_w[__pos / __SIZEOF_LONG__] >> (__pos % __SIZEOF_LONG__)) & 1UL;
    }
    
    // 翻转位
    bitset& flip(size_t __pos) {
        _M_w[__pos / __SIZEOF_LONG__] ^= (1UL << (__pos % __SIZEOF_LONG__));
        return *this;
    }
};
```

---

## 二、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ 存储类型               │ unsigned long[]      │ unsigned long[]      │
│ 位操作                 │ 位运算               │ 位运算               │
│ count                  │ 内建函数             │ 内建函数             │
│ find_first/find_next   │ 支持                 │ 支持                 │
│ to_string              │ 支持                 │ 支持                 │
│ constexpr 支持         │ C++20                │ C++20                │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 三、最佳实践

```
bitset 使用指南：

1. 固定大小位集合：
   bitset<64> flags;

2. 位操作：
   flags.set(5);      // 设置位 5
   flags.reset(5);    // 清除位 5
   flags.flip(5);     // 翻转位 5
   bool b = flags.test(5);  // 测试位 5

3. 批量操作：
   flags.set();       // 设置所有位
   flags.reset();     // 清除所有位
   flags.flip();      // 翻转所有位

4. 转换：
   unsigned long val = flags.to_ulong();
   string str = flags.to_string();
```

---

## 延伸阅读

- [std::vector 实现](/internals/containers/vector) — 动态大小容器
- [std::array 实现](/internals/containers/array) — 固定大小数组
- [std::optional 实现](/internals/utilities/optional) — 可选值容器
