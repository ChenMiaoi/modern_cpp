---
title: "std::map/set 实现分析"
topic: internals
feature: map-set
standard: N/A
status_checked_at: 2026-06-20
source_gcc: "references/impl/gcc/libstdc++-v3/include/bits/stl_tree.h"
source_llvm: "references/impl/llvm-project/libcxx/include/__tree"
---

# std::map/set 实现分析

> `std::map` 和 `std::set` 都基于红黑树（Red-Black Tree）实现。本文基于 GCC 和 LLVM 的源码，分析红黑树的内部实现。

---

## 一、核心数据结构

### 1.1 红黑树节点

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/include/bits/stl_tree.h

struct _Rb_tree_node_base {
    typedef _Rb_tree_node_base* _Base_ptr;
    
    _Rb_tree_color _M_color;   // 节点颜色（红或黑）
    _Base_ptr      _M_parent;  // 父节点
    _Base_ptr      _M_left;    // 左子节点
    _Base_ptr      _M_right;   // 右子节点
};

template<typename _Val>
struct _Rb_tree_node : public _Rb_tree_node_base {
    __gnu_cxx::__aligned_membuf<_Val> _M_storage;  // 对齐存储
    
    _Val* _M_valptr() { return _M_storage._M_ptr(); }
};
```

```
红黑树节点布局：

┌─────────────────────────────────────────┐
│ _M_color (1 字节)                        │  ← 红或黑
├─────────────────────────────────────────┤
│ _M_parent (8 字节)                       │  ← 父节点指针
├─────────────────────────────────────────┤
│ _M_left (8 字节)                         │  ← 左子节点
├─────────────────────────────────────────┤
│ _M_right (8 字节)                        │  ← 右子节点
├─────────────────────────────────────────┤
│ _M_storage (对齐存储)                    │  ← 值类型
└─────────────────────────────────────────┘

总大小：4（颜色+填充）+ 8×3（指针）+ sizeof(T) = 28 + sizeof(T)
```

### 1.2 红黑树头节点

```cpp
struct _Rb_tree_header {
    _Rb_tree_node_base _M_header;    // 头节点
    size_t             _M_node_count; // 节点数量
    
    void _M_reset() {
        _M_header._M_parent = 0;
        _M_header._M_left = &_M_header;
        _M_header._M_right = &_M_header;
        _M_node_count = 0;
    }
};
```

```
红黑树的头节点设计：

_header._M_parent → 根节点
_header._M_left   → 最左节点（begin()）
_header._M_right  → 最右节点

空树状态：
┌──────────────────┐
│ header           │
│ _M_parent = nullptr │
│ _M_left = &header │  ← 指向自己
│ _M_right = &header │  ← 指向自己
└──────────────────┘
```

---

## 二、红黑树性质

### 2.1 红黑树的五个性质

```
红黑树的五个性质：

1. 每个节点是红色或黑色
2. 根节点是黑色
3. 所有叶子节点（NIL）是黑色
4. 红色节点的两个子节点都是黑色
5. 从任一节点到其每个叶子的所有路径包含相同数目的黑色节点

这些性质保证：
· 最长路径不超过最短路径的 2 倍
· 树的高度为 O(log n)
· 插入、删除、查找都是 O(log n)
```

### 2.2 插入操作

```
插入操作的步骤：

1. 找到插入位置（O(log n)）
2. 创建新节点（红色）
3. 插入节点
4. 修复红黑树性质（可能需要旋转）

修复规则：
· 情况 1：叔叔节点是红色
  → 重新着色（父+叔变黑，祖父变红）

· 情况 2：叔叔节点是黑色，当前节点是右孩子
  → 左旋转换为情况 3

· 情况 3：叔叔节点是黑色，当前节点是左孩子
  → 右旋 + 重新着色
```

---

## 三、GCC (libstdc++) 的实现

### 3.1 红黑树插入与旋转（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/src/c++98/tree.cc:194

void _Rb_tree_insert_and_rebalance(const bool __insert_left,
                                   _Rb_tree_node_base* __x,
                                   _Rb_tree_node_base* __p,
                                   _Rb_tree_node_base& __header) throw () {
    _Rb_tree_node_base*& __root = __header._M_parent;

    // 1. 初始化新节点
    __x->_M_parent = __p;
    __x->_M_left = 0;
    __x->_M_right = 0;
    __x->_M_color = _S_red;  // 新节点总是红色

    // 2. 插入节点
    if (__insert_left) {
        __p->_M_left = __x;
        if (__p == &__header) {
            __header._M_parent = __x;
            __header._M_right = __x;
        } else if (__p == __header._M_left)
            __header._M_left = __x;
    } else {
        __p->_M_right = __x;
        if (__p == __header._M_right)
            __header._M_right = __x;
    }

    // 3. 重新平衡（核心算法）
    while (__x != __root && __x->_M_parent->_M_color == _S_red) {
        _Rb_tree_node_base* const __xpp = __x->_M_parent->_M_parent;
        
        if (__x->_M_parent == __xpp->_M_left) {
            // 情况 1：父节点是左子节点
            _Rb_tree_node_base* const __y = __xpp->_M_right;
            if (__y && __y->_M_color == _S_red) {
                // 叔叔节点是红色：重新着色
                __x->_M_parent->_M_color = _S_black;
                __y->_M_color = _S_black;
                __xpp->_M_color = _S_red;
                __x = __xpp;
            } else {
                // 叔叔节点是黑色
                if (__x == __x->_M_parent->_M_right) {
                    __x = __x->_M_parent;
                    local_Rb_tree_rotate_left(__x, __root);
                }
                __x->_M_parent->_M_color = _S_black;
                __xpp->_M_color = _S_red;
                local_Rb_tree_rotate_right(__xpp, __root);
            }
        } else {
            // 情况 2：父节点是右子节点（对称）
            _Rb_tree_node_base* const __y = __xpp->_M_left;
            if (__y && __y->_M_color == _S_red) {
                __x->_M_parent->_M_color = _S_black;
                __y->_M_color = _S_black;
                __xpp->_M_color = _S_red;
                __x = __xpp;
            } else {
                if (__x == __x->_M_parent->_M_left) {
                    __x = __x->_M_parent;
                    local_Rb_tree_rotate_right(__x, __root);
                }
                __x->_M_parent->_M_color = _S_black;
                __xpp->_M_color = _S_red;
                local_Rb_tree_rotate_left(__xpp, __root);
            }
        }
    }
    __root->_M_color = _S_black;  // 根节点总是黑色
}
```

### 3.2 红黑树旋转（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/src/c++98/tree.cc:132

// 左旋
static void local_Rb_tree_rotate_left(_Rb_tree_node_base* const __x,
                                      _Rb_tree_node_base*& __root) {
    _Rb_tree_node_base* const __y = __x->_M_right;
    
    __x->_M_right = __y->_M_left;
    if (__y->_M_left != 0)
        __y->_M_left->_M_parent = __x;
    __y->_M_parent = __x->_M_parent;
    
    if (__x == __root)
        __root = __y;
    else if (__x == __x->_M_parent->_M_left)
        __x->_M_parent->_M_left = __y;
    else
        __x->_M_parent->_M_right = __y;
    
    __y->_M_left = __x;
    __x->_M_parent = __y;
}

// 右旋（对称）
static void local_Rb_tree_rotate_right(_Rb_tree_node_base* const __x,
                                       _Rb_tree_node_base*& __root) {
    _Rb_tree_node_base* const __y = __x->_M_left;
    
    __x->_M_left = __y->_M_right;
    if (__y->_M_right != 0)
        __y->_M_right->_M_parent = __x;
    __y->_M_parent = __x->_M_parent;
    
    if (__x == __root)
        __root = __y;
    else if (__x == __x->_M_parent->_M_right)
        __x->_M_parent->_M_right = __y;
    else
        __x->_M_parent->_M_left = __y;
    
    __y->_M_right = __x;
    __x->_M_parent = __y;
}
```

### 3.3 迭代器递增（源码分析）

```cpp
// 源码路径：references/impl/gcc/libstdc++-v3/src/c++98/tree.cc:59

// 中序遍历的下一个节点
static _Rb_tree_node_base* local_Rb_tree_increment(_Rb_tree_node_base* __x) throw () {
    if (__x->_M_right != 0) {
        // 有右子树：找到右子树的最左节点
        __x = __x->_M_right;
        while (__x->_M_left != 0)
            __x = __x->_M_left;
    } else {
        // 无右子树：向上回溯
        _Rb_tree_node_base* __y = __x->_M_parent;
        while (__x == __y->_M_right) {
            __x = __y;
            __y = __y->_M_parent;
        }
        if (__x->_M_right != __y)
            __x = __y;
    }
    return __x;
}
```

### 3.2 node_handle 支持 (C++17)

```cpp
// GCC 支持 node_handle 用于提取/插入节点
// 这允许在容器之间移动节点而不拷贝数据

node_type extract(const_iterator __position);
iterator insert(node_type&& __nh);
```

---

## 四、LLVM (libc++) 的实现

### 4.1 __tree 结构

LLVM 使用 `__tree` 类实现红黑树：

```cpp
// 源码路径：references/impl/llvm-project/libcxx/include/__tree

template <class _Tp, class _Compare, class _Allocator>
class __tree {
    using __node = __tree_node_types<_Tp>::__node;
    using __node_pointer = __node*;
    
    __node_pointer __begin_node_;  // 指向最左节点
    pair<__node_pointer, compressed_pair<__end_node_t, __tree_balance>> __pair3_;
};
```

### 4.2 压缩存储

LLVM 使用 `compressed_pair` 压缩比较器和平衡信息：

```
LLVM __tree 的内存布局：

┌─────────────────────────────────────┐
│ __begin_node_ (8 字节)               │  ← 指向最左节点
├─────────────────────────────────────┤
│ __pair3_                             │
│   first: __end_node_ (包含根节点)    │
│   second: compressed_pair            │
│     first: __node_count_            │
│     second: __comp_ (比较器)         │
└─────────────────────────────────────┘
```

---

## 五、map vs set

```
map 和 set 的区别：

set：
  · 只存储键（key）
  · 节点类型：_Rb_tree_node<key_type>
  · 比较器：key_compare

map：
  · 存储键值对（key-value）
  · 节点类型：_Rb_tree_node<value_type>
  · value_type = pair<const Key, T>
  · 比较器：key_compare（只比较 key）

内存布局差异：
  set 节点：sizeof(node) = 28 + sizeof(Key)
  map 节点：sizeof(node) = 28 + sizeof(pair<const Key, T>)
```

---

## 六、GCC vs LLVM 差异对比

```
┌────────────────────────┬──────────────────────┬──────────────────────┐
│ 特性                    │ GCC (libstdc++)      │ LLVM (libc++)        │
├────────────────────────┼──────────────────────┼──────────────────────┤
│ 红黑树实现             │ _Rb_tree             │ __tree               │
│ 节点存储               │ aligned_membuf       │ 直接存储             │
│ 头节点设计             │ _Rb_tree_header      │ __end_node_t         │
│ 迭代器                 │ _Rb_tree_iterator    │ __tree_iterator      │
│ node_handle (C++17)    │ 支持                 │ 支持                 │
│ extract/insert 节点    │ 支持                 │ 支持                 │
│ 比较器存储             │ _Rb_tree_key_compare │ compressed_pair      │
└────────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 七、性能特征

```
map/set 的性能：

查找：O(log n)
插入：O(log n)（摊销）
删除：O(log n)
迭代：O(n)

内存开销：
  · 每个节点：28 字节（颜色+3指针+填充）+ 数据
  · 100 万个 int → 约 32 MB（28 + 4 = 32 字节/节点）

与 unordered_map 对比：
  · map 查找：O(log n) vs O(1) 平均
  · map 内存：更少（无桶数组）
  · map 迭代：有序 vs 无序
```

---

## 延伸阅读

- [std::unordered_map 实现](/internals/containers/unordered-map) — 哈希表实现
- [std::vector 实现](/internals/containers/vector) — 连续存储容器
- [std::deque 实现](/internals/containers/deque) — 分段连续存储容器
