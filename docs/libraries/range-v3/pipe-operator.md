# range-v3 管道运算符的源码实现

> 源码路径：`references/impl/llvm-project/libcxx/include/__ranges/range_adaptor.h`

## 三层实现机制

```
  vec | filter(pred) | transform(fn)

  第 1 层: __range_adaptor_closure<T>  (CRTP 基类, 标签分发)
       |
  第 2 层: __pipeable<Fn>  (把函数对象包装为可管道的 closure)
       |
  第 3 层: 两个 operator|
       |
       |  range | closure  ->  invoke(closure, range)
       |  closure | closure -> __pipeable(__compose(c2, c1))
```

### 1. `__range_adaptor_closure` — CRTP 基类

```cpp
template <class _Tp>
  requires is_class_v<_Tp> && same_as<_Tp, remove_cv_t<_Tp>>
struct __range_adaptor_closure {};
```

空类，仅做标签分发。Concept 约束确保 T 只继承 `__range_adaptor_closure<T>` 一次，且 T 不是 range（避免歧义）。

### 2. `__pipeable` — EBO 函数对象包装

```cpp
template <class _Fn>
struct __pipeable : _Fn, __range_adaptor_closure<__pipeable<_Fn>> {
  constexpr explicit __pipeable(_Fn&& __f) : _Fn(std::move(__f)) {}
};
```

同时继承函数对象和 CRTP 基类。EBO 确保无状态 lambda 时 `sizeof` 与指针相同。

### 3. 两个 `operator|` 重载

**重载一：range | closure**

```cpp
template <ranges::range _Range, _RangeAdaptorClosure _Closure>
constexpr decltype(auto)
operator|(_Range&& __range, _Closure&& __closure) {
  return std::invoke(std::forward<_Closure>(__closure),
                     std::forward<_Range>(__range));
}
```

**重载二：closure | closure**

```cpp
template <_RangeAdaptorClosure _Closure, _RangeAdaptorClosure _OtherClosure>
constexpr auto operator|(_Closure&& __c1, _OtherClosure&& __c2) {
  return __pipeable(std::__compose(
      std::forward<_OtherClosure>(__c2),
      std::forward<_Closure>(__c1)));
}
```

`__compose(g, f)` 返回 `g(f(x))`。结果包装回 `__pipeable`，链式组合不停止。

## 部分应用：adaptor 怎么只接受一个参数

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

`__bind_back` 捕获 CPO + pred，生成等价于 `[=](auto&& rng) { return (*this)(rng, pred); }` 的一元函数，包装进 `__pipeable`。
