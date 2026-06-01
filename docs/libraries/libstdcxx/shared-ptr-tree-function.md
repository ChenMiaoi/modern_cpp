# libstdc++ shared_ptr / RB-tree / function

## std::shared_ptr 控制块设计

```cpp
template<typename _Lp>
class _Sp_counted_base {
  _Atomic_word  _M_use_count;    // 强引用计数
  _Atomic_word  _M_weak_count;   // 弱引用计数
  virtual void _M_dispose() = 0;      // use_count → 0
  virtual void _M_destroy() = 0;      // weak_count → 0
};
```

### make_shared 单次分配

```
  _Sp_counted_ptr_inplace<T, Alloc>
  ┌────────────────────────────────────────────────┐
  │ +0:  vptr                  (8B)                │
  │ +8:  _M_use_count  = 2    (4B)                │
  │ +12: _M_weak_count = 1    (4B)                │
  │ +16: _M_impl._M_alloc     (0B, 空 allocator)  │
  │ +16: _M_storage           (sizeof(T))          │
  └────────────────────────────────────────────────┘
  一次 malloc，控制块 + 对象连续存储
```

析构序列：use_count → 0 时只析构元素（不释放），weak_count → 0 时释放整块内存。

## _Rb_tree 红黑树

### 哨兵节点 _M_header

```
  _M_header（始终标红）
  ┌────────────┐
  │ _M_left ───┼──→ 最左节点 (begin)
  │ _M_right ──┼──→ 最右节点 (--end, O(1))
  │ _M_parent──┼──→ 根节点 root
  └────────────┘

  空树时：_M_left = _M_right = &header
  root._M_parent = header → 用于 end() 判定
```

### hint 插入优化

```cpp
iterator insert_unique(iterator __position, const value_type& __v) {
  if (__position._M_node == _M_impl._M_header._M_left) {
    // hint = begin()：如果新元素 < 最小，O(1) 插到左边
  } else if (__position._M_node == &_M_impl._M_header) {
    // hint = end()：如果新元素 > 最大，O(1) 插到右边
  }
  return insert_unique(__v).first;  // fallback: O(log n)
}
```

## std::function：函数指针 SBO

```cpp
class function<_Res(_ArgTypes...)> {
  typedef typename aligned_storage<3 * sizeof(void*)>::type _Any_data;
  _Any_data      _M_functor;   // callable 存储（24B 栈或堆）
  _Invoker_type  _M_invoker;   // 调用分发函数指针
  void (*_M_manager)(_Any_data&, const _Any_data&, _Manager_operation);
};
```

**与 libc++ 的差异**：libstdc++ 用函数指针代替虚函数，避免 vtable 间接调用开销。在频繁调用 `std::function` 的热路径上可能有微小性能差异。
