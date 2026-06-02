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

待补：把 `vector` 的连续存储/扩容语义，以及 `unordered_map` 的 bucket、load factor、rehash 语义，逐条对齐到标准要求。

## 对象布局

上文已经覆盖 `vector` 的三指针布局与 `_Hashtable` 的 bucket + node 结构；后续补一张统一的对象内存对照表。

## 核心源码路径

`vector` 侧待补 `references/impl/gcc/libstdc++-v3/include/bits/stl_vector.h` 与 `vector.tcc`；`unordered_map` 侧本文已给出 `hashtable.h` / `hashtable_policy.h`，后续补 include 链与入口函数。

## 核心类 / 函数

待补：统一整理 `_Vector_impl_data`、`_M_check_len`、`std::_Hashtable`、`_Hash_node`、`_Prime_rehash_policy` 这些正文已出现的关键类型与入口函数。

## 关键算法

待补：把 `vector` 扩容搬移、`unordered_map` bucket 定位与链式查找整理成一张“触发条件 → 算法路径”表。

## ABI 约束

待补：说明 `vector`/`unordered_map` 公开 ABI 关注的是对象大小、迭代器语义与异常边界，而内部节点类型和 rehash policy 变更必须受这些承诺约束。

## 异常安全

待补：补充 `vector` 在扩容时依赖 `move_if_noexcept` 的强/基本保证，以及 `_Hashtable` 在插入、rehash、节点分配失败时的回滚路径。

## iterator / reference invalidation

待补：明确 `vector` 在 reallocate 后 iterator / pointer / reference 全失效，而节点式 `_Hashtable` 在 `rehash` 后 iterator 失效但未删除元素的引用通常保持稳定。

## 性能模型

上文已经给出“连续内存 vs 节点追逐”的核心差异；后续补 cache line、分配次数、hash 命中链长与扩容成本的统一性能模型。

## libstdc++ vs libc++ vs MSVC

待补：把正文已有的 `vector` 增长/搬移差异，和三家 `unordered_*` 在节点布局、bucket 策略、调试迭代器上的差别放进统一对照表。

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

待补：补上 `vector` 扩容热路径与 `_Hashtable::find` 的编译器输出、反汇编片段，以及和 libc++ / MSVC 的 microbenchmark 对照。

## cpplings 练习入口

- [`unordered1` — 无序容器 (unordered_map / unordered_set)](../../../exercises/cpp11-std/unordered1.cpp)
- [`customhash1` — 自定义哈希](../../../exercises/cpp11-std/customhash1.cpp)
- [`noexcept1` — noexcept 的影响：移动语义与 vector`](../../../exercises/topics/noexcept1.cpp)
- [`cachefriendly1` — 缓存友好的数据结构](../../../exercises/topics/cachefriendly1.cpp)
