---
title: range-v3 View 实现
topic: libraries
feature: range-v3-views
standard: C++20
status_checked_at: 2026-06-02
implementation:
  libcxx:
    paths:
      - references/impl/llvm-project/libcxx/include/__ranges/filter_view.h
      - references/impl/llvm-project/libcxx/include/__ranges/transform_view.h
      - references/impl/llvm-project/libcxx/include/__ranges/take_view.h
    symbols:
      - filter_view
      - transform_view
      - take_view
      - view_interface
exercises: []
solutions: []
---
# range-v3 View 实现

> 源码路径：`references/impl/llvm-project/libcxx/include/__ranges/filter_view.h`, `transform_view.h`, `take_view.h`

## View 惰性求值数据流

```
  vec | views::filter(pred) | views::transform(fn) | views::take(5)

  构造阶段 (零开销, 仅组装描述):
  +-----------+     +----------------+     +-----------------+     +----------+
  |   vec     |---->| filter_view    |---->| transform_view  |---->| take_view|
  | (底层数据)|     | 存储: pred     |     | 存储: fn        |     | 存储: 5  |
  +-----------+     | 存储: &vec     |     | 存储: &filter   |     | count    |
                    +----------------+     +-----------------+     +----------+

  迭代阶段 (按需驱动, pull-based):
  遍历 take_view → 调用 transform_view::operator*() → 调用 filter_view::operator++()
  → 内部 ranges::find_if 逐个扫描, 跳过不匹配元素

  无中间存储, 无堆分配, 每个元素经过 filter → transform → take 才输出
```

## filter_view

```cpp
template <input_range _View, indirect_unary_predicate<iterator_t<_View>> _Pred>
class filter_view : public view_interface<filter_view<_View, _Pred>> {
  _View __base_ = _View();
  __movable_box<_Pred> __pred_;           // optional 存储谓词
  __non_propagating_cache<iterator_t<_View>> __cached_begin_;  // forward_range 时缓存
};
```

**begin() 实现**：首次调用线性扫描找第一个匹配元素，之后缓存命中（仅 `forward_range`）。

**operator++ 跳过**：调用 `ranges::find_if` 从当前位置下一个开始找。每次 `++` 是线性扫描，但总开销 O(n)——每个元素恰好访问一次。

## transform_view

```cpp
template <input_range _View, copy_constructible _Fn>
class transform_view : public view_interface<transform_view<_View, _Fn>> {
  _View __base_ = _View();
  __movable_box<_Fn> __fn_;
};
```

**operator* 实现**：直接对底层迭代器解引用并应用函数，无缓存。

```cpp
constexpr auto operator*() const {
  return std::invoke(*__parent_->__fn_, *__current_);
}
```

## 多次遍历陷阱

```cpp
auto v = vec | views::filter(pred);
auto sz = ranges::distance(v);   // 第一次遍历: O(n)
for (auto&& x : v) {}            // 第二次遍历: O(n) 重头来!
// 每次迭代都从头执行整条链，view 无状态缓存
```

## 标准语义

range-v3 views 遵循 C++20 ranges 标准语义，核心概念包括：

- **viewable_range**：要么是左值 range，要么是 view 本身。右值非 view range 不满足此概念，防止悬垂迭代器。
- **view**：满足 `range` 且可默认构造、移动构造、可拷贝（C++20 要求），且无额外拷贝开销（通过 `enable_view` trait 标记）。
- **borrowed_range**：迭代器在 range 销毁后仍有效。`take_view` 通过 `enable_borrowed_range` 特化继承底层 range 的 borrowed 属性。
- **lazy evaluation**：所有 view 适配器在构造时仅存储描述符（谓词、函数、计数），遍历时才逐元素执行操作，无中间容器分配。

view_interface<Derived> 作为 CRTP 基类，为所有 view 提供默认实现：
- `empty()`：优先使用 `size() == 0`（若 sized_range），否则 `begin() == end()`
- `operator bool()`：委托给 `!empty()`
- `front()` / `back()`：分别要求 forward_range / bidirectional_range + common_range
- `operator[]`：要求 random_access_range，委托给 `begin()[index]`
- `data()`：要求 contiguous_iterator，返回 `to_address(begin())`
- `size()`：要求 forward_range + sized_sentinel_for，返回 `end() - begin()`

## 核心源码路径

```
libcxx/include/__ranges/
├── view_interface.h          # CRTP 基类，提供 empty/front/back/operator[]/size
├── range_adaptor.h           # __range_adaptor_closure + __pipeable + operator|
├── filter_view.h             # filter_view: 存储谓词 + 缓存 begin 迭代器
├── transform_view.h          # transform_view: 存储变换函数 + 延迟应用
├── take_view.h               # take_view: 计数 + counted_iterator/sentinel
├── movable_box.h             # __movable_box: optional 包装，用于存储谓词/函数
├── non_propagating_cache.h   # __non_propagating_cache: 缓存 begin 迭代器
├── all.h                     # views::all_t: 将 range 转为 view（ref_view/owning_view）
└── concepts.h                # view/range/borrowed_range 等概念定义
```

## 核心类 / 函数

### filter_view<View, Pred>
- **成员**：`__base_`（底层 view）、`__pred_`（`__movable_box<_Pred>` 存储谓词）、`__cached_begin_`（`__non_propagating_cache` 缓存首个匹配迭代器）
- **begin()**：首次调用执行 `ranges::find_if` 线性扫描找首个匹配元素，forward_range 时缓存结果
- **operator++**：内部调用 `ranges::find_if(++current, end, pred)` 跳过不匹配元素
- **operator--**：仅 bidirectional_range 可用，反向扫描直到谓词匹配

### transform_view<View, Fn>
- **成员**：`__base_`（底层 view）、`__func_`（`__movable_box<_Fn>` 存储变换函数）
- **operator***：直接 `std::invoke(*__parent_->__func_, *__current_)`，无缓存
- **value_type**：`remove_cvref_t<invoke_result_t<Fn&, range_reference_t<View>>>`，即函数返回值的纯类型
- **iterator_category**：若函数返回引用且底层是 contiguous_tag，降级为 random_access_tag；否则继承底层 category

### take_view<View>
- **成员**：`__base_`（底层 view）、`__count_`（`range_difference_t<View>` 计数值）
- **begin()**：三种策略——
  1. random_access + sized_range：直接返回 `begin(__base_)`（无需包装）
  2. sized_range：返回 `counted_iterator(begin, size)`
  3. 其他：返回 `counted_iterator(begin, count)`
- **end()**：random_access + sized_range 返回 `begin + size`（迭代器类型）；sized_range 返回 `default_sentinel`；其他返回 `__sentinel`
- **size()**：`min(ranges::size(__base_), __count_)`

### view_interface<Derived>
CRTP 基类，通过 `static_cast<Derived&>(*this)` 访问派生类的 begin()/end()，提供 empty、operator bool、front、back、operator[]、data、size 的默认实现。

### range_adaptor_closure 与 pipe operator
- `__range_adaptor_closure<_Tp>`：CRTP 标记基类，无数据成员
- `__pipeable<_Fn>`：继承 `_Fn` 和 `__range_adaptor_closure`，将任意函数对象包装为可管道化闭包
- `operator|(range, closure)`：等价于 `closure(range)`
- `operator|(closure1, closure2)`：通过 `std::compose` 组合为新 `__pipeable`，实现 `g(x) = closure2(closure1(x))`

## 关键算法

### filter_view 迭代推进
```
operator++ 实现（简化）：
  current = ranges::find_if(
    std::move(++current),          // 从下一个位置开始
    ranges::end(parent->base),     // 到底层 range 末尾
    std::ref(*parent->pred)        // 应用谓词
  )
  // 总时间复杂度 O(n)：每个元素恰好被访问一次
```

### transform_view 延迟求值
```
operator* 实现：
  return std::invoke(*parent->func, *current)
  // 每次解引用调用一次函数，无缓存
  // 若函数返回值类型（非引用），每次产生临时对象
```

### take_view 计数终止
```
sentinel 比较（非 sized_range 路径）：
  return iter.count() == 0 || iter.base() == sentinel.end()
  // counted_iterator 内部维护剩余计数，count=0 时终止
  // 即使底层未到末尾，计数归零即停止
```

### 迭代器 category 降级规则
- **filter_view**：继承底层 iterator_category，但 bidirectional 以上保持不变（filter 不增加能力）
- **transform_view**：若函数返回非引用（值类型），contiguous_tag 降级为 random_access_tag，forward_tag 降级为 input_tag
- **take_view**：使用 counted_iterator 包装，保持底层 iterator_concept

## iterator / reference invalidation

### filter_view
- **迭代器失效**：底层 range 修改后，缓存的 `__cached_begin_` 可能失效。`__non_propagating_cache` 在 view 赋值时自动清空缓存。
- **引用稳定性**：filter_view 不持有元素，引用直接指向底层 range 元素。底层元素失效则引用失效。
- **begin() 语义**：首次调用触发线性扫描，后续调用返回缓存迭代器。若底层 range 结构变化，缓存迭代器可能悬垂。

### transform_view
- **迭代器失效**：内部存储 parent 指针 + 底层迭代器。底层迭代器失效则 transform_view 迭代器失效。
- **引用稳定性**：若函数返回值类型（非引用），每次 `operator*` 产生临时对象，返回的是临时对象的引用（危险！）。若函数返回左值引用，则直接引用底层元素。
- **value_type 语义**：`remove_cvref_t` 确保 value_type 是纯类型，不包含引用修饰。

### take_view
- **迭代器失效**：`counted_iterator` 包装底层迭代器，底层迭代器失效则失效。
- **sentinel 失效**：`__sentinel` 存储底层 sentinel，若底层 sentinel 失效则 take_view 的 end() 失效。
- **borrowed_range 传播**：`enable_borrowed_range<take_view<T>> = enable_borrowed_range<T>`，仅当底层 range 是 borrowed 时，take_view 才是 borrowed。

### 通用规则
- view 适配器本身不拥有元素（owning_view 除外），迭代器/引用稳定性完全依赖底层 range。
- `__movable_box` 和 `__non_propagating_cache` 在 view 移动/赋值时执行非传播语义，避免悬垂指针。

## 性能模型

### 时间复杂度
| 操作 | filter_view | transform_view | take_view |
|------|-------------|----------------|-----------|
| 构造 | O(1) | O(1) | O(1) |
| begin() | O(n) 首次，O(1) 缓存 | O(1) | O(1) |
| operator++ | O(1) 均摊（每个元素恰好访问一次） | O(1) | O(1) |
| 完整遍历 | O(n) | O(n) | O(min(n, count)) |
| size() | O(n)（需遍历） | O(1)（委托底层） | O(1)（min 计算） |

### 空间开销
- **filter_view**：存储 view + `__movable_box<Pred>` + 可选的 `__non_propagating_cache<iterator>`（forward_range 时）
- **transform_view**：存储 view + `__movable_box<Fn>`
- **take_view**：存储 view + `difference_type` 计数值
- 所有 view 适配器均为栈分配，无堆分配（除非底层 range 本身分配）

### 缓存行为
- **filter_view**：遍历时跳跃式访问底层元素，缓存不友好。谓词调用开销可能主导。
- **transform_view**：顺序访问底层元素，缓存友好。函数调用开销可能主导。
- **take_view**：顺序访问前 N 个元素，缓存友好。counted_iterator 轻量包装。

### 组合开销
```cpp
vec | views::filter(pred) | views::transform(fn) | views::take(5)
// 构造：3 次 view 构造，O(1)
// 遍历：每个元素经过 filter → transform → take 三重检查
// 无中间容器，无堆分配
// 但每次 operator++ 可能触发多层间接调用
```

### 与 eager 算法对比
```cpp
// eager（C++17 风格）：
vector<int> filtered;
copy_if(vec.begin(), vec.end(), back_inserter(filtered), pred);  // O(n) + 堆分配
vector<int> transformed;
transform(filtered.begin(), filtered.end(), back_inserter(transformed), fn);  // O(k) + 堆分配
vector<int> result(transformed.begin(), transformed.begin() + min(5, transformed.size()));  // O(5) + 堆分配

// lazy（C++20 views）：
auto result = vec | views::filter(pred) | views::transform(fn) | views::take(5);
// 构造 O(1)，遍历 O(n) 但无堆分配，短路终止（找到 5 个即停）
```
