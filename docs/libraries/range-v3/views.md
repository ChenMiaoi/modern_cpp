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
