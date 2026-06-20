---
title: "std::unordered_map 实现分析"
topic: internals
feature: unordered-map
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/bits/hashtable.h"
source_llvm: "references/impl/llvm-project/libcxx/include/__hash_table"
---

# std::unordered_map 实现分析

> `std::unordered_map` 基于哈希表（Hash Table）实现，提供平均 O(1) 的查找性能。本文基于 GCC 和 LLVM 的源码，分析哈希表的内部实现。

---

## 一、核心数据结构

### 1.1 哈希表节点

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/hashtable.h

// 节点基类
struct _Hash_node_base {
    _Hash_node_base* _M_next;  // 指向下一个节点（链表）
};

// 节点（带值类型）
template<typename _Value, bool __cache_hash_code>
struct _Hash_node : _Hash_node_base {
    _Value _M_value;  // 存储的值
    
    // 如果缓存哈希码
    size_t _M_hash_code;  // 缓存的哈希值
};
```

```
哈希表节点布局：

不缓存哈希码时：
┌─────────────────────────────────────┐
│ _M_next (8 字节)                     │  ← 链表指针
├─────────────────────────────────────┤
│ _M_value (sizeof(Value) 字节)        │  ← 存储的值
└─────────────────────────────────────┘

缓存哈希码时：
┌─────────────────────────────────────┐
│ _M_next (8 字节)                     │
├─────────────────────────────────────┤
│ _M_value (sizeof(Value) 字节)        │
├─────────────────────────────────────┤
│ _M_hash_code (8 字节)                │  ← 缓存的哈希值
└─────────────────────────────────────┘
```

### 1.2 哈希表结构

```cpp
// GCC 的 _Hashtable 结构
template<typename _Key, typename _Value, ...>
class _Hashtable {
    // 数据成员：
    // - _Bucket[] _M_buckets        桶数组
    // - _Hash_node_base _M_before_begin  单链表头
    // - size_type _M_bucket_count    桶数量
    // - size_type _M_element_count   元素数量
};
```

```
哈希表的内存布局：

┌─────────────────────────────────────┐
│ _M_buckets (桶数组)                  │
│   [0] → node → node → nullptr      │
│   [1] → nullptr                     │
│   [2] → node → nullptr             │
│   [3] → node → node → node → ...   │
│   ...                               │
│   [N-1] → nullptr                   │
├─────────────────────────────────────┤
│ _M_before_begin                     │  ← 单链表头节点
├─────────────────────────────────────┤
│ _M_bucket_count (桶数量)            │
├─────────────────────────────────────┤
│ _M_element_count (元素数量)         │
└─────────────────────────────────────┘
```

---

## 二、哈希冲突解决

### 2.1 链地址法（Separate Chaining）

GCC 使用链地址法解决哈希冲突：

```
链地址法示例：

hash("hello") % 4 = 2
hash("world") % 4 = 2
hash("foo") % 4 = 0

桶数组：
┌───────┐
│   0   │ → "foo" → nullptr
├───────┤
│   1   │ → nullptr
├───────┤
│   2   │ → "hello" → "world" → nullptr  ← 哈希冲突
├───────┤
│   3   │ → nullptr
└───────┘
```

### 2.2 负载因子

```
负载因子（Load Factor）= 元素数量 / 桶数量

负载因子的影响：
  · 负载因子高 → 冲突多 → 查找性能下降
  · 负载因子低 → 空间浪费

默认负载因子：
  · GCC: max_load_factor() = 1.0
  · LLVM: max_load_factor() = 1.0

rehash 时机：
  · 插入时如果 负载因子 > max_load_factor()
  · 调用 rehash() 或 reserve() 时
```

---

## 三、GCC (libstdc++) 的实现

### 3.1 rehash 策略（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/hashtable_policy.h

// GCC 使用 _Prime_rehash_policy 控制 rehash 策略
struct _Prime_rehash_policy {
    float _M_max_load_factor;  // 最大负载因子（默认 1.0）
    size_t _M_next_bkt;        // 下一个桶数量
    size_t _M_resize_threshold; // 重新哈希阈值
    
    // 计算下一个桶数量（使用质数表）
    size_t _M_next_bkt(size_t __n) const {
        // 质数表：7, 11, 13, 17, 23, 29, 31, 37, ...
        // 找到第一个 >= __n 的质数
        return _S_next_prime(__n);
    }
    
    // 判断是否需要 rehash
    pair<bool, size_t> _M_need_rehash(size_t __n_bkt, size_t __n_elt, 
                                       size_t __n_ins) const {
        if (__n_elt + __n_ins > _M_resize_threshold) {
            // 需要 rehash
            size_t __new_bkt = _M_next_bkt(max(__n_bkt, __n_ins) * 2);
            return make_pair(true, __new_bkt);
        }
        return make_pair(false, __n_bkt);
    }
};
```

### 3.2 插入节点（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/hashtable.h:1099

auto _Hashtable::_M_insert_unique_node(size_type __bkt, __hash_code __code,
                                       __node_ptr __node, size_type __n_elt)
    -> iterator {
    // 1. 检查是否需要 rehash
    __rehash_guard_t __rehash_guard(_M_rehash_policy);
    auto __do_rehash = _M_rehash_policy._M_need_rehash(
        _M_bucket_count, _M_element_count, __n_elt);
    
    if (__do_rehash.first) {
        // 需要 rehash
        _M_rehash(__do_rehash.second, __node_allocator_type());
        __bkt = _M_bucket_index(__node);
    }
    
    // 2. 插入节点到桶的链表头部
    __node->_M_next = _M_buckets[__bkt];
    _M_buckets[__bkt] = __node;
    
    // 3. 更新元素计数
    ++_M_element_count;
    
    // 4. 返回迭代器
    return iterator(__node);
}
```

### 3.3 哈希码计算（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/hashtable_policy.h

// 默认哈希函数
struct _Default_hash_policy {
    // 将哈希值映射到桶索引
    size_t _M_bkt_for_elements(size_t __n) const {
        // 返回 >= __n 的质数
        return _S_next_prime(__n);
    }
    
    // 范围哈希：hash % bucket_count
    template<typename _RangeHash>
    size_t operator()(size_t __hash, size_t __bucket_count) const {
        return __hash % __bucket_count;
    }
};

// 标准库提供的哈希函数
template<typename _Tp>
struct hash : public __hash_base<size_t, _Tp> {
    size_t operator()(_Tp __val) const noexcept {
        // 使用 FNV-1a 或其他哈希算法
        size_t __hash = 0;
        for (size_t i = 0; i < sizeof(_Tp); ++i) {
            __hash ^= (reinterpret_cast<const char*>(&__val)[i] << (i % 8));
        }
        return __hash;
    }
};
```

---

## 四、LLVM (libc++) 的实现

### 4.1 __hash_table 结构

LLVM 使用 `__hash_table` 类：

```cpp
// 源码路径：references/impl/llvm-project/libcxx/include/__hash_table

template <class _Tp, class _Hash, class _Equal, class _Allocator>
class __hash_table {
    __node_pointer              __bucket_list_;    // 桶数组
    __compressed_pair<size_type, __node_pointer> __p1_;  // 元素数量 + 首节点
    compressed_pair<hasher, key_equal> __p2_;     // 哈希函数 + 相等比较
};
```

### 4.2 负载因子控制

LLVM 使用不同的 rehash 策略：

```
LLVM 的 rehash 策略：

  · 负载因子 > max_load_factor() 时触发
  · 桶数量增长：通常是 2 倍（不是质数）
  · 更简单的实现，但可能有更多冲突
```

---

## 五、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ 哈希表实现             │ _Hashtable           │ __hash_table         │
│ 冲突解决               │ 链地址法             │ 链地址法             │
│ 桶数量策略             │ 质数                 │ 2 的幂次             │
│ 哈希码缓存             │ 可选                 │ 可选                 │
│ rehash 策略            │ _RehashPolicy        │ 内置                 │
│ 节点分配               │ allocator aware      │ allocator aware      │
│ 迭代器                 │ 跨桶遍历             │ 跨桶遍历             │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 六、性能特征

```
unordered_map 的性能：

查找：O(1) 平均，O(n) 最坏
插入：O(1) 平均，O(n) 最坏
删除：O(1) 平均，O(n) 最坏
迭代：O(n + bucket_count)

内存开销：
  · 每个节点：16 字节（指针）+ sizeof(T)
  · 桶数组：bucket_count × 8 字节
  · 100 万个 int → 约 24 MB（16 + 4 = 20 字节/节点 + 桶数组）

与 map 对比：
  · unordered_map 查找：O(1) vs O(log n)
  · unordered_map 内存：更多（桶数组）
  · unordered_map 迭代：无序 vs 有序
```

---

## 延伸阅读

- [std::map/set 实现](/internals/containers/map-set) — 红黑树实现
- [std::vector 实现](/internals/containers/vector) — 连续存储容器
- [std::hash 实现](/internals/utilities/hash) — 哈希函数的实现
