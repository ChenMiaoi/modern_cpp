---
title: "std::flat_map 实现分析"
topic: internals
feature: flat-map
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/std/flat_map"
source_llvm: "references/impl/llvm-project/libcxx/include/__flat_map/flat_map.h"
---

# std::flat_map 实现分析

> `std::flat_map` 是 C++23 引入的有序关联容器，基于排序 vector 实现。本文基于 GCC 和 LLVM 的源码，分析 flat_map 的内部实现。

---

## 一、核心概念

### 1.1 什么是 flat_map

flat_map 是基于排序 vector 的关联容器：

```cpp
// flat_map 的基本使用
flat_map<string, int> m;
m["key1"] = 1;
m["key2"] = 2;

// 使用
for (auto& [k, v] : m) {
    cout << k << ": " << v << endl;
}
```

### 1.2 flat_map vs map

```
flat_map vs map：

flat_map：
  · 基于排序 vector
  · 插入 O(n)
  · 查找 O(log n)
  · 更好的缓存局部性

map：
  · 基于红黑树
  · 插入 O(log n)
  · 查找 O(log n)
  · 更差的缓存局部性
```

### 1.3 GCC (libstdc++) 的实现（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/std/flat_map

// flat_map 的存储结构
template<typename _Key, typename _Tp, typename _Compare = less<_Key>,
         typename _Allocator = allocator<pair<const _Key, _Tp>>>
class flat_map {
    // 排序后的 vector
    vector<pair<key_type, mapped_type>, _Allocator> _M_c;
    
    // 比较器
    _Compare _M_comp;
    
public:
    // 插入
    pair<iterator, bool> insert(const value_type& __x) {
        // 二分查找插入位置
        auto __it = lower_bound(__x.first);
        if (__it != end() && !_M_comp(__x.first, __it->first)) {
            // 键已存在
            return {__it, false};
        }
        // 插入
        __it = _M_c.insert(__it, __x);
        return {__it, true};
    }
    
    // 查找
    iterator find(const key_type& __x) {
        auto __it = lower_bound(__x);
        if (__it != end() && !_M_comp(__x, __it->first)) {
            return __it;
        }
        return end();
    }
    
    // 二分查找
    iterator lower_bound(const key_type& __x) {
        return std::lower_bound(begin(), end(), __x,
            [this](const value_type& __v, const key_type& __k) {
                return _M_comp(__v.first, __k);
            });
    }
};
```

---

## 二、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ flat_map               │ 支持                 │ 支持                 │
│ flat_set               │ 支持                 │ 支持                 │
│ sorted_unique          │ 支持                 │ 支持                 │
│ sorted_equivalent      │ 支持                 │ 支持                 │
│ node_type              │ 支持                 │ 支持                 │
│ extract                │ 支持                 │ 支持                 │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 延伸阅读

- [std::map/set 实现](/internals/containers/map-set) — 红黑树实现
- [std::vector 实现](/internals/containers/vector) — 底层容器
