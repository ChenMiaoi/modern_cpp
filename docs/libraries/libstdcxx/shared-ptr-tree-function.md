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

## 用户 API

本文覆盖的用户侧入口分别是 `std::shared_ptr`、关联容器背后的树结构，以及 `std::function` 的可调用包装器 API；现有正文已直接给出控制块、树哨兵与 SBO 骨架。

## 标准语义

待补：把共享所有权、关联容器有序性与 `std::function` 可空包装语义，分别对齐到标准要求与实现取舍。

## 对象布局

上文已经覆盖 `shared_ptr` 控制块、`_Rb_tree` 头节点与 `std::function` SBO 存储；后续补一张“对象本体 / 控制块 / 间接层”总览图。

## 核心源码路径

待补：补上 `shared_ptr_base.h`、`stl_tree.h`、`std_function.h` 的入口类与调用链，方便从公开 API 追到具体实现。

## 核心类 / 函数

待补：统一整理 `_Sp_counted_base`、`_Sp_counted_ptr_inplace`、`_Rb_tree::_M_header`、`insert_unique`、`function::_M_manager`、`_M_invoker`。

## 关键算法

待补：补充引用计数减到零的析构路径、RB-tree hint 插入分支、`std::function` 的 manager / invoker 分发路径。

## ABI 约束

待补：说明控制块虚函数布局、`std::function` SBO 大小以及树节点链接方式为什么属于 ABI 敏感实现细节。

## 异常安全

待补：补充 `make_shared` 单次分配、树插入失败回滚、`std::function` 堆分配与目标构造失败时的保证等级。

## iterator / reference invalidation

待补：这里需要分别讨论三类对象——`shared_ptr` 引用计数不涉及 iterator，RB-tree 插入/删除的迭代器稳定性，以及 `std::function` 目标替换后外部引用不可继续假定稳定。

## 性能模型

待补：补上 `shared_ptr` 原子引用计数、RB-tree 指针追逐、`std::function` SBO 命中率与间接调用成本的统一性能视角。

## libstdc++ vs libc++ vs MSVC

待补：对照三家在 `shared_ptr` 控制块、树节点布局和 `std::function` SBO / 调度策略上的差异。

## 最小复现代码

```cpp
#include <functional>
#include <memory>
#include <set>

int main() {
  auto p = std::make_shared<int>(42);
  std::set<int> s{3, 1, 2};
  std::function<int(int)> f = [keep = p](int x) { return x + *keep; };
  return f(*s.begin());
}
```

## 编译 / 反汇编 / benchmark 证据

待补：补上 `make_shared` 单次分配、树插入热路径以及 `std::function` SBO/堆分配切换点的汇编与 benchmark 证据。

## cpplings 练习入口

- [`smartptr2` — shared_ptr 与 weak_ptr](../../../exercises/cpp11-classes/smartptr2.cpp)
- [`smartptr1` — unique_ptr](../../../exercises/cpp11-classes/smartptr1.cpp)
- [`movonlyfunc1` — move_only_function 移动专用可调用包装器](../../../exercises/cpp23/movonlyfunc1.cpp)
