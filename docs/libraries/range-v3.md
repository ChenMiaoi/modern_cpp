# range-v3 深度剖析

> range-v3 是 Eric Niebler 编写的 C++ 范围库，直接催生了 C++20 Ranges 标准。标准委员会的多个提案（P0896 等）以它为原型。理解 range-v3 就是理解 `std::ranges` 为什么长成现在这个样子。

## 1. 管道运算符 `|` 的源码实现

管道运算符是整个 Ranges 库的骨架。libc++ 的实现分为三层：CRTP 基类、`__pipeable` 包装器、两个 `operator|` 重载。


### 管道运算符组合流程

```
  vec | filter(pred) | transform(fn)

  等价的函数组合:

  filter(pred) | transform(fn)
       |
       | operator|(closure, closure)
       | = __pipeable(__compose(transform(fn), filter(pred)))
       v
  +-----------------------------------+
  |  composed_closure                  |
  |  g(x) = transform(fn)(filter(pred)(x))  |
  +-----------------------------------+

  最终执行:

  vec | composed_closure
       |
       | operator|(range, closure)
       | = invoke(closure, range)
       = composed_closure(vec)
       = transform(fn)(filter(pred)(vec))
       v
  +-----------------------------------+
  |  结果: filter_view 包装在           |
  |  transform_view 中, 惰性求值        |
  +-----------------------------------+

  三层实现机制:

  第 1 层: __range_adaptor_closure<T>  (CRTP 基类, 标签分发)
       |
       | 继承
       v
  第 2 层: __pipeable<Fn>  (把函数对象包装为可管道的 closure)
       |   同时继承 Fn 和 __range_adaptor_closure
       |   EBO: 无状态 lambda 时 sizeof == 指针大小
       |
       | operator| 重载
       v
  第 3 层: 两个 operator|
       |
       |  range | closure  ->  invoke(closure, range)
       |  closure | closure -> __pipeable(__compose(c2, c1))
       v
  链式组合不停止, 每步产生新的 __pipeable closure

  partial application (部分应用):
  filter(pred) 调用 __fn::operator()(pred)
       |
       v
  __bind_back(*this, pred)  捕获 CPO + pred
       |
       v
  一元函数对象: [this, pred](auto&& rng) { return (*this)(rng, pred); }
       |
       v
  包装进 __pipeable -> 成为可管道化的 closure
```
### 1.1 `__range_adaptor_closure` — CRTP 基类

```cpp
// libc++: __ranges/range_adaptor.h
template <class _Tp>
  requires is_class_v<_Tp> && same_as<_Tp, remove_cv_t<_Tp>>
struct __range_adaptor_closure {};
```

这个类本身是空的。它的作用是做**标签分发**：任何继承自 `__range_adaptor_closure&lt;T&gt;` 的类型都被识别为 range adaptor closure，从而激活管道运算符重载。CRTP 模式确保每种 closure 类型只继承 `__range_adaptor_closure` 一次，避免歧义。

约束中的 concept `_RangeAdaptorClosure` 做了更严格的检查：

```cpp
template <class _Tp>
concept _RangeAdaptorClosure =
    !ranges::range<remove_cvref_t<_Tp>> && requires {
      // 确保 T 只继承 __range_adaptor_closure<T>，不继承其他版本
      { __derived_from_range_adaptor_closure((remove_cvref_t<_Tp>*)nullptr) }
        -> same_as<remove_cvref_t<_Tp>>;
    };
```

`!ranges::range&lt;remove_cvref_t&lt;_Tp&gt;&gt;` 是关键——它防止容器本身被当作 closure 使用，避免 `vec | closure` 与 `closure1 | closure2` 产生歧义。

### 1.2 `__pipeable` — 把任意函数对象变成 closure

```cpp
// libc++: __ranges/range_adaptor.h
template <class _Fn>
struct __pipeable : _Fn, __range_adaptor_closure<__pipeable<_Fn>> {
  constexpr explicit __pipeable(_Fn&& __f) : _Fn(std::move(__f)) {}
};
```

`__pipeable` **同时继承函数对象本身和 CRTP 基类**。这是 EBO（空基类优化）的应用：如果 `_Fn` 是无状态 lambda，`__pipeable` 的大小与指针相同。`__pipeable` 的 CTAD 推导指引允许 `__pipeable(some_lambda)` 自动推导模板参数。

### 1.3 两个 `operator|` 重载

**重载一：range | closure → 调用 closure**

```cpp
template <ranges::range _Range, _RangeAdaptorClosure _Closure>
  requires invocable<_Closure, _Range>
constexpr decltype(auto)
operator|(_Range&& __range, _Closure&& __closure) {
  return std::invoke(std::forward<_Closure>(__closure),
                     std::forward<_Range>(__range));
}
```

`a | filter(pred)` 被解析为 `filter(pred)(a)`。closure 对象本质上是个一元函数，接收 range 返回 view。

**重载二：closure | closure → 组合成新 closure**

```cpp
template <_RangeAdaptorClosure _Closure, _RangeAdaptorClosure _OtherClosure>
constexpr auto operator|(_Closure&& __c1, _OtherClosure&& __c2) {
  return __pipeable(std::__compose(
      std::forward<_OtherClosure>(__c2),
      std::forward<_Closure>(__c1)));
}
```

这里调用了 `std::__compose(f2, f1)`，它返回一个函数对象 `g` 使得 `g(x) = f2(f1(x))`。结果被包装回 `__pipeable`，所以 `f1 | f2 | f3` 每一步都产生新的 closure，链式组合不停止。

### 1.4 `__compose` 的内部实现

```cpp
// libc++: __functional/compose.h
struct __compose_op {
  template <class _Fn1, class _Fn2, class... _Args>
  constexpr auto operator()(_Fn1&& __f1, _Fn2&& __f2, _Args&&... __args) const {
    return std::invoke(std::forward<_Fn1>(__f1),
                       std::invoke(std::forward<_Fn2>(__f2),
                                   std::forward<_Args>(__args)...));
  }
};

template <class _Fn1, class _Fn2>
struct __compose_t : __perfect_forward<__compose_op, _Fn1, _Fn2> { ... };
```

`__perfect_forward` 是 libc++ 的完美转发包装器——它用 tuple 存储捕获的参数，通过 `std::forward` 在调用时按正确的值类别传递。这样 compose 出的闭包本身也可以被正确地移动或拷贝。

### 1.5 部分应用：adaptor 怎么只接受一个参数

```cpp
struct __fn {
  // filter(rng, pred) → 直接构造 filter_view
  template <class _Range, class _Pred>
  constexpr auto operator()(_Range&& __range, _Pred&& __pred) const {
    return filter_view(std::forward<_Range>(__range),
                       std::forward<_Pred>(__pred));
  }

  // filter(pred) → 返回 pipeable closure
  template <class _Pred>
  constexpr auto operator()(_Pred&& __pred) const {
    return __pipeable(std::__bind_back(*this, std::forward<_Pred>(__pred)));
  }
};
```

当只传一个参数时，`__bind_back` 捕获 `*this`（即 `__fn` 对象，通常是全局 CPO）和 `pred`，生成一个一元函数对象，等价于 `[=](auto&& rng) { return (*this)(rng, pred); }`。这个一元对象被包装进 `__pipeable`，就成了可管道化的 closure。

## 2. View 的源码实现


### View 惰性求值数据流

```
  vec | views::filter(pred) | views::transform(fn) | views::take(5)

  构造阶段 (零开销, 仅组装描述):
  +-----------+     +----------------+     +-----------------+     +----------+
  |   vec     |---->| filter_view    |---->| transform_view  |---->| take_view|
  | (底层数据)|     | 存储: pred     |     | 存储: fn        |     | 存储: 5  |
  +-----------+     | 存储: &vec     |     | 存储: &filter   |     | count    |
                    +----------------+     +-----------------+     +----------+

  迭代阶段 (按需驱动, pull-based):

  遍历 take_view
       |
       | count > 0 ?
       v
  调用 transform_view::operator*()
       |
       v
  调用 filter_view::operator++()
       |
       |  内部: ranges::find_if(从当前下一个开始, end, pred)
       |        逐个扫描, 跳过不匹配元素
       v
  pred(*current) == true ?
       |           |
      [否]        [是]
       |           |
       v           v
  继续 ++      transform_view::operator*()
               fn(*current) 得到变换后的值
                    |
                    v
               take_view: count--, 返回结果
                    |
                    v
               下一次迭代...

  关键特性:
  - 每个元素经过 filter -> transform -> take 才输出
  - 无中间存储, 无堆分配
  - filter 的 ++ 是线性扫描 (find_if), 总开销 O(n)
  - transform 的 * 每次调用 fn, 无缓存

  多次遍历陷阱:
  auto v = vec | views::filter(pred);
  auto sz = ranges::distance(v);   // 第一次遍历: O(n)
  for (auto&& x : v) {}            // 第二次遍历: O(n) 重头来!
  // 每次迭代都从头执行整条链, view 无状态缓存
```
### 2.1 `filter_view` — 存储 view + 谓词，惰性跳过不匹配元素

```cpp
// libc++: __ranges/filter_view.h
template <input_range _View, indirect_unary_predicate<iterator_t<_View>> _Pred>
  requires view<_View> && is_object_v<_Pred>
class filter_view : public view_interface<filter_view<_View, _Pred>> {
  _View __base_ = _View();                // 底层 view（拷贝，O(1)）
  __movable_box<_Pred> __pred_;           // 谓词（用 optional 存储，支持不默认构造的类型）

  // 缓存 begin() 结果，使 forward_range 的 begin() 摊还 O(1)
  static constexpr bool _UseCache = forward_range<_View>;
  _If<_UseCache, __non_propagating_cache<iterator_t<_View>>, __empty_cache>
      __cached_begin_;
};
```

**`__movable_box`** 是对 `std::optional` 的优化封装，保证谓词可以被移动、可以不默认构造。**`__non_propagating_cache`** 是一个指针式缓存——当 `filter_view` 被拷贝时缓存不会被传播，避免拷贝后使用过期的迭代器。

**`begin()` 的实现——找第一个满足谓词的元素：**

```cpp
constexpr __iterator begin() {
  if constexpr (_UseCache) {
    if (!__cached_begin_.__has_value()) {
      __cached_begin_.__emplace(ranges::find_if(__base_, std::ref(*__pred_)));
    }
    return {*this, *__cached_begin_};
  } else {
    return {*this, ranges::find_if(__base_, std::ref(*__pred_))};
  }
}
```

对于 `forward_range`，首次调用 `begin()` 会线性扫描找到第一个匹配元素，之后缓存命中。对于 `input_range`（不可多次遍历），每次调用 `begin()` 都重新查找。

**`__iterator` — `operator++` 跳过不匹配元素：**

```cpp
class __iterator {
  iterator_t<_View> __current_;
  filter_view* __parent_;

  constexpr __iterator& operator++() {
    // 从当前位置的下一个开始，找到下一个满足谓词的元素
    __current_ = ranges::find_if(
        std::move(++__current_),            // 先递增
        ranges::end(__parent_->__base_),    // 到末尾
        std::ref(*__parent_->__pred_));     // 匹配谓词
    return *this;
  }

  // operator-- 对 bidirectional_range：反向逐个递减直到匹配
  constexpr __iterator& operator--()
    requires bidirectional_range<_View>
  {
    do { --__current_; }
    while (!std::invoke(*__parent_->__pred_, *__current_));
    return *this;
  }
};
```

每次 `++` 都调用 `ranges::find_if`，这是线性扫描。如果过滤率很高（比如从一百万个元素中只保留 10 个），遍历 `filter_view` 的总开销是 O(n)——每个元素恰好被访问一次，`find_if` 只跳过不匹配的元素。

### 2.2 `transform_view` — 解引用时应用函数

```cpp
// libc++: __ranges/transform_view.h
template <input_range _View, copy_constructible _Fn>
  requires __transform_view_constraints<_View, _Fn>
class transform_view : public view_interface<transform_view<_View, _Fn>> {
  __movable_box<_Fn> __func_;
  _View __base_ = _View();
};
```

与 `filter_view` 不同，`transform_view` 不跳过任何元素。它的迭代器在 `operator*` 时应用函数：

```cpp
template <bool _Const>
class __iterator {
  _Parent* __parent_;
  iterator_t<_Base> __current_;

  // 核心：解引用时调用函数
  constexpr decltype(auto) operator*() const {
    return std::invoke(*__parent_->__func_, *__current_);
  }

  // ++/-- 只是简单委托给底层迭代器，不做任何过滤
  constexpr __iterator& operator++() {
    ++__current_;
    return *this;
  }
};
```

`value_type` 是 `remove_cvref_t&lt;invoke_result_t&lt;_Fn&amp;, range_reference_t&lt;_Base&gt;&gt;&gt;`——即函数返回值去掉引用和 cv 限定符。如果函数返回引用（如 `&amp;Employee::name`），`transform_view` 的迭代器解引用结果就是引用；如果返回值（如 lambda `{ return n * n; }`），结果就是值。

注意 `operator*()` 返回的是 `decltype(auto)`——这意味着如果函数返回 `std::string&`，view 的每个元素就是引用，不拷贝。但这也意味着 view 存储的是底层数据的**引用语义**，底层数据被销毁后 view 就悬空了。

## 3. Sentinel 机制

### 3.1 `unreachable_sentinel_t` — 永远不等于任何迭代器

```cpp
// libc++: __iterator/unreachable_sentinel.h
struct unreachable_sentinel_t {
  template <weakly_incrementable _Iter>
  friend constexpr bool operator==(unreachable_sentinel_t, const _Iter&) noexcept {
    return false;  // 永远返回 false——迭代器永远没到"终点"
  }
};
inline constexpr unreachable_sentinel_t unreachable_sentinel{};
```

实现极其简单：对任何迭代器类型，`operator==` 恒返回 `false`。这使得 `unreachable_sentinel` 只能与外部约束（如 `take` 的计数、`take_while` 的谓词）配合使用，否则迭代永不停止。

典型用法是描述无限序列的起点：

```cpp
auto iota_view = views::iota(0, unreachable_sentinel); // 无限递增
auto first_10 = iota_view | views::take(10);            // take 负责终止
```

### 3.2 `take_while_view` — 谓词本身就是 sentinel

`take_while_view` 不用迭代器比较来终止，而是用谓词。它的 `end()` 返回一个特殊的 `__sentinel` 类型：

```cpp
// libc++: __ranges/take_while_view.h
template <bool _Const>
class take_while_view<_View, _Pred>::__sentinel {
  sentinel_t<_Base> __end_;   // 底层 view 的真正末尾
  const _Pred* __pred_;       // 指向谓词的指针

  friend constexpr bool operator==(const iterator_t<_Base>& __x,
                                   const __sentinel& __y) {
    // 终止条件：到底层末尾了，或者谓词不满足了
    return __x == __y.__end_ || !std::invoke(*__y.__pred_, *__x);
  }
};
```

比较逻辑是**短路 OR**：当底层迭代器到达 `end`，或者谓词对当前元素返回 `false`，迭代停止。注意 sentinel 存储的是谓词的 **const 指针**——谓词在 view 本体中（通过 `__movable_box`），sentinel 只是引用它。这意味着 take_while_view 的 begin 和 end 类型不同（一个是迭代器，一个是 sentinel），它不是一个 `common_range`。

**这带来了实际约束**：许多标准算法（如 `std::ranges::sort`）要求 `common_range`。`take_while_view` 的结果不能直接排序，必须先 `ranges::to&lt;vector&gt;()` 物化。

## 4. Projections — 算法的第三参数

Projections 是 range-v3 引入、C++20 全面采纳的设计。算法签名形如：

```cpp
// ranges::sort
template <random_access_range _Range,
          class _Comp = ranges::less,
          class _Proj = identity>
constexpr borrowed_iterator_t<_Range>
operator()(_Range&& __r, _Comp __comp = {}, _Proj __proj = {}) const;
```

调用时：`ranges::sort(employees, std::less{}, &Employee::age)` —— 按年龄排序。

### 4.1 实现机制：`__make_projected`

```cpp
// libc++: __algorithm/make_projected.h
template <class _Pred, class _Proj>
struct _ProjectedPred {
  _Pred& __pred;
  _Proj& __proj;

  // 一元调用：pred(proj(elem))
  template <class _Tp>
  auto operator()(_Tp&& __v) const {
    return std::__invoke(__pred, std::__invoke(__proj, std::forward<_Tp>(__v)));
  }

  // 二元调用：pred(proj(lhs), proj(rhs))
  template <class _T1, class _T2>
  auto operator()(_T1&& __lhs, _T2&& __rhs) const {
    return std::__invoke(__pred,
        std::__invoke(__proj, std::forward<_T1>(__lhs)),
        std::__invoke(__proj, std::forward<_T2>(__rhs)));
  }
};
```

Projection 通过组合实现——把 `_ProjectedPred` 包装原始比较器，然后把投影器"缠绕"进去。对于 `sort`，实际调用链是 `__make_projected(comp, proj)` 产生一个二元谓词，内部每次比较时先对两个元素分别应用 `proj`，再把结果传给 `comp`。

**零开销优化**：当 `Proj` 是 `identity`（默认值）且 `Pred` 不是成员指针时，`__make_projected` **直接返回原始谓词的引用**，不创建任何包装器：

```cpp
// 当 proj == identity 且 pred 不是成员指针时，直接透传
template <class _Pred, class _Proj>
  requires (!is_member_pointer_v<decay_t<_Pred>>) && __is_identity<decay_t<_Proj>>
_Pred& __make_projected(_Pred& __pred, _Proj&) {
  return __pred;  // 零开销
}
```

### 4.2 Projection vs Lambda 的对比

```cpp
// 用 projection（编译器看到的是成员指针，可以内联优化）
ranges::sort(employees, std::less{}, &Employee::age);

// 用 lambda（语义相同，但编译器看到的是闭包类型，可能多一层间接调用）
ranges::sort(employees, [](const auto& a, const auto& b) {
    return a.age < b.age;
});
```

Projection 更简洁，而且对 `ranges::find`、`ranges::count` 等一元算法尤其方便：`ranges::find(employees, 30, &Employee::age)` 直接按年龄查找。

## 5. `ranges::to&lt;Container&gt;()` — 物化视图为容器

`ranges::to` 是 C++23 引入的（range-v3 早期就有），解决了一个核心痛点：如何把惰性 view 链的最终结果变成容器。

### 5.1 源码中的四层 fallback 策略

```cpp
// libc++: __ranges/to.h (简化)
template <class _Container, input_range _Range, class... _Args>
  requires (!view<_Container>)
constexpr _Container to(_Range&& __range, _Args&&... __args) {
  // 第 1 层：直接构造（如 vector(rng)）
  if constexpr (constructible_from<_Container, _Range, _Args...>)
    return _Container(std::forward<_Range>(__range), ...);

  // 第 2 层：from_range_t 标签构造（C++23 新增的构造函数）
  else if constexpr (constructible_from<_Container, from_range_t, _Range, _Args...>)
    return _Container(from_range, std::forward<_Range>(__range), ...);

  // 第 3 层：迭代器对构造（如 vector(begin, end)）
  else if constexpr (__constructible_from_iter_pair<_Container, _Range, _Args...>)
    return _Container(ranges::begin(__range), ranges::end(__range), ...);

  // 第 4 层：默认构造 + 逐个插入（有 reserve 优化）
  else if constexpr (constructible_from<_Container, _Args...> &&
                     __container_appendable<_Container, range_reference_t<_Range>>) {
    _Container __result(std::forward<_Args>(__args)...);
    if constexpr (sized_range<_Range> && __reservable_container<_Container>)
      __result.reserve(static_cast<range_size_t<_Container>>(ranges::size(__range)));
    for (auto&& __ref : __range)
      __result.emplace_back(std::forward<decltype(__ref)>(__ref));
    return __result;
  }
}
```

**嵌套容器的递归处理**：当 `_Container` 是 `vector&lt;vector&lt;int&gt;&gt;` 而输入是 `range of range of int` 时，第一层 fallback 失败（`vector&lt;vector&lt;int&gt;&gt;` 不能从 `range of range` 直接构造），进入递归分支：

```cpp
else if constexpr (input_range<range_reference_t<_Range>>) {
  return ranges::to<_Container>(
      ref_view(__range) | views::transform([](auto&& __elem) {
        return ranges::to<range_value_t<_Container>>(std::move(__elem));
      }), __args...);
}
```

对每一行递归调用 `ranges::to<vector&lt;int&gt;&gt;`。

### 5.2 模板模板参数版本

```cpp
// ranges::to<vector>()  — 不指定元素类型，自动推导
template <template <class...> class _Container, input_range _Range, class... _Args>
constexpr auto to(_Range&& __range, _Args&&... __args) {
  using _DeducedExpr = typename _Deducer<_Container, _Range, _Args...>::type;
  return ranges::to<_DeducedExpr>(std::forward<_Range>(__range), ...);
}
```

`_Deducer` 内部构造一个 `__minimal_input_iterator`（只有类型信息，没有实现），用它来推导 `vector&lt;int&gt;` 之类的完整类型。推导完成后委托给具体类型版本。

### 5.3 管道用法

```cpp
// 管道形式：ranges::to<vector>() 返回 __pipeable closure
template <class _Container, class... _Args>
constexpr auto to(_Args&&... __args) {
  auto __to_func = []<input_range _Range, class... _Tail>(
      _Range&& __range, _Tail&&... __tail) static {
    return ranges::to<_Container>(std::forward<_Range>(__range), ...);
  };
  return __pipeable(std::__bind_back(__to_func, std::forward<_Args>(__args)...));
}
```

`views::iota(0, 10) | ranges::to&lt;vector&gt;()` 的执行路径：`to&lt;vector&gt;()` 返回一个 `__pipeable` closure，`|` 运算符调用 `closure(iota_view)`，最终执行 `vector(iota_view.begin(), iota_view.end())`。

## 6. 性能陷阱

### 6.1 多次遍历导致 O(n²)

View 是惰性的、**无状态缓存**的（除了 `filter_view` 的 begin 缓存）。每次遍历都从头执行整条流水线：

```cpp
// 危险：两次遍历 = O(n²)
auto filtered = vec | views::filter(pred);  // 无代价，只存储指针和谓词
auto sz = ranges::distance(filtered);        // 第一次遍历：O(n)
for (auto&& x : filtered) {}                 // 第二次遍历：O(n) — 再从头来

// 安全：物化后多次访问 = O(n)
auto collected = filtered | ranges::to<std::vector>();  // 一次遍历 O(n)，分配 O(n)
auto sz = collected.size();                              // O(1)
for (auto&& x : collected) {}                            // O(n)
```

**根本原因**：view 不存储元素，只存储"如何生成元素"的描述。`filter_view` 的 `begin()` 每次调用 `find_if` 从头扫描，`transform_view` 的 `operator*()` 每次解引用都调用函数。这是有意的设计——零分配、零缓存、与手写循环等价。代价是不能假设遍历是免费的。

### 6.2 必须物化的场景

| 场景 | 原因 |
|------|------|
| 需要随机访问（`operator[]`） | 大部分 view 只提供前向/双向迭代 |
| 需要 `size()` | 只有少数 view（`transform_view` 等）提供 |
| 需要多次遍历 | 每次遍历都重算整条链 |
| 需要传给要求 `common_range` 的算法 | `take_while_view` 等产生非 common range |
| view 链的底层数据会变 | view 持有引用语义，底层修改会影响 view |

### 6.3 编译时间

view 链的每一层都产生新的模板实例化类型。10 层嵌套的 view 链可以产生数秒的增量编译时间。range-v3 比 C++20 ranges 更严重——头文件按组件拆分但类型层次更深。C++20 的 `&lt;ranges&gt;` 是单一大头文件，编译器前端缓存效果更好。

## 7. Concepts-First 设计

range-v3 始于 2014 年，当时 C++ Concepts 尚未入标准。但它内部实现了一套完整的 Concepts 子系统，所有接口约束基于 concept 而非 SFINAE：

```cpp
namespace ranges::concepts {
    template<typename T>
    concept Semiregular = Copyable<T> && DefaultConstructible<T>;
    template<typename I>
    concept InputIterator = Readable<I> && Incrementable<I> && ...;
}
```

这个决策证明了**基于 concept 的泛型编程比 SFINAE 更可组合**，直接推动了 C++20 Concepts 标准化。代价是早期编译错误信息极难读。range-v3 用 `CONCEPT_REQUIRES_` 宏兼容 C++14/17；迁移时注意约束语义等价性。

## 8. Views vs Actions vs Projections

### Views：惰性求值

View 拥有摊还 O(1) 的拷贝成本，不拥有数据，只描述访问方式。求值发生在迭代时：

```cpp
namespace rv = ranges::views;
auto result = numbers
    | rv::filter([](int n) { return n % 2 == 0; })
    | rv::transform([](int n) { return n * n; })
    | rv::take(5);
```

### Actions：急切原地变换

Actions 操作容器本身，直接修改元素。**C++20 标准没有采纳 Actions**——这是最大的迁移痛点。

```cpp
namespace ra = ranges::actions;
std::vector<int> v{5, 3, 1, 4, 2, 3};
v |= ra::sort;
v |= ra::unique;
v |= ra::remove_if([](int n) { return n < 3; });
// v = {3, 4, 5}
```

## 9. range-v3 vs C++20 Ranges 对照

| 类别 | range-v3 | C++20/C++23 | 备注 |
|------|----------|-------------|------|
| `filter / transform / take` | ✅ | ✅ | 完全采纳 |
| `iota / reverse / join` | ✅ | ✅ | |
| `zip / chunk / slide` | ✅ | ✅ C++23 | |
| `concat` | ✅ | ❌ | P2520 进行中 |
| `cycle / cache_latest` | ✅ | ❌ | |
| **Actions** | ✅ `ra::sort` 等 | **❌ 未采纳** | 最大差异 |
| Projections | ✅ 全面 | ✅ 部分 | 算法有，视图无 |
| `to&lt;Container&gt;()` | ✅ | ✅ C++23 | |
| 头文件 | 按组件拆分 | `&lt;ranges&gt;` 大包 | 影响编译速度 |

## 10. 迁移指南

```cpp
// 命名空间：range-v3 → C++20
// ranges::views → std::views
// ranges::actions → 无直接替代

// range-v3 actions 替代方案
// 旧：v |= ra::sort | ra::unique;
// 新：
std::ranges::sort(v);
auto [f, l] = std::ranges::unique(v);
v.erase(f, l);
```

| 检查项 | 操作 |
|--------|------|
| 命名空间 | `ranges::views::` → `std::views::` |
| 头文件 | `&lt;range/v3/view/*.hpp&gt;` → `&lt;ranges&gt;` |
| Actions | 改写为 `std::ranges` 算法 + `erase` |
| Projection 位置 | range-v3 常为第三参数，`std::ranges` 常在最后 |
| `split` 接口 | C++20 返回的子范围类型与 range-v3 不同 |

## 11. 实战：复杂流水线

```cpp
// 日志分析：提取 ERROR → 按消息分组计数 → 取 top-5
auto top_errors = lines
    | rv::transform(&parse_log_line)
    | rv::filter([](const auto& e) { return e.level == "ERROR"; })
    | rv::transform(&LogEntry::message)
    | ranges::to<std::vector>()   // materialize
    | ra::sort                     // eager sort
    | rv::group_by(std::equal_to<>())
    | rv::transform([](auto g) {
        return std::pair{*g.begin(), ranges::distance(g)};
    })
    | ra::sort(std::greater<>{}, &decltype(std::declval<pair_t>())::second)
    | rv::take(5);
```

```cpp
// 矩阵行运算：enumerate + zip + accumulate
auto indexed = rv::enumerate(matrix);
auto row_means = indexed
    | rv::transform([](auto&& [i, row]) {
        return std::pair{i, ranges::accumulate(row, 0.0) / row.size()};
    })
    | rv::filter([](auto&& p) { return p.second > threshold; });
```

## 12. Composability 哲学

range-v3 的组合性建立在严格的 concept 层次上。每个 view adaptor 只声明它需要的最弱约束：`view::filter` 要求 `viewable_range` + `indirect_unary_predicate`，`view::transform` 只要求 `input_range` + 可调用对象。这意味着几乎任意两个 view 可以自由组合，无需中间类型匹配。

当你写 `a | b | c`，没有数据流动——只是构建了一棵操作树。数据在最终迭代时按需驱动，每个元素经过 `a → b → c` 才输出。这是函数式编程中的 pull-based streaming model，对大数据集和 I/O 流特别有效。

## 13. 何时仍用 range-v3

- 需要 Actions（原地容器操作）
- 需要 `concat`、`cycle` 等未入标准的视图
- 项目停留在 C++14/17
- 需要 `to&lt;Container&gt;()`（C++23 之前）

新项目优先 `std::ranges`，仅在标准库不足时引入 range-v3 作为补充。

---

**参考**：[range-v3 GitHub](https://github.com/ericniebler/range-v3) | [P0896R4](https://wg21.link/P0896R4) | [cppreference Ranges](https://en.cppreference.com/w/cpp/ranges) | [libc++ `__ranges/range_adaptor.h`](https://github.com/llvm/llvm-project/blob/main/libcxx/include/__ranges/range_adaptor.h)
