---
title: "Ranges 实现内部机制：view_interface、管道操作、迭代器降级与适配器实现"
topic: cpp20
feature: ranges-internals
standard: C++20
status_checked_at: 2026-06-02
standard_refs:
  - draft: N4861
    clause: "[ranges]"
  - draft: N4861
    clause: "[range.adaptor]"
  - draft: N4861
    clause: "[range.view]"
  - draft: N4861
    clause: "[range.filter.view]"
  - draft: N4861
    clause: "[range.transform.view]"
proposals:
  - P0896R4
  - P1035R7
  - P1391R4
  - P1394R4
  - P2210R2
  - P2387R3
exercises: []
solutions: []
---

# Ranges 实现内部机制

## 概述

C++20 Ranges 库的用户接口简洁——`views::filter`、`views::transform`、管道 `|` 操作。但其实现涉及大量 CRTP 基类、trait 机制和惰性迭代器设计。本文深入标准库实现者的视角，揭示 `view_interface` 如何通过 CRTP 自动生成默认成员、`range_adaptor_closure` 如何实现管道组合、`filter_view` 如何缓存 `begin()`、`transform_view` 如何维护父指针、sentinel 与 iterator 分离的设计价值、view 迭代器的类别降级规则、`borrowed_range` 如何防止悬空迭代器，以及 range-v3 到 C++20 的迁移要点。

## view_interface CRTP 基类

`std::ranges::view_interface<D>` 是一个 CRTP 基类，为派生类自动生成 `empty()`、`operator bool()`、`data()`、`size()`、`front()`、`back()`、`operator[]` 等成员的默认实现。

### CRTP 机制

```cpp
template <std::ranges::view Derived>
class view_interface {
protected:
    // CRTP：向下转换为派生类型
    constexpr Derived& derived() noexcept {
        return static_cast<Derived&>(*this);
    }
    constexpr const Derived& derived() const noexcept {
        return static_cast<const Derived&>(*this);
    }
public:
    constexpr bool empty() requires std::ranges::forward_range<Derived>
    {
        return std::ranges::begin(derived()) == std::ranges::end(derived());
    }

    constexpr explicit operator bool()
        requires requires { !std::ranges::empty(derived()); }
    {
        return !std::ranges::empty(derived());
    }

    constexpr auto front() requires std::ranges::forward_range<Derived>
        && requires { *std::ranges::begin(derived()); }
    {
        return *std::ranges::begin(derived());
    }

    constexpr auto back() requires std::ranges::bidirectional_range<Derived>
        && std::ranges::common_range<Derived>
        && requires { *std::ranges::begin(derived()); }
    {
        return *std::prev(std::ranges::end(derived()));
    }

    constexpr auto operator[](std::ranges::range_difference_t<Derived> n)
        requires std::ranges::random_access_range<Derived>
    {
        return std::ranges::begin(derived())[n];
    }

    constexpr auto size() requires std::ranges::sized_range<Derived>
    {
        return std::ranges::end(derived()) - std::ranges::begin(derived());
    }

    constexpr auto data() requires std::ranges::contiguous_range<Derived>
    {
        return std::to_address(std::ranges::begin(derived()));
    }
};
```

**生成条件**：每个默认成员都用 `requires` 检查底层迭代器能力。如果派生类不满足 `random_access_range`，`operator[]` 不会被实例化（SFINAE 友好）。

### 自定义派生类示例

```cpp
template <typename It, typename Sent>
class my_subrange : public std::ranges::view_interface<my_subrange<It, Sent>> {
    It begin_;
    Sent end_;
public:
    constexpr my_subrange() = default;
    constexpr my_subrange(It b, Sent e) : begin_(b), end_(e) {}

    constexpr It begin() const { return begin_; }
    constexpr Sent end() const { return end_; }

    // 从 view_interface 继承：
    // empty(), operator bool(), front(), back(), operator[], size(), data()
    // 全部基于 begin()/end() 自动生成
};
```

### CRTP 的实现开销

CRTP 是零开销抽象——所有成员函数通过 `static_cast` 调用派生类的 `begin()`/`end()`，编译器内联后无运行时开销。虚函数表（vtable）不会被引入。

## viewable_range 定义

```cpp
// C++20 定义
template <typename T>
concept viewable_range =
    range<T> && (
        // 情况1：左值 range 总是 viewable
        std::is_lvalue_reference_v<T> ||
        // 情况2：右值 view（move 语义）
        (movable<remove_cvref_t<T>> && !__is_derived_from_view_base<remove_cvref_t<T>>)
        // 情况3：右值 view 子类（特殊处理）
        || view<remove_cvref_t<T>>
    );
```

**设计意图**：
- 左值 range（`vector&`）可以直接构建 view（view 存引用）
- 右值 non-view range 可以安全地被 view 接管（通常 moved）
- 右值 view 可以直接 move 进管道
- 右值 non-movable range 不可构建 view（无法安全存储）

## borrowed_range 与悬空迭代器防护

```cpp
// borrowed_range：range 的迭代器在 range 本身销毁后仍有效
template <typename T>
concept borrowed_range = range<T> && (
    std::is_lvalue_reference_v<T> ||
    enable_borrowed_range<remove_cvref_t<T>>
);
```

标准库中的 borrowed_range 类型：
- `std::span<T>`（不拥有数据）
- `std::string_view`（不拥有数据）
- 所有左值 range 引用

非 borrowed_range 类型：
- `std::vector<T>` 右值（销毁后迭代器悬空）
- `std::string` 右值

```cpp
// dangling 迭代器保护
auto result = std::ranges::find(std::vector{1, 2, 3}, 2);
// 返回类型是 std::ranges::dangling 而非迭代器
// 任何对 dangling 的操作都是编译错误（不是 UB）

// 正确用法：保持 range 存活
std::vector v = {1, 2, 3};
auto it = std::ranges::find(v, 2);  // v 是左值 → 真迭代器
```

## enable_view / enable_borrowed_range opt-in traits

### enable_view

```cpp
// 派生自 view_base 自动 opt-in
struct my_view : std::ranges::view_interface<my_view> {
    // ...
};

// 或特化 enable_view
template <>
inline constexpr bool std::ranges::enable_view<my_legacy_view> = true;
```

判断一个类型是否为 view 的逻辑：

```cpp
template <typename T>
concept view =
    range<T> &&
    movable<T> &&
    enable_view<T>;  // 关键：需要显式 opt-in
```

**为什么需要 opt-in**：`vector` 和 `string` 满足 `range` 和 `movable`，但它们不是 view——它们拥有数据且拷贝代价高。opt-in 防止它们被意外当作 view 使用。

### enable_borrowed_range

```cpp
// 为自定义类型标记为 borrowed
template <>
inline constexpr bool std::ranges::enable_borrowed_range<my_span> = true;
```

**安全警告**：只有在迭代器不持有 range 本体引用时才应 opt-in borrowed_range。错误标记将导致悬空迭代器而无编译保护。

## range_adaptor_closure 与管道操作

### 管道 `|` 的实现机制

管道操作分两步：
1. `range | adaptor` → 适配器应用于 range
2. `adaptor1 | adaptor2` → 两个适配器组合为新的闭包

```cpp
// range_adaptor_closure CRTP 基类
template <typename D>
struct range_adaptor_closure {};

// range | closure：直接调用
template <typename R, typename C>
    requires range<R> && derived_from<C, range_adaptor_closure<C>>
constexpr auto operator|(R&& r, C&& c) {
    return std::forward<C>(c)(std::forward<R>(r));
}

// closure | closure：组合两个闭包
template <typename C1, typename C2>
    requires derived_from<C1, range_adaptor_closure<C1>>
          && derived_from<C2, range_adaptor_closure<C2>>
constexpr auto operator|(C1&& c1, C2&& c2) {
    // 返回新的闭包，内部保存两个闭包
    return pipeline_closure(std::forward<C1>(c1), std::forward<C2>(c2));
}
```

### 组合闭包的实现

```cpp
template <typename C1, typename C2>
struct pipeline_closure : range_adaptor_closure<pipeline_closure<C1, C2>> {
    C1 c1_;
    C2 c2_;

    template <typename R>
    constexpr auto operator()(R&& r) {
        // 先应用 c1，再应用 c2
        return std::forward<C2>(c2_)(std::forward<C1>(c1_)(std::forward<R>(r)));
    }
};
```

### views::filter 的适配器对象实现

```cpp
namespace std::ranges::views {

struct filter_fn {
    // 直接调用形式：filter(range, pred)
    template <viewable_range R, typename Pred>
    constexpr auto operator()(R&& r, Pred pred) const {
        return filter_view(std::forward<R>(r), std::move(pred));
    }

    // 柯里化形式：filter(pred) → 返回闭包
    template <typename Pred>
    constexpr auto operator()(Pred pred) const {
        // 返回 range_adaptor_closure 子类
        return filter_closure<Pred>{std::move(pred)};
    }
};

inline constexpr filter_fn filter{};  // 函数对象

template <typename Pred>
struct filter_closure : range_adaptor_closure<filter_closure<Pred>> {
    Pred pred_;

    template <typename R>
    constexpr auto operator()(R&& r) const {
        return filter_view(std::forward<R>(r), pred_);
    }
};

} // namespace std::ranges::views
```

**两种调用形式**：

```cpp
// 形式一：直接调用（两个参数）
auto v1 = std::views::filter(data, pred);

// 形式二：管道式（一个参数返回闭包）
auto v2 = data | std::views::filter(pred);
// 等价于 filter_closure{pred}(data) → filter_view(data, pred)

// 形式三：组合
auto v3 = data
    | std::views::filter(pred1)
    | std::views::transform(func);
// pipeline_closure(filter_closure{pred1}, transform_closure{func})(data)
```

## filter_view begin 缓存机制

`filter_view` 的 `begin()` 需要跳过不满足谓词的前缀元素。为了避免每次调用 `begin()` 都重新遍历前缀，标准规定了缓存机制：

```cpp
template <view V, typename Pred>
class filter_view : public view_interface<filter_view<V, Pred>> {
    V base_;
    Pred pred_;
    // 缓存：指向第一个满足谓词的元素
    optional<iterator_t<V>> cached_begin_;

public:
    constexpr iterator begin() {
        if (!cached_begin_) {
            // 首次调用：找到第一个满足谓词的元素
            auto it = ranges::begin(base_);
            auto last = ranges::end(base_);
            while (it != last && !invoke(pred_, *it))
                ++it;
            cached_begin_ = std::move(it);
        }
        return iterator{this, *cached_begin_};
    }
    // ...
};
```

**缓存失效条件**：
- 当 `filter_view` 被 move 时缓存失效
- 标准并未要求在底层 range 变化时自动刷新缓存（惰性语义）
- **实际陷阱**：修改底层 range 后再次遍历 filter_view，缓存的 `begin` 可能指向无效位置

**C++23 改进**：`filter_view` 的迭代器在底层迭代器失效时行为更明确。

## transform_view：父指针 + 当前迭代器

```cpp
template <view V, typename F>
class transform_view : public view_interface<transform_view<V, F>> {
    V base_;
    F fun_;

public:
    class iterator {
        transform_view* parent_;       // 指向父 view（非拥有）
        iterator_t<V> current_;        // 底层迭代器

    public:
        using value_type = remove_cvref_t<invoke_result_t<F&, range_reference_t<V>>>;
        using reference = invoke_result_t<F&, range_reference_t<V>>;
        using difference_type = range_difference_t<V>;
        // 迭代器类别降级（见下文）

        constexpr iterator() = default;
        constexpr iterator(transform_view* parent, iterator_t<V> current)
            : parent_(parent), current_(std::move(current)) {}

        constexpr reference operator*() const {
            return invoke(parent_->fun_, *current_);
        }

        constexpr iterator& operator++() {
            ++current_;
            return *this;
        }
        // ... 其他操作委托给 current_
    };

    constexpr auto begin() { return iterator{this, ranges::begin(base_)}; }
    constexpr auto end() {
        if constexpr (common_range<V>)
            return iterator{this, ranges::end(base_)};
        else
            return sentinel{ranges::end(base_)};
    }
};
```

**父指针的设计理由**：`operator*()` 需要访问 `parent_->fun_` 来应用变换函数。迭代器通过非拥有指针引用父 view——这意味着父 view 必须比迭代器存活更久（标准库 view 的普遍要求）。

**与 range-v3 的差异**：range-v3 用 `semiregular` 包装器存储函数对象，迭代器可以独立于父 view 存在。C++20 标准库选择更简单的父指针方案。

## Sentinel vs Iterator：为什么不同类型有价值

### 设计动机

传统 STL 要求 `begin()` 和 `end()` 返回相同类型。这强制要求某些 sentinel 是完整的迭代器，浪费空间：

```cpp
// 传统 STL 的问题
for (auto it = begin; it != end; ++it) { ... }
// end 迭代器携带冗余状态：底层数据指针 + 长度
// 但实际上只需知道"到末尾了"
```

### Ranges 的哨兵分离

```cpp
template <view V, typename Pred>
class filter_view {
    // ...
    class sentinel {
        sentinel_t<V> end_;   // 只持有底层哨兵
    public:
        constexpr sentinel(sentinel_t<V> end) : end_(end) {}
    };

    constexpr auto end() { return sentinel{ranges::end(base_)}; }
    // 返回类型与 begin() 不同
};
```

**哨兵比迭代器更小、更简单**。在 `filter_view` 中，`end()` 只需要底层序列的结束标记，无需谓词、父指针或当前元素迭代器。

### 实际收益

```cpp
// 旧代码需要 common_view 适配
auto v = std::views::iota(1, 10);  // iterator != sentinel
// std::sort(v.begin(), v.end());  // 编译错误：类型不匹配

// C++23 ranges 算法原生支持不同类型的哨兵
std::ranges::sort(v | std::views::common);  // 需要 common
// 或 C++23 部分算法直接支持 sentinel
```

## Lazy View 迭代器类别降级规则

view 适配器的迭代器类别通常低于底层 range 的类别：

```
迭代器类别降级规则
─────────────────────────────────────────────────
适配器             底层类别        结果类别
─────────────────────────────────────────────────
transform_view    contiguous     random_access
                  random_access  random_access
                  bidirectional  bidirectional
                  forward        forward
                  input          input
─────────────────────────────────────────────────
filter_view       任何           forward（最多）
                  input          forward
                  forward        forward
                  random_access  forward（降级！）
─────────────────────────────────────────────────
join_view         forward(外部)  input
                  bidirectional  bidirectional*
─────────────────────────────────────────────────
zip_view          min(各range)   min(各range)
─────────────────────────────────────────────────
```

**filter_view 为什么总是 forward**：filter 需要在遍历中跳过不满足谓词的元素。`operator--` 对于双向 filter 是可能的（实现复杂），但 C++20 标准选择简单实现：filter_view 的迭代器最高为 `forward_iterator`。即使底层是 `random_access`，`operator[]` 和 `operator-` 也无法高效实现。

**transform_view 为什么降级 contiguous**：`transform_view` 的 `operator*()` 返回函数应用结果而非原始元素引用。`contiguous_iterator` 要求 `operator*` 返回元素的引用，但变换结果通常不是底层存储的引用，因此 `contiguous` 降级为 `random_access`。

```cpp
// 验证迭代器类别
std::vector<int> v = {1, 2, 3};
static_assert(std::contiguous_iterator<decltype(v.begin())>);

auto tv = v | std::views::transform([](int x) { return x * 2; });
static_assert(std::random_access_iterator<decltype(tv.begin())>);
static_assert(!std::contiguous_iterator<decltype(tv.begin())>);

auto fv = v | std::views::filter([](int x) { return x > 1; });
static_assert(std::forward_iterator<decltype(fv.begin())>);
static_assert(!std::bidirectional_iterator<decltype(fv.begin())>);
```

## subrange_kind::sized

`std::ranges::subrange` 有三种形式，由 `subrange_kind` 枚举控制：

```cpp
enum class subrange_kind { sized, unsized };

template <std::input_or_output_iterator I,
          std::sentinel_for<I> S = I,
          subrange_kind K = sized_and_borrowed<I, S>() ? subrange_kind::sized
                                                       : subrange_kind::unsized>
class subrange;
```

- **`sized`**：subrange 可以 O(1) 计算 `size()`（需要 `S - I` 合法）
- **`unsized`**：只能通过遍历计算 `size()`（O(n)）

```cpp
std::vector v = {1, 2, 3, 4, 5};
// sized subrange（迭代器是 random_access）
std::ranges::subrange sub1(v.begin() + 1, v.begin() + 4);
static_assert(std::ranges::sized_range<decltype(sub1)>);
sub1.size();  // 3 — O(1)

// unsized subrange（迭代器是 forward）
std::forward_list fl = {1, 2, 3, 4, 5};
auto it = fl.begin(); std::advance(it, 1);
std::ranges::subrange<
    decltype(it), decltype(fl.end()),
    std::ranges::subrange_kind::unsized
> sub2(it, fl.end());
// sub2.size() 不可用——需要遍历
```

## C++23 zip_view 与 chunk_view 实现要点

### zip_view

```cpp
template <typename... Views>
class zip_view : public view_interface<zip_view<Views...>> {
    std::tuple<Views...> bases_;

public:
    class iterator {
        std::tuple<iterator_t<Views>...> current_;
        // operator* 返回 tuple<reference...>
        constexpr auto operator*() const {
            return std::apply([](auto&... its) {
                return std::tuple<decltype(*its)...>(*its...);
            }, current_);
        }
    };

    class sentinel {
        std::tuple<sentinel_t<Views>...> ends_;
    };
};
```

**实现难点**：
- 迭代器类别取所有子 range 的**最小公共类别**
- 当任一子 range 到达末尾时 `zip_view` 结束
- `operator*` 返回 `tuple` 的引用类型，需处理右值/左值混合

### chunk_view

```cpp
// C++23 chunk_view 按大小分块
auto chunks = data | std::views::chunk(3);
// 内部维护一个 "当前块" 的 subrange
// 每次 ++ 推进 n 个底层元素
// 最后一块可能不满 n 个
```

**实现难点**：底层迭代器是 forward 时，chunk_view 的迭代器降级为 input——因为回溯到前一个块需要重新遍历。

## range-v3 到 C++20 Ranges 迁移要点

| 维度 | range-v3 | C++20 Ranges |
|------|----------|-------------|
| 头文件 | `<range/v3/view/filter.hpp>` | `<ranges>` |
| 命名空间 | `ranges::views`（无 `std::`） | `std::views`（即 `std::ranges::views`） |
| view 概念 | `view_<T>` | `std::ranges::view<T>` |
| 适配器命名 | `view::filter` | `std::views::filter` |
| 转换到容器 | `ranges::to<vector>()` | C++23: `std::ranges::to<vector>()` |
| zip | `view::zip`（完整实现） | C++23: `std::views::zip` |
| chunk/slide | 完整实现 | C++23: 部分实现 |
| 迭代器基础 | `ranges::iterator_t` | `std::ranges::iterator_t` |
| actions | `ranges::actions::sort` | 无直接对应（`std::ranges::sort` 可原地操作） |

### 迁移策略

```cpp
// range-v3
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>
namespace rv = ranges::views;

auto result = data | rv::filter(pred) | rv::transform(func);

// C++20
#include <ranges>
namespace sv = std::views;

auto result = data | sv::filter(pred) | sv::transform(func);

// 主要差异：
// 1. C++20 没有 actions → 使用 ranges:: 算法原地操作
// 2. ranges::to 在 C++23 才标准化
// 3. 某些 view 在 C++20 中缺失（zip, chunk, slide）
```

## 总结

```
Ranges 内部架构层次
─────────────────────────────────────────────────
用户层:      range | adaptor1 | adaptor2 | adaptor3
                ↓
管道层:      range_adaptor_closure::operator|
             pipeline_closure 闭包组合
                ↓
适配器层:    filter_closure::operator()(range)
             → filter_view(range, pred)
                ↓
View 层:     view_interface<Derived> CRTP
             自动生成 empty/front/back/size/data
                ↓
迭代器层:    view::iterator（父指针 + 底层迭代器）
             类别降级规则
                ↓
底层 range:  begin() / end() / sentinel
─────────────────────────────────────────────────
```

理解这些内部机制后：
- 可以正确实现自定义 view（继承 `view_interface`，实现 `begin/end`）
- 了解管道操作的零开销实现（CRTP + 内联闭包组合）
- 预测迭代器类别的降级对算法选择的影响
- 避免 filter_view begin 缓存陷阱和悬空迭代器问题
