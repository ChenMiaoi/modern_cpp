---
title: libstdc++ shared_ptr / RB-tree / function
topic: libraries
feature: shared-ptr-tree-function
standard: C++11
status_checked_at: 2026-06-02
implementation:
  libstdcxx:
    paths:
      - references/impl/gcc/libstdc++-v3/include/bits/shared_ptr_base.h
      - references/impl/gcc/libstdc++-v3/include/bits/stl_tree.h
      - references/impl/gcc/libstdc++-v3/include/bits/std_function.h
    symbols:
      - _Sp_counted_base
      - _Sp_counted_ptr_inplace
      - _Rb_tree
      - std::function
      - _Function_base
exercises:
  - exercises/cpp11-classes/smartptr2.cpp
  - exercises/cpp11-classes/smartptr1.cpp
solutions:
  - exercises/solutions/smartptr2.cpp
  - exercises/solutions/smartptr1.cpp
---
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

**`std::shared_ptr`**：共享所有权模型，多个 `shared_ptr` 实例可指向同一对象。内部维护两阶段引用计数——`_M_use_count`（强引用）与 `_M_weak_count`（弱引用 + 1 基线），均通过 `__exchange_and_add_dispatch` 原子操作实现线程安全（`_S_atomic` 策略）。支持自定义删除器（deleter 与对象指针分开存储，导致两次分配），别名构造函数（aliasing constructor）允许从已有 `shared_ptr` 派生出指向其子对象的新 `shared_ptr`，共享同一个控制块但持有不同裸指针。

**`std::set` / `std::map`**：有序关联容器，底层数据结构为 `_Rb_tree` 红黑树。`set` 的 `value_type` 即 `key_type`（唯一键），`multiset` 允许重复键。元素按 `_Compare`（默认 `std::less<Key>`）排序存储。提供双向迭代器（`bidirectional_iterator_tag`），`begin()` 指向 `_M_header._M_left`（最左节点），`end()` 指向 `_M_header` 本身。插入操作不会使已有迭代器或引用失效（标准 §[associative.reqmts] 保证节点稳定性）。

**`std::function`**：类型擦除的可调用对象包装器，可存储函数指针、lambda、`std::bind` 表达式等任意 `Callable`。支持 `bool` 判空（`operator bool`），`operator()` 为空时抛 `std::bad_function_call`。与 C++23 `move_only_function` 不同，`std::function` 要求目标可拷贝（copyable），因此 `sizeof(std::function)` 通常为 3 个指针大小（`_Any_data` + `_M_invoker` + `_M_manager`）。libstdc++ 用函数指针而非虚函数实现分发，避免 vtable 间接开销。

## 对象布局

上文已经覆盖 `shared_ptr` 控制块、`_Rb_tree` 头节点与 `std::function` SBO 存储；后续补一张“对象本体 / 控制块 / 间接层”总览图。

## 核心源码路径

从用户 API 到具体实现的源码追踪路径：

| 用户 API | 公开头文件 | 内部实现头 | 关键入口 |
|----------|-----------|-----------|---------|
| `std::shared_ptr<T>` | `<memory>` → `shared_ptr.h` | `bits/shared_ptr_base.h` | `_Sp_counted_base` → `_M_release()` / `_M_dispose()` |
| `std::make_shared<T>` | `<memory>` → `shared_ptr.h` | `bits/shared_ptr_base.h` | `_Sp_counted_ptr_inplace::_M_get_deleter()` |
| `std::set<K>` / `std::map<K,V>` | `<set>` / `<map>` | `bits/stl_set.h` / `bits/stl_map.h` → `bits/stl_tree.h` | `_Rb_tree::_M_insert_unique()` / `_M_insert_()` |
| `std::function<Sig>` | `<functional>` | `bits/std_function.h` | `_Function_base::_Base_manager` → `_M_manager()` / `_M_invoke()` |

`shared_ptr_base.h`（约 2400 行）包含完整的控制块层次：`_Sp_counted_base` → `_Sp_counted_ptr`（两次分配路径）与 `_Sp_counted_ptr_inplace`（`make_shared` 路径）。`stl_tree.h`（约 3500 行）实现 `_Rb_tree` 全部红黑树逻辑，`stl_set.h` / `stl_map.h` 仅为薄包装层，将容器操作转发至 `_Rb_tree` 成员。`std_function.h`（约 800 行）包含 `_Function_base`（SBO 存储 + manager）、`_Function_handler`（类型擦除的 invoker）以及 `function` 模板类的 `operator()` / 赋值实现。

## 核心类 / 函数

### shared_ptr 控制块层次

- **`_Sp_counted_base<_Lp>`**（`shared_ptr_base.h`）：抽象引用计数基类，持有 `_M_use_count` 和 `_M_weak_count`（均为 `_Atomic_word`）。声明三个纯虚 / 虚函数：`_M_dispose()`（use_count 归零时释放托管资源）、`_M_destroy()`（weak_count 归零时 `delete this` 释放控制块自身）、`_M_get_deleter()`（返回类型擦除的删除器指针）。`_Lp` 模板参数决定锁策略：`_S_atomic`（默认，使用 `__exchange_and_add_dispatch`）、`_S_mutex`（互斥锁回退）、`_S_single`（单线程，无原子操作）。
- **`_Sp_counted_ptr_inplace<_Ptr, _Alloc>`**：`make_shared` 使用的控制块，将对象存储在控制块内部的 `_M_storage` 成员中（一次分配）。`_M_dispose()` 调用 allocator 的 `destroy()` 析构对象；`_M_destroy()` 调用 allocator 的 `deallocate()` 释放整块内存。
- **`_Sp_counted_ptr<_Ptr>`**：`shared_ptr(new T(...))` 使用的控制块，仅持有裸指针，对象需独立分配（两次分配）。`_M_dispose()` 直接 `delete _M_ptr` 或调用自定义删除器。

### RB-tree 核心

- **`_Rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>`**（`stl_tree.h`）：红黑树完整实现，所有 `set`/`map`/`multiset`/`multimap` 均内嵌此类。模板参数 `_KeyOfValue` 是一个 functor，从 `_Val` 中提取键（`set` 中为 identity，`map` 中为 `select1st`）。
- **`_Rb_tree_node<_Val>`**：继承自 `_Rb_tree_node_base`，附加 `_M_storage`（`__aligned_membuf<_Val>`）存储实际值。节点布局：`_M_color`(1B + padding) + `_M_parent`(8B) + `_M_left`(8B) + `_M_right`(8B) + `_M_storage`。
- **`_Rb_tree::_M_header`**（`_Rb_tree_header`）：哨兵节点，`_M_parent` 指向根节点，`_M_left` 指向最左节点（`begin()`），`_M_right` 指向最右节点。空树时 `_M_left = _M_right = &_M_header`。`end()` 迭代器指向此哨兵。

### std::function 核心

- **`_Function_base`**（`std_function.h`）：持有 `_Any_data _M_functor`（SBO 缓冲区，`sizeof(_Nocopy_types)` = 3 × `sizeof(void*)` = 24 字节）和 `_Manager_type _M_manager`（函数指针，管理拷贝/销毁/类型查询操作）。析构函数通过 `_M_manager(..., __destroy_functor)` 释放目标。
- **`_Function_handler<_Res(_ArgTypes...), _Functor>`**：继承 `_Function_base::_Base_manager<_Functor>`，提供静态 `_M_invoke()` 方法——通过 `std::__invoke_r<_Res>` 调用存储的可调用对象。`_M_manager()` 处理 `__clone_functor`（拷贝）和 `__destroy_functor`（析构）操作。
- **`_M_invoker`**：`function` 类中的函数指针，指向 `_Function_handler::_M_invoke` 的特化实例。`operator()` 直接调用 `_M_invoker(_M_functor, args...)`，无虚函数开销。

## 关键算法

### shared_ptr 引用计数生命周期

1. **构造**：`_M_use_count = 1`，`_M_weak_count = 1`（弱引用基线值，保证 use_count 未归零前控制块不被释放）。
2. **拷贝 `_M_add_ref_copy()`**：`__exchange_and_add_dispatch(&_M_use_count, 1)`——原子自增，返回旧值用于溢出检查（`_S_chk`）。
3. **析构 `_M_release()`**：`__exchange_and_add_dispatch(&_M_use_count, -1)`——原子自减，若旧值 == 1（即减后归零），调用 `_M_release_last_use()`。
4. **`_M_release_last_use()`**：先调用 `_M_dispose()` 析构托管对象；然后 `_M_weak_count` 原子自减 1（取消基线值），若旧值 == 1（即弱引用也归零），调用 `_M_destroy()` 释放控制块内存。
5. **`weak_ptr` 锁定 `_M_add_ref_lock()`**：CAS 循环检查 `_M_use_count > 0` 时原子自增，否则抛 `bad_weak_ptr`。

### make_shared 单次分配路径

`make_shared<T>(args...)` → 分配 `sizeof(_Sp_counted_ptr_inplace<T>)` 的内存 → placement new 构造控制块 + 对象。`_M_dispose()` 析构对象但不释放内存；`_M_destroy()` 通过 allocator 释放整块。对比 `shared_ptr(new T(...))`：先 `new T` 再 `new _Sp_counted_ptr<T>`，两次独立分配，无法利用局部性且有额外 malloc 开销。

### RB-tree 插入与再平衡

`_M_insert_unique(__v)` 流程：从根开始 BST 搜索找到插入位置（O(log n)）→ `_M_create_node()` 分配并构造节点 → 设置 parent/child 指针 → 若树非空，将节点挂载到正确位置 → `_Rb_tree_insert_and_rebalance()` 再平衡。再平衡规则：新节点着红色，若父节点也红色则违反红黑性质，需通过旋转（左旋 / 右旋）和重新着色修复。最多 2 次旋转即可恢复平衡。

hint 插入（`insert_unique(iterator __position, __v)`）：若 hint 指向 `begin()` 且 `__v < *begin()`，或 hint 指向 `end()` 且 `__v > *--end()`，可 O(1) 直接插入。否则 fallback 到无 hint 的 O(log n) 路径。

### std::function 分发路径

赋值时 `_M_init_functor()` 决定存储策略：若 `sizeof(_Functor) <= _M_max_size` 且 `alignof(_Functor) <= _M_max_align` 且满足 `is_trivially_copyable`（`__is_location_invariant`），则 `_M_create(__dest, __f, true_type)` —— placement new 到 SBO 缓冲区。否则 `_M_create(__dest, __f, false_type)` —— `new _Functor` 堆分配，SBO 中仅存指针。调用时 `_M_invoker` 直接取 SBO 中的对象（或解引用指针）并 `__invoke_r`。

## ABI 约束

- **控制块虚函数布局**：`_Sp_counted_base` 有虚析构函数、`_M_dispose()`、`_M_destroy()`、`_M_get_deleter()` 四个虚函数，每个控制块实例携带一个 vtable 指针（8B on x86-64）。vtable 中虚函数的偏移顺序是 ABI 固定的——任何虚函数的增删或重排都会导致跨 DSO 的 ODR 违规和崩溃。`_Sp_counted_ptr` 和 `_Sp_counted_ptr_inplace` 作为派生类各自有独立 vtable。
- **`std::function` SBO 大小**：`_Any_data` 定义为 `aligned_storage<3 * sizeof(void*)>`，在 LP64 上为 24 字节。但实际 SBO 判定阈值 `_M_max_size = sizeof(_Nocopy_types)` 也为 24 字节。这意味着 `sizeof(std::function)` = 24（SBO 缓冲区）+ 8（`_M_invoker`）+ 8（`_M_manager`）= 40 字节。增大 SBO 缓冲区会改变 `sizeof(std::function)`，破坏 ABI 兼容性——所有已编译的 `std::function` 实例的偏移量都会错位。
- **RB-tree 节点布局**：`_Rb_tree_node_base` 的字段顺序（`_M_color` / `_M_parent` / `_M_left` / `_M_right`）和对齐方式是 ABI 固定的。`_Rb_tree_node<_Val>` 继承 `_Rb_tree_node_base` 后附加 `_M_storage`。任何字段重排或类型变更都会破坏与已编译代码的节点偏移兼容性。`_Rb_tree_header`（`_M_header` + `_M_node_count`）的布局同样固定——迭代器通过 `_M_node` 指针直接访问节点字段，编译期偏移量已硬编码。
- **`_Lock_policy` 模板参数**：控制块的 `_Lp` 参数（`_S_atomic` / `_S_mutex` / `_S_single`）影响原子操作的选择。默认 `__default_lock_policy = _S_atomic`，但在某些嵌入式平台上可能为 `_S_single`。不同策略的控制块实例有不同的 vtable，混用会导致 UB。

## 异常安全

- **`make_shared`**：强异常安全保证（strong guarantee）。单次内存分配 + placement new 构造对象，若 `T` 的构造函数抛异常，`_Sp_counted_ptr_inplace` 的构造函数会通过 allocator 释放已分配的内存，不会泄漏。相比 `shared_ptr(new T(...))` 的两次分配路径——若第二次分配（控制块）失败，已构造的 `T` 会被自定义删除器正确析构（或 `delete`），但整体仅提供基本保证。
- **`shared_ptr` 拷贝**：`noexcept`。`_M_add_ref_copy()` 仅为原子自增操作，不分配内存，不调用用户代码，不抛异常。
- **RB-tree 插入**：强异常安全保证。`_M_create_node()` 先分配节点内存再构造值（`_M_construct_node()`），若值构造抛异常，在 catch 块中 `_M_put_node()` 释放节点内存后重新抛出，树状态不变。再平衡阶段（`_Rb_tree_insert_and_rebalance()`）仅操作指针和枚举值，不会抛异常。
- **`std::function` 赋值**：基本异常安全保证（basic guarantee）。`_M_init_functor()` 中若 `_Functor` 的拷贝构造抛异常，`function` 对象会变为空状态（`_M_manager == nullptr`），之前的可调用对象已通过 `__destroy_functor` 销毁。不满足强保证，因为赋值无法回滚到之前的可调用对象（旧对象已被 destroy）。C++23 的 `std::move_only_function` 同样仅提供基本保证。

## iterator / reference invalidation

- **`shared_ptr`**：不涉及迭代器概念。`shared_ptr` 的拷贝、赋值、`reset()` 等操作对同一控制块的其他 `shared_ptr` / `weak_ptr` 实例完全安全。`weak_ptr::lock()` 返回的 `shared_ptr` 始终有效（若对象仍存活）。注意：`shared_ptr` 本身不是线程安全的——对同一 `shared_ptr` 实例的并发读写需要外部同步；但不同 `shared_ptr` 实例指向同一控制块时，各自操作是线程安全的（原子引用计数保证）。
- **RB-tree**：**插入（`insert` / `emplace`）不使任何已有迭代器或引用失效**——新节点作为叶节点附加到树上，不移动已有节点。**删除（`erase`）仅使指向被删元素的迭代器失效**——libstdc++ 实现中，删除有两个子节点的节点时采用 successor relink（链接后继节点到被删位置）而非 copy，因此其他节点的迭代器保持有效（标准 §[associative.reqmts]/8 要求）。**整体容器操作**：`clear()` 使所有迭代器失效；`swap()` 不使迭代器失效（迭代器跟随节点移动）。
- **`std::function`**：`std::function` 不暴露迭代器。当通过 `operator=` 或 `swap()` 替换目标可调用对象时，先前通过引用捕获或指向旧目标内部状态的外部引用将悬空——旧对象已被 `_M_destroy(..., __destroy_functor)` 析构。`std::function` 拷贝时通过 `__clone_functor` 创建目标的独立副本，因此两个 `function` 实例的内部状态互不影响。

## 性能模型

- **`shared_ptr` 原子引用计数**：拷贝时 `_M_add_ref_copy()` 执行 `__exchange_and_add_dispatch`（编译为 `lock xadd` on x86），内存序为 `memory_order_acq_rel`（隐含在 `__exchange_and_add_dispatch` 中）。析构时 `_M_release()` 同样 `lock xadd`，减到零后进入冷路径（`__attribute__((noinline))` 标记的 `_M_release_last_use_cold()`），调用 `_M_dispose()` + 可能的 `_M_destroy()`。原子操作的开销：在无竞争场景下约 5-20ns（x86），有竞争时可能退化到 cache line bouncing（MESI 协议），在多核高并发场景下成为瓶颈。`weak_ptr::lock()` 的 CAS 循环在竞争激烈时额外增加开销。
- **RB-tree 指针追逐**：树操作（`find` / `insert` / `erase`）的时间复杂度 O(log n)，但每次比较需要从父节点跟随 `_M_left` 或 `_M_right` 指针跳转到子节点，造成 cache miss。在现代 CPU 上，一次 cache miss 约 50-100ns，因此对于 n = 1000 的树（深度 ~10），一次查找可能触发 10 次 cache miss，远超 flat array 的顺序访问。这也是 `std::flat_map`（C++23）在小数据集上可能优于 `std::map` 的原因。
- **`std::function` SBO 与间接调用**：libstdc++ 的 SBO 判定条件为 `sizeof(_Functor) <= 24 && alignof(_Functor) <= alignof(void*) && is_trivially_copyable`。满足时避免堆分配（省去 `new` + `delete` 开销，约 50-200ns）。调用路径：`operator()` → `_M_invoker(_M_functor, args...)` → `__invoke_r`。虽然经过函数指针间接调用（一次间接跳转，可能触发分支预测失败），但比虚函数调用少一次指针解引用（vtable 查找）。典型函数指针 / 小 lambda 的 SBO 命中率接近 100%。

## libstdc++ vs libc++ vs MSVC

| 维度 | libstdc++ (GCC) | libc++ (Clang) | MSVC STL |
|------|----------------|---------------|----------|
| **控制块基类** | `_Sp_counted_base<_Lp>`，含 `_M_dispose()` / `_M_destroy()` 虚函数 | `__shared_count` / `__shared_weak_count`，虚函数接口类似但类名不同 | `_Ref_count_base`，虚函数接口与 libstdc++ 类似 |
| **make_shared 路径** | `_Sp_counted_ptr_inplace`，一次分配 | `__shared_ptr_emplace`，一次分配 | `_Ref_count_obj`，一次分配 |
| **原子操作** | `__exchange_and_add_dispatch` → `__atomic_fetch_add` | `__libcpp_atomic_refcount_*` → `__c11_atomic_*` | `_Atomic_*` 或编译器内置 |
| **锁策略模板参数** | `_Lock_policy`（`_S_atomic` / `_S_mutex` / `_S_single`） | 无模板参数，始终原子 | 无模板参数，始终原子 |
| **RB-tree 节点布局** | `_Rb_tree_node_base`：`color + parent + left + right`，节点继承后附加 `_M_storage` | `__tree_node_base` / `__tree_end_node`：`__left_ + __right_ + __parent_ + __is_black_`（字段顺序不同） | `_Tree_node`：`_Color + _Parent + _Left + _Right + _Myval` |
| **`std::function` SBO 大小** | 24B（`3 * sizeof(void*)`），`sizeof(std::function)` = 40B | 24B（`3 * sizeof(void*)`），但 `sizeof(std::function)` = 32B（布局不同） | ≥ 6 * `sizeof(void*)`（实现相关），`sizeof(std::function)` 更大 |
| **`std::function` 分发方式** | 函数指针（`_M_invoker` + `_M_manager`） | 函数指针（`__f_` + `__buf_`） | 函数指针或内部 vtable（实现相关） |
| **虚函数 vs 函数指针** | shared_ptr 用虚函数，function 用函数指针 | shared_ptr 用虚函数，function 用函数指针 | shared_ptr 用虚函数，function 用函数指针 |

**关键差异总结**：三家的 `shared_ptr` 控制块都使用虚函数实现多态，但类名和锁策略参数不同。RB-tree 节点字段顺序各异（libstdc++ 的 color 在首位，libc++ 的 `__is_black_` 在末尾），因此跨库的节点指针强转会导致 UB。`std::function` 的 SBO 缓冲区大小差异最大——libstdc++ 的 24B 可容纳大多数 lambda，而 MSVC 的更大缓冲区可容纳更多场景，但代价是 `sizeof(std::function)` 更大。

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

### shared_ptr 原子操作汇编

```bash
g++ -std=c++20 -O2 -S shared_ptr_copy.cpp -o shared_ptr_copy.s
# 观察 _M_add_ref_copy 编译为 lock xadd 指令
```

`shared_ptr` 拷贝的核心汇编（x86-64）：

```asm
; _M_add_ref_copy —— 原子自增
lock add    DWORD PTR [rax], 1   ; 或 lock xadd
```

析构路径 `_M_release()`：

```asm
; _M_release —— 原子自减并检查归零
lock sub    DWORD PTR [rax], 1
jne         .Lskip_dispose       ; 未归零则跳过
call        _M_release_last_use_cold  ; 冷路径，noinline
```

### RB-tree 插入热路径

```bash
g++ -std=c++20 -O2 -S rbtree_insert.cpp -o rbtree_insert.s
# 观察 BST 搜索 + _Rb_tree_insert_and_rebalance 调用
```

`_M_insert_unique` 的 BST 搜索部分：

```asm
; 从根节点开始，沿 left/right 指针下降
.Lsearch_loop:
    mov     rdx, [rax+16]       ; load _M_left
    mov     rcx, [rax+24]       ; load _M_right
    cmp     edi, [rax+32]       ; 比较 key
    jge     .Lgo_right
    mov     rax, rdx            ; 走左子树
    jmp     .Lsearch_loop
```

### std::function SBO vs 堆分配

```bash
g++ -std=c++20 -O2 -S function_call.cpp -o function_call.s
# 对比 SBO 路径（直接调用）与堆路径（多一次指针解引用）
```

SBO 路径（目标 lambda ≤ 24B）：

```asm
; _M_invoker 直接调用 SBO 中的可调用对象
lea     rdi, [rbx+8]          ; &_M_functor._M_pod_data
call    [rbx+32]              ; 通过 _M_invoker 函数指针调用
```

堆路径（目标 lambda > 24B）：

```asm
; 需先从 SBO 中取出堆指针再解引用
mov     rdi, [rbx+8]          ; load heap pointer from _M_functor
call    [rbx+32]              ; 通过 _M_invoker 函数指针调用
```

### weak_ptr lock 路径

```bash
objdump -d a.out | grep -A20 '_M_add_ref_lock_nothrow'
```

关键：CAS 循环检查 `_M_use_count > 0` 后原子自增，失败则返回 false（触发 `bad_weak_ptr`）。

### Benchmark 提示

```bash
# shared_ptr 拷贝吞吐量（单线程 vs 多线程竞争）
g++ -std=c++20 -O2 -pthread bench_shared_ptr.cpp -o bench
./bench --benchmark_filter="BM_SharedPtr"

# RB-tree vs flat_map 查找（cache 效果）
g++ -std=c++20 -O2 bench_tree.cpp -o bench
./bench --benchmark_filter="BM_TreeLookup"

# std::function SBO 命中率
g++ -std=c++20 -O2 bench_function.cpp -o bench
./bench --benchmark_filter="BM_Function"
```

## cpplings 练习入口

- [`smartptr2` — shared_ptr 与 weak_ptr](../../../exercises/cpp11-classes/smartptr2.cpp)
- [`smartptr1` — unique_ptr](../../../exercises/cpp11-classes/smartptr1.cpp)
- [`movonlyfunc1` — move_only_function 移动专用可调用包装器](../../../exercises/cpp23/movonlyfunc1.cpp)
