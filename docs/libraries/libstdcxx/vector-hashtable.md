---
title: libstdc++ vector 与 unordered_map _Hashtable
topic: libraries
feature: vector-hashtable
standard: C++11
status_checked_at: 2026-06-02
implementation:
  libstdcxx:
    paths:
      - references/impl/gcc/libstdc++-v3/include/bits/stl_vector.h
      - references/impl/gcc/libstdc++-v3/include/bits/hashtable.h
      - references/impl/gcc/libstdc++-v3/include/bits/hashtable_policy.h
    symbols:
      - std::vector
      - std::_Hashtable
      - __detail::_Hash_node
      - _Prime_rehash_policy
exercises:
  - exercises/cpp11-std/unordered1.cpp
  - exercises/cpp11-std/customhash1.cpp
solutions:
  - exercises/solutions/unordered1.cpp
  - exercises/solutions/customhash1.cpp
---
# libstdc++ vector 与 unordered_map _Hashtable

## std::vector

### 三指针布局

```cpp
struct _Vector_impl_data {
  pointer _M_start;           // 数据起始
  pointer _M_finish;          // size = _M_finish - _M_start
  pointer _M_end_of_storage;  // capacity = _M_end_of_storage - _M_start
};
// sizeof(vector<T>) = 24（与 libc++ 完全相同）
```

### 增长公式

```cpp
size_type _M_check_len(size_type __n) const {
  const size_type __len = size() + std::max(size(), __n);
  // push_back: 新容量 = size + size = 2×size（与 libc++ 相同）
  // insert(n个): 新容量 = size + max(size, n)
  return (__len < size() || __len > max_size()) ? max_size() : __len;
}
```

**与 libc++ 的差异**：libstdc++ 始终走 move_if_noexcept + destroy 路径，没有 libc++ 的 trivially relocatable memcpy 优化。

### 中间插入：三段复制

```cpp
// 无 split_buffer 概念，直接三段操作：
// 1. move 后半段到新位置
// 2. 构造新元素
// 3. move 前半段（如果有扩容）
```

## std::unordered_map：节点式 `_Hashtable`

> 源码路径：`references/impl/gcc/libstdc++-v3/include/bits/hashtable.h`
>
> `references/impl/gcc/libstdc++-v3/include/bits/hashtable_policy.h`

libstdc++ 的 `std::unordered_map` / `std::unordered_set` 并没有采用 SwissTable。它们的核心实现是节点式、bucket + chaining 的 `std::_Hashtable`：

```cpp
std::_Hashtable<_Key, _Value, _Alloc, _ExtractKey, _Equal, _Hash,
                _RangeHash, _Unused, _RehashPolicy, _Traits>
```

源码里的关键别名是 `using __node_type = __detail::_Hash_node<_Value, __hash_cached::value>;`。`hashtable.h` 对整体结构的描述很直接：`_M_buckets` 是 bucket 数组，元素本体是 `__node_type`，节点之间通过 `_M_nxt` 串成单链表。可以把它理解为：

- 一个 bucket 数组，负责把 key 映射到某条链
- 一串 `__node_type` / `_Hash_node<_Value, _Traits::__hash_cached::value>` 节点，真正持有 `value_type`
- 每个 bucket 记录对应链表的入口位置

### 结构示意

```text
bucket array (_M_buckets)
[0] ──> before_begin ──> node(K1,V1) ──> node(K9,V9) ──> ...
[1] ──> nullptr
[2] ──> before_begin ──> node(K2,V2) ──> ...
...

node = _Hash_node<_Value, _Traits::__hash_cached::value>
     = next 指针 + value + （可选）缓存 hash code
```

从概念模型看，可以把 bucket 数组理解为“链表头指针数组”；源码里的真实类型是 `__buckets_ptr = __node_base_ptr*`，也就是先指向节点基类，再通过 `_M_nxt` 进入真正的 `_Hash_node`。本质上，这仍然是典型的链式哈希表，而不是开放寻址表。

### 负载因子与 rehash 策略

libstdc++ 的默认 `max_load_factor` 是 1.0，由 rehash policy 控制：

- `_Prime_rehash_policy(float __z = 1.0)`
- `_Power2_rehash_policy(float __z = 1.0)`

也就是说，默认策略是在平均每个 bucket 接近 1 个元素时扩容；这与 SwissTable 常见的 7/8 负载上限是两套完全不同的设计。

### 稳定性与代价

节点式 `_Hashtable` 的关键特征是：

- 元素对象存放在独立节点里，不会像开放寻址那样在扩容或探测过程中搬移到连续 slot
- 引用 / 指针对未删除元素保持稳定
- 迭代器实现基于节点，稳定性强于开放寻址；但按 `unordered_map` 语义，`rehash` 后仍不应继续使用旧迭代器

代价也很明确：

- 需要额外节点分配
- bucket 命中后还要跟随指针访问节点
- 没有 SIMD probing、没有 ctrl byte 数组、没有 H1/H2 分裂哈希、也没有 open addressing

换句话说，**libstdc++ 的 `std::unordered_map` 是传统节点式 `_Hashtable`；SwissTable 属于 Abseil `flat_hash_map` 这一类高性能开放寻址实现，不属于 libstdc++ `unordered_map`。**

## 与其他哈希表实现的对比

| 实现 | 基本策略 | 元信息 / 探测 | 典型负载因子 | 稳定性 |
|------|----------|---------------|--------------|--------|
| libstdc++ `_Hashtable` | 节点式 chaining，bucket + `_Hash_node` 单链表 | 无 ctrl bytes；按 bucket 找链，再沿节点遍历 | 1.0 | 引用稳定；迭代器基于节点 |
| Abseil SwissTable (`flat_hash_map`) | 开放寻址 | SIMD probing，H1/H2 分裂，ctrl byte 数组 | 7/8 | 扩容/重排会搬移 slot |
| Folly F14 | chunk-based 哈希表 | SIMD tag probing，14-element chunks | 高负载优化 | 依变体而异，不是传统节点链表 |

## 用户 API

`std::vector<T>` 面向顺序增长与随机访问，`std::unordered_map<Key, T>` 面向均摊 O(1) 的按 key 访问；现有正文已经直接下钻到两者的实现骨架，后续在这里补齐用户侧 API 入口与内部路径的映射。

## 标准语义

### `std::vector`

- **连续存储**：标准要求元素占用连续内存（`&v[n] == &v[0] + n`），`data()` 返回指向首元素的指针
- **均摊 O(1) push_back**：每次扩容时容量至少翻倍，使得 N 次 push_back 总开销 O(N)，均摊 O(1)
- **迭代器失效**：任何导致重新分配的操作（`push_back`、`insert`、`reserve`、`resize`）使所有迭代器、指针和引用失效
- **`erase` 语义**：仅使被删位置及其之后的迭代器失效；返回指向最后被删元素之后位置的迭代器
- **`shrink_to_fit`**：是 non-binding 请求（C++11 起），实现可以忽略

### `std::unordered_map`

- **均摊 O(1) 查找**：标准要求 `find`、`count`、`contains` 在平均情况下为 O(1)，最坏为 O(N)
- **bucket 接口**：标准暴露 `bucket_count()`、`bucket(key)`、`begin(n)/end(n)` 等本地迭代器接口，允许用户直接访问每个 bucket 的链表
- **`load_factor()` / `max_load_factor()`**：`load_factor() == size() / bucket_count()`；默认 `max_load_factor()` 为 1.0；用户可调用 `rehash(n)` 或 `reserve(n)` 触发 rehash
- **引用稳定性**：标准保证 `insert` 和 `rehash` 不会使已存在元素的引用或指针失效（§[unord.req.general]）；这是节点式实现的核心承诺
- **迭代器失效**：`rehash` 使所有迭代器失效，但不使引用失效；`erase` 仅使指向被删元素的迭代器失效

## 对象布局

上文已经覆盖 `vector` 的三指针布局与 `_Hashtable` 的 bucket + node 结构；后续补一张统一的对象内存对照表。

## 核心源码路径

### vector

| 文件 | 内容 |
|------|------|
| `bits/stl_vector.h` | `_Vector_base` / `_Vector_impl_data`（三指针布局）、`_Vector_impl`（继承 allocator + 三指针）、`vector` 主模板的类定义与所有 inline 成员函数 |
| `bits/vector.tcc` | 模板 out-of-line 实现：`reserve()`、`emplace_back()`、`_M_realloc_insert()`、`_M_realloc_append()`、`insert()`、`erase()`、`operator=` |
| `bits/stl_bvector.h` | `vector<bool>` 特化：`_Bit_reference`（代理引用）、`_Bit_iterator`（位级迭代器）、`_M_reallocate()`、`_M_fill_insert()` |

### unordered_map / unordered_set

| 文件 | 内容 |
|------|------|
| `bits/hashtable.h` | `_Hashtable` 主模板：数据成员（`_M_buckets`、`_M_before_begin`、`_M_element_count`）、`_M_find_node()`、`_M_find_before_node_tr()`、`_M_insert_unique_node()`、`_M_insert_bucket_begin()`、`_M_rehash()` |
| `bits/hashtable_policy.h` | 策略与节点类型：`_Hash_node_base`、`_Hash_node_value_base`、`_Hash_node`、`_Prime_rehash_policy`、`_Power2_rehash_policy`、`_Hashtable_traits`、`_ReuseOrAllocNode` |
| `bits/unordered_map.h` | `std::unordered_map` / `std::unordered_multimap` 别名，实例化 `_Hashtable` 参数 |
| `bits/unordered_set.h` | `std::unordered_set` / `std::unordered_multiset` 别名 |

## 核心类 / 函数

### vector 侧

| 类型 / 函数 | 源码位置 | 说明 |
|-------------|----------|------|
| `_Vector_impl_data` | `stl_vector.h:98` | 三个指针：`_M_start`、`_M_finish`、`_M_end_of_storage`，零构造成本 |
| `_Vector_impl` | `stl_vector.h:139` | 继承 `_Tp_alloc_type` + `_Vector_impl_data`；EBO 消除空 allocator |
| `_M_check_len(n, s)` | `stl_vector.h:2272` | 计算新容量：`size() + max(size(), n)`；超过 `max_size()` 则抛 `length_error` |
| `_M_realloc_insert(pos, arg)` | `vector.tcc:433` | 扩容三段操作：① 分配新缓冲 ② 在新位置构造目标元素 ③ `__uninitialized_move_if_noexcept_a` 搬移前后两段；最后 `_Destroy` 旧缓冲并 `_M_deallocate` |
| `_M_realloc_append(arg)` | `vector.tcc:542` | 尾部快速路径：跳过三段中的后半段搬移，直接构造 + move 前半段 |
| `_M_erase_at_end(p)` | `stl_vector.h` | 从 p 到 `_M_finish` 调用 `_Destroy`，更新 `_M_finish`；`clear()` 的底层实现 |

### unordered_map 侧

| 类型 / 函数 | 源码位置 | 说明 |
|-------------|----------|------|
| `_Hash_node_base` | `hashtable_policy.h:293` | 链表节点基类，仅含 `_M_nxt` 指针（8 字节） |
| `_Hash_node<Value, Cache>` | `hashtable_policy.h:360` | 继承 `_Hash_node_base` + `_Hash_node_value`；value 通过 `__aligned_buffer` 存放；`Cache=true` 时额外携带 `size_t _M_hash_code` |
| `_Hashtable` | `hashtable.h:189` | 主模板，10 个模板参数；CRTP 继承 `_Hashtable_base`、`_Map_base`、`_Rehash_base`、`_Hashtable_alloc` |
| `_M_find_node(bkt, key, code)` | `hashtable.h:924` | 调用 `_M_find_before_node()` 获取前驱节点，返回 `__before_n->_M_nxt` 或 `nullptr` |
| `_M_find_before_node_tr(bkt, k, code)` | `hashtable.h:2252` | 沿 bucket 链遍历，调用 `_M_equals_tr` 比较 hash code 和 key 等值；返回前驱节点或 `nullptr` |
| `_M_insert_unique_node(bkt, code, node)` | `hashtable.h:2492` | ① 调用 `_M_need_rehash()` 判断是否需要扩容 ② 若需要则 `_M_rehash()` ③ 重新计算 bucket index ④ 调用 `_M_insert_bucket_begin()` 插入链表头 |
| `_M_insert_bucket_begin(bkt, node)` | `hashtable.h:944` | 非空 bucket：在 `_M_buckets[bkt]->_M_nxt` 后插入；空 bucket：设置 `_M_before_begin._M_nxt` 并更新原 begin bucket 的指针 |
| `_Prime_rehash_policy` | `hashtable_policy.h:597` | `max_load_factor` 默认 1.0；`_M_next_bkt(n)` 返回 ≥ n 的最小素数；`_M_need_rehash()` 根据 `size()/bucket_count()` 判断是否超过负载因子 |

## 关键算法

### vector：扩容搬移

| 触发条件 | 算法路径 |
|----------|----------|
| `push_back` / `emplace_back`：`_M_finish == _M_end_of_storage` | `_M_realloc_append` → `_M_check_len(1)` → 分配 `2×size` → 构造新元素 → `__uninitialized_move_if_noexcept_a` 搬移旧元素 → `_Destroy` 旧缓冲 → 释放 |
| `insert(pos, val)`：容量不足 | `_M_realloc_insert` → `_M_check_len(1)` → 分配 → 构造新元素 → `move_if_noexcept` 前半段 + 后半段 → destroy 旧缓冲 |
| `insert(pos, first, last)`：容量不足 | `_M_check_len(n)` → `size() + max(size(), n)` → 同三段搬移，但 n 可能 > 1 |
| `_S_use_relocate() == true`（trivially relocatable 或 `noexcept_move`） | 直接 `__relocate_a`，跳过 move + destroy 两步 |

关键源码片段（`vector.tcc:433`）：

```cpp
const size_type __len1 = _M_check_len(1u, "vector::_M_realloc_insert");
_Alloc_result __r = this->_M_allocate_at_least(__len1);
// 先在新缓冲的插入位置构造目标元素
_Alloc_traits::construct(this->_M_impl, __new_start + __elems_before, ...);
// 搬移插入点之前的元素
__new_finish = __uninitialized_move_if_noexcept_a(__old_start, __position.base(), __new_start, ...);
// 搬移插入点之后的元素
__new_finish = __uninitialized_move_if_noexcept_a(__position.base(), __old_finish, __new_finish, ...);
// 释放旧缓冲
```

### unordered_map：bucket 定位与链式查找

| 触发条件 | 算法路径 |
|----------|----------|
| `find(key)` | `_M_hash_code_tr(key)` → `_M_bucket_index(hash_code)` 得到 bucket → `_M_find_before_node_tr(bkt, key, code)` → 遍历链表调用 `_M_equals_tr` 比较 |
| `insert/emplace` | `_M_compute_hash_code(key)` → `_M_bucket_index` → `_M_find_before_node_tr` 检查重复 → `_M_allocate_node` 构造 → `_M_insert_unique_node(bkt, code, node)` |
| rehash（`insert_unique_node` 内部） | `_M_need_rehash(bkt_count, elt_count, 1)` → 若需扩容：`_M_rehash(new_count)` → 分配新 bucket 数组 → 遍历所有节点重新 `_M_bucket_index` → `_M_insert_bucket_begin` |
| 小表优化（`size ≤ 20` 且 hash 是 fast hash） | `_M_locate_tr` 跳过 bucket 定位，直接线性遍历全表 |

查找核心路径（`hashtable.h:2252`）：

```cpp
// _M_find_before_node_tr：沿 bucket __bkt 的链表遍历
__node_base_ptr __prev_p = _M_buckets[__bkt];
for (__node_ptr __p = __prev_p->_M_nxt;; __p = __p->_M_next()) {
    if (_M_equals_tr(__k, __code, *__p)) return __prev_p;  // 命中
    if (!__p->_M_nxt || _M_bucket_index(*__p->_M_next()) != __bkt) break;
    __prev_p = __p;
}
return nullptr;
```

## ABI 约束

### vector ABI

- **对象大小**：`sizeof(vector<T>) = sizeof(_Vector_impl_data) + sizeof(_Tp_alloc_type)` = 24 字节（EBO 消除空 allocator 时）或 32 字节（有状态 allocator）
- **三指针布局固定**：任何改变 `_M_start` / `_M_finish` / `_M_end_of_storage` 顺序、增加成员、或改变指针类型的改动都是 ABI break
- **迭代器类型**：普通迭代器是裸指针 `T*`；`vector<bool>` 迭代器是 `_Bit_iterator`（指针 + 位偏移），改变其布局影响所有使用 `vector<bool>::iterator` 的编译单元
- **异常边界**：`push_back` / `insert` 的强保证依赖 `move_if_noexcept`；若某类型的 move 构造函数不是 `noexcept`，则回退到 copy，这影响 ABI 级别的代码生成

### unordered_map ABI

- **`_Hashtable` 数据成员**（`hashtable.h:358-370`）：
  - `__buckets_ptr _M_buckets`（8 字节）
  - `size_type _M_bucket_count`（8 字节）
  - `__node_base _M_before_begin`（8 字节，单指针）
  - `size_type _M_element_count`（8 字节）
  - `_RehashPolicy _M_rehash_policy`（`_Prime_rehash_policy` = float + size_t = 16 字节）
  - `__node_base_ptr _M_single_bucket`（8 字节）
  - 总计约 56 字节（64 位平台）
- **`_Hash_node` 布局**：`_M_nxt`（8）+ `value`（对齐后）+ 可选 `hash_code`（8）；改变节点继承链或对齐方式改变每个元素的分配大小，属于 ABI break
- **`_Prime_rehash_policy`**：`_M_max_load_factor`（float）+ `_M_next_resize`（size_t）+ `_S_growth_factor = 2`；改变素数表或增长因子改变 rehash 行为，影响所有已编译的 `unordered_*` 二进制
- **调试迭代器**：libstdc++ 的 `debug` 模式下迭代器额外携带容器指针用于失效检查，改变此结构仅影响 debug ABI

## 异常安全

### vector：强保证

`_M_realloc_insert`（`vector.tcc:433`）的异常安全策略分两层：

1. **分配失败**（`_M_allocate_at_least` 抛出 `bad_alloc`）：旧缓冲完全未动，容器状态不变 → **强保证**
2. **元素构造 / 搬移失败**：
   - `_S_use_relocate()` 为 true 时，`__relocate_a` 是 `noexcept`（bitwise move），不会抛异常
   - 否则走 `__uninitialized_move_if_noexcept_a`：对每个元素调用 `move_if_noexcept` → 若类型 move 构造是 `noexcept` 则移动，否则拷贝；**任何异常都会触发 RAII guard（`_Guard_alloc` / `_Guard_elts`）销毁已构造的新元素并释放新缓冲**
   - 异常在旧缓冲的元素上重新传播 → 旧容器完全不变 → **强保证**
3. **关键依赖**：用户类型的 move 构造函数是否标记 `noexcept` 决定了扩容时是移动还是拷贝；未标记 `noexcept` 的类型在扩容时回退到拷贝，性能下降但仍保证异常安全

### `_Hashtable`：基本保证

`_M_insert_unique_node`（`hashtable.h:2492`）的异常路径：

1. **`_M_need_rehash` 不抛异常**：纯算术运算（比较、乘法），`noexcept`
2. **`_M_rehash` 抛异常**（bucket 分配失败）：`__rehash_guard_t` RAII 对象在栈展开时恢复 `_M_next_resize`，但已分配的节点由 `_Scoped_node` 释放 → 容器不变 → **强保证**
3. **`_M_allocate_node` 抛异常**（节点构造失败）：`_Scoped_node` 确保已分配的节点被释放；容器 `_M_element_count` 未变 → **强保证**
4. **hash / key_equal 抛异常**：在 `_M_insert_unique_node` 内部的 `_M_store_code` / `_M_insert_bucket_begin` 不会调用用户代码（hash code 已在外部计算）；若 `_M_hash_code_tr`（在 insert 入口处）抛异常，容器完全不变 → **强保证**
5. **总结**：libstdc++ 的 `_Hashtable` 在所有异常路径上都能做到至少基本保证；对于 `insert` 单元素操作，在 hash 计算成功后的路径中实质上达到强保证

## iterator / reference invalidation

### vector

| 操作 | 迭代器 | 指针/引用 |
|------|--------|-----------|
| `push_back` / `emplace_back`（触发扩容） | **全部失效** | **全部失效** |
| `push_back` / `emplace_back`（未扩容） | 不失效 | 不失效 |
| `insert(pos, val)`（触发扩容） | **全部失效** | **全部失效** |
| `insert(pos, val)`（未扩容） | **pos 之后失效** | **pos 之后失效** |
| `erase(pos)` | **pos 之后失效** | **pos 之后失效** |
| `erase(first, last)` | **last 之后失效** | **last 之后失效** |
| `reserve(n)`（n > capacity） | **全部失效** | **全部失效** |
| `shrink_to_fit` | **全部失效**（若实际收缩） | **全部失效**（若实际收缩） |
| `clear()` | 不返回迭代器；所有迭代器语义上失效 | 不失效（内存未释放） |

核心原因：vector 的三指针布局意味着任何重新分配都会替换 `_M_start`，导致所有基于旧指针的迭代器指向已释放内存。

### unordered_map

| 操作 | 迭代器 | 引用/指针 |
|------|--------|-----------|
| `insert` / `emplace`（触发 rehash） | **全部失效** | **不失效**（节点独立分配） |
| `insert` / `emplace`（未触发 rehash） | 不失效 | 不失效 |
| `erase(pos)` | **仅 pos 失效** | 仅被删元素失效 |
| `rehash(n)` | **全部失效** | **不失效** |
| `reserve(n)` | **全部失效**（若触发 rehash） | **不失效** |
| `clear()` | **全部失效** | **全部失效** |

关键差异：`_Hashtable` 的节点式设计使得 rehash 只改变 bucket 数组中的指针指向，节点对象本身（`_Hash_node`）的地址从未改变，因此引用和指针始终保持有效。这是 C++ 标准 §[unord.req.general] 明确要求的：`rehash` 仅使迭代器失效，不使引用失效。

实现细节：`_M_rehash`（`hashtable.h`）在 rehash 时会：① 分配新 bucket 数组 → ② 遍历所有节点，用 `_M_bucket_index` 重新计算 bucket → ③ `_M_insert_bucket_begin` 重建链表 → ④ 释放旧 bucket 数组。节点对象全程不被移动或销毁。
## 性能模型

上文已经给出“连续内存 vs 节点追逐”的核心差异；后续补 cache line、分配次数、hash 命中链长与扩容成本的统一性能模型。

## libstdc++ vs libc++ vs MSVC

### vector

| 维度 | libstdc++ | libc++ | MSVC STL |
|------|-----------|--------|----------|
| 对象大小 | 24 字节（3 指针） | 24 字节（3 指针） | 24 字节（3 指针） |
| 增长因子 | 2× | 2× | 1.5× |
| 搬移策略 | `move_if_noexcept` + destroy；trivially relocatable 类型走 `__relocate_a` | trivially copyable 类型走 `memcpy` 优化；其余 move + destroy | 类似 libstdc++，无 memcpy 优化 |
| `vector<bool>` | `stl_bvector.h` 特化，`_Bit_iterator`（指针 + 位偏移） | 类似位压缩特化 | 类似位压缩特化 |
| 调试迭代器 | `_GLIBCXX_DEBUG` 宏启用 debug 模式，迭代器额外关联容器 | 无内建调试迭代器 | `_ITERATOR_DEBUG_LEVEL=1/2`，迭代器持有容器和序列号 |
| ASan 支持 | `__sanitizer_annotate_contiguous_container` 标记已用/未用区域 | 类似 ASan annotation | 类似 ASan annotation |

### unordered_map

| 维度 | libstdc++ | libc++ | MSVC STL |
|------|-----------|--------|----------|
| 核心结构 | `_Hashtable`：bucket 数组 + `_Hash_node` 单链表 | `_HashTable`：bucket 数组 + 节点链表（`__hash_node`），结构相似但细节不同 | `_Hash`：bucket 数组 + 节点链表 |
| bucket 策略 | `_Prime_rehash_policy`：素数 bucket count，默认 max_load_factor=1.0 | 类似素数策略，默认 max_load_factor=1.0 | 2 的幂次 bucket count |
| hash cache | 可选：`_Hash_node_code_cache<true>` 缓存 `size_t _M_hash_code` | 默认缓存 hash code（`__hash_cached`） | 通常不缓存 hash code |
| 节点分配 | `_ReuseOrAllocNode`：erase 的节点可被后续 insert 复用 | 无复用池，每次 allocate/deallocate | 无复用池 |
| 调试迭代器 | `_GLIBCXX_DEBUG` 下迭代器持有容器指针，检查失效使用 | 无内建调试迭代器 | `_ITERATOR_DEBUG_LEVEL` 下迭代器持有容器指针 + 序列号 |
| 小表优化 | `size ≤ __small_size_threshold`（默认 20）时线性扫描跳过 bucket 定位 | 无此优化 | 无此优化 |
| `local_iterator` | 支持，沿 bucket 链表遍历 | 支持 | 支持 |

## 最小复现代码

```cpp
#include <unordered_map>
#include <vector>

int main() {
  std::vector<int> v;
  v.reserve(4);
  v.push_back(1);

  std::unordered_map<int, int> m;
  m.emplace(1, 42);
}
```

## 编译 / 反汇编 / benchmark 证据

### 查看 vector 扩容热路径的汇编

```bash
# 生成 vector push_back 扩容路径的汇编
g++ -std=c++20 -O2 -S -o vector_push.s vector_push.cpp

# 反汇编查看 _M_realloc_insert / _M_realloc_append 的实现
objdump -d -C a.out | grep -A30 '_M_realloc_insert\|_M_realloc_append'
```

关键汇编特征（x86-64，`-O2`）：
- `_M_realloc_insert` 会调用 `_M_check_len` → `operator new` → `__uninitialized_move_if_noexcept_a` 循环（`rep movsb` 或逐元素 move）→ `operator delete`
- `_M_realloc_append` 类似但跳过后半段搬移
- `_S_use_relocate()` 为 true 时走 `__relocate_a`，可见更紧凑的 `rep movsb` 序列

### 查看 `_Hashtable::find` 的汇编

```bash
# 生成 unordered_map find 路径的汇编
g++ -std=c++20 -O2 -S -o unordered_find.s unordered_find.cpp

# 反汇编查看 _M_find_node / _M_find_before_node_tr
objdump -d -C a.out | grep -A20 '_M_find_before_node_tr\|_M_find_node'
```

关键汇编特征：
- `_M_hash_code_tr` → 调用用户 `hash` 函数
- `_M_bucket_index(hash_code)` → `hash_code % bucket_count`（libstdc++ 用 `_RangeHash` 即模运算）
- `_M_find_before_node_tr` 内部是链表遍历循环：加载 `_M_nxt` → 比较 hash code（若缓存）→ 比较 key → 沿 `_M_nxt` 前进或跳出

### microbenchmark 对照框架

```bash
# Google Benchmark 示例：vector push_back 扩容
# unordered_map find 随机访问
# 对比 libstdc++ / libc++（需 -stdlib=libc++）/ MSVC
```

典型对照维度：

| 场景 | libstdc++ | libc++ | MSVC |
|------|-----------|--------|------|
| vector push_back 1M 元素 | 2× 增长，约 log₂(1M) ≈ 20 次 realloc | 同 libstdc++，trivially copyable 类型 memcpy 更快 | 1.5× 增长，约 33 次 realloc，总搬移字节略多 |
| unordered_map find（1M 元素，均匀分布） | 每次 find 一次 hash + 桶内 1-2 次比较 | 类似 | 类似，2 的幂 bucket 可能略微减少 rehash 开销 |
| unordered_map insert + erase 循环 | `_ReuseOrAllocNode` 复用节点减少 malloc | 每次 malloc/free | 每次 malloc/free |


## cpplings 练习入口

- [`unordered1` — 无序容器 (unordered_map / unordered_set)](../../../exercises/cpp11-std/unordered1.cpp)
- [`customhash1` — 自定义哈希](../../../exercises/cpp11-std/customhash1.cpp)
- [`noexcept1` — noexcept 的影响：移动语义与 vector`](../../../exercises/topics/noexcept1.cpp)
- [`cachefriendly1` — 缓存友好的数据结构](../../../exercises/topics/cachefriendly1.cpp)
