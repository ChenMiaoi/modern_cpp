---
title: "Ranges Implementation Internals: view_interface, Pipe Operations, Iterator Demotion, and Adapter Implementation"
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

# Ranges Implementation Internals

## Overview

The user-facing API of the C++20 Ranges library is clean — `views::filter`, `views::transform`, pipe `|` operations. But the implementation involves extensive CRTP base classes, trait mechanisms, and lazy iterator design. This article takes a standard library implementer's perspective, revealing how `view_interface` auto-generates default members through CRTP, how `range_adaptor_closure` implements pipe composition, how `filter_view` caches `begin()`, how `transform_view` maintains a parent pointer, the design value of sentinel/iterator separation, view iterator category demotion rules, how `borrowed_range` prevents dangling iterators, and range-v3 to C++20 migration notes.

## view_interface CRTP Base Class

`std::ranges::view_interface<D>` is a CRTP base class that auto-generates default implementations of `empty()`, `operator bool()`, `data()`, `size()`, `front()`, `back()`, `operator[]`, and other members for derived classes.

### CRTP Mechanism

```cpp
template <std::ranges::view Derived>
class view_interface {
protected:
    // CRTP: downcast to derived type
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

**Generation conditions**: Each default member uses `requires` to check the underlying iterator capabilities. If the derived class does not satisfy `random_access_range`, `operator[]` will not be instantiated (SFINAE-friendly).

### Custom Derived Class Example

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

    // Inherited from view_interface:
    // empty(), operator bool(), front(), back(), operator[], size(), data()
    // All auto-generated from begin()/end()
};
```

### CRTP Implementation Overhead

CRTP is zero-overhead abstraction — all member functions call the derived class's `begin()`/`end()` via `static_cast`, and compiler inlining produces no runtime overhead. No virtual function table (vtable) is introduced.

## viewable_range Definition

```cpp
// C++20 definition
template <typename T>
concept viewable_range =
    range<T> && (
        // Case 1: lvalue range is always viewable
        std::is_lvalue_reference_v<T> ||
        // Case 2: rvalue view (move semantics)
        (movable<remove_cvref_t<T>> && !__is_derived_from_view_base<remove_cvref_t<T>>)
        // Case 3: rvalue view subclass (special handling)
        || view<remove_cvref_t<T>>
    );
```

**Design intent**:
- Lvalue range (`vector&`) can directly construct a view (view stores a reference)
- Rvalue non-view range can be safely taken over by a view (usually moved)
- Rvalue view can be directly moved into a pipeline
- Rvalue non-movable range cannot construct a view (cannot be safely stored)

## borrowed_range and Dangling Iterator Protection

```cpp
// borrowed_range: the range's iterators remain valid after the range itself is destroyed
template <typename T>
concept borrowed_range = range<T> && (
    std::is_lvalue_reference_v<T> ||
    enable_borrowed_range<remove_cvref_t<T>>
);
```

borrowed_range types in the standard library:
- `std::span<T>` (does not own data)
- `std::string_view` (does not own data)
- All lvalue range references

Non-borrowed_range types:
- `std::vector<T>` rvalue (iterators dangle after destruction)
- `std::string` rvalue

```cpp
// dangling iterator protection
auto result = std::ranges::find(std::vector{1, 2, 3}, 2);
// Return type is std::ranges::dangling, not an iterator
// Any operation on dangling is a compile error (not UB)

// Correct usage: keep the range alive
std::vector v = {1, 2, 3};
auto it = std::ranges::find(v, 2);  // v is lvalue → real iterator
```

## enable_view / enable_borrowed_range Opt-in Traits

### enable_view

```cpp
// Deriving from view_base automatically opts in
struct my_view : std::ranges::view_interface<my_view> {
    // ...
};

// Or specialize enable_view
template <>
inline constexpr bool std::ranges::enable_view<my_legacy_view> = true;
```

Logic for determining whether a type is a view:

```cpp
template <typename T>
concept view =
    range<T> &&
    movable<T> &&
    enable_view<T>;  // key: requires explicit opt-in
```

**Why opt-in is needed**: `vector` and `string` satisfy `range` and `movable`, but they are not views — they own data and copying is expensive. Opt-in prevents them from being accidentally used as views.

### enable_borrowed_range

```cpp
// Mark a custom type as borrowed
template <>
inline constexpr bool std::ranges::enable_borrowed_range<my_span> = true;
```

**Safety warning**: Only opt-in to borrowed_range when iterators do not hold a reference to the range body. Incorrect marking will cause dangling iterators without compiler protection.

## range_adaptor_closure and Pipe Operations

### How the Pipe `|` Works

Pipe operations work in two steps:
1. `range | adaptor` → adapter applied to range
2. `adaptor1 | adaptor2` → two adapters compose into a new closure

```cpp
// range_adaptor_closure CRTP base class
template <typename D>
struct range_adaptor_closure {};

// range | closure: direct call
template <typename R, typename C>
    requires range<R> && derived_from<C, range_adaptor_closure<C>>
constexpr auto operator|(R&& r, C&& c) {
    return std::forward<C>(c)(std::forward<R>(r));
}

// closure | closure: compose two closures
template <typename C1, typename C2>
    requires derived_from<C1, range_adaptor_closure<C1>>
          && derived_from<C2, range_adaptor_closure<C2>>
constexpr auto operator|(C1&& c1, C2&& c2) {
    // Return new closure that internally stores both closures
    return pipeline_closure(std::forward<C1>(c1), std::forward<C2>(c2));
}
```

### Composed Closure Implementation

```cpp
template <typename C1, typename C2>
struct pipeline_closure : range_adaptor_closure<pipeline_closure<C1, C2>> {
    C1 c1_;
    C2 c2_;

    template <typename R>
    constexpr auto operator()(R&& r) {
        // Apply c1 first, then c2
        return std::forward<C2>(c2_)(std::forward<C1>(c1_)(std::forward<R>(r)));
    }
};
```

### views::filter Adapter Object Implementation

```cpp
namespace std::ranges::views {

struct filter_fn {
    // Direct call form: filter(range, pred)
    template <viewable_range R, typename Pred>
    constexpr auto operator()(R&& r, Pred pred) const {
        return filter_view(std::forward<R>(r), std::move(pred));
    }

    // Curried form: filter(pred) → returns closure
    template <typename Pred>
    constexpr auto operator()(Pred pred) const {
        // Return range_adaptor_closure subclass
        return filter_closure<Pred>{std::move(pred)};
    }
};

inline constexpr filter_fn filter{};  // function object

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

**Two call forms**:

```cpp
// Form 1: direct call (two arguments)
auto v1 = std::views::filter(data, pred);

// Form 2: piped (one argument returns closure)
auto v2 = data | std::views::filter(pred);
// Equivalent to filter_closure{pred}(data) → filter_view(data, pred)

// Form 3: composition
auto v3 = data
    | std::views::filter(pred1)
    | std::views::transform(func);
// pipeline_closure(filter_closure{pred1}, transform_closure{func})(data)
```

## filter_view begin Caching Mechanism

`filter_view`'s `begin()` needs to skip prefix elements that don't satisfy the predicate. To avoid re-scanning the prefix on every `begin()` call, the standard specifies a caching mechanism:

```cpp
template <view V, typename Pred>
class filter_view : public view_interface<filter_view<V, Pred>> {
    V base_;
    Pred pred_;
    // Cache: points to the first element satisfying the predicate
    optional<iterator_t<V>> cached_begin_;

public:
    constexpr iterator begin() {
        if (!cached_begin_) {
            // First call: find the first element satisfying the predicate
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

**Cache invalidation conditions**:
- Cache is invalidated when the `filter_view` is moved
- The standard does not require automatic cache refresh when the underlying range changes (lazy semantics)
- **Practical trap**: after modifying the underlying range and re-traversing the filter_view, the cached `begin` may point to an invalid position

**C++23 improvement**: `filter_view`'s iterator behavior when the underlying iterator is invalidated is more well-defined.

## transform_view: Parent Pointer + Current Iterator

```cpp
template <view V, typename F>
class transform_view : public view_interface<transform_view<V, F>> {
    V base_;
    F fun_;

public:
    class iterator {
        transform_view* parent_;       // pointer to parent view (non-owning)
        iterator_t<V> current_;        // underlying iterator

    public:
        using value_type = remove_cvref_t<invoke_result_t<F&, range_reference_t<V>>>;
        using reference = invoke_result_t<F&, range_reference_t<V>>;
        using difference_type = range_difference_t<V>;
        // Iterator category demotion (see below)

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
        // ... other operations delegate to current_
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

**Design rationale for parent pointer**: `operator*()` needs to access `parent_->fun_` to apply the transformation function. The iterator references the parent view through a non-owning pointer — meaning the parent view must outlive the iterator (a universal requirement for standard library views).

**Difference from range-v3**: range-v3 uses a `semiregular` wrapper to store function objects, allowing iterators to exist independently of the parent view. C++20 standard library chose the simpler parent pointer approach.

## Sentinel vs Iterator: Why Different Types Have Value

### Design Motivation

Traditional STL requires `begin()` and `end()` to return the same type. This forces certain sentinels to be full iterators, wasting space:

```cpp
// Traditional STL problem
for (auto it = begin; it != end; ++it) { ... }
// end iterator carries redundant state: data pointer + length
// But it really only needs to know "reached the end"
```

### Ranges Sentinel Separation

```cpp
template <view V, typename Pred>
class filter_view {
    // ...
    class sentinel {
        sentinel_t<V> end_;   // only holds the underlying sentinel
    public:
        constexpr sentinel(sentinel_t<V> end) : end_(end) {}
    };

    constexpr auto end() { return sentinel{ranges::end(base_)}; }
    // Return type differs from begin()
};
```

**Sentinels are smaller and simpler than iterators**. In `filter_view`, `end()` only needs the underlying sequence's end marker, without the predicate, parent pointer, or current element iterator.

### Practical Benefits

```cpp
// Old code needed common_view adaptation
auto v = std::views::iota(1, 10);  // iterator != sentinel
// std::sort(v.begin(), v.end());  // compile error: type mismatch

// C++23 ranges algorithms natively support different sentinel types
std::ranges::sort(v | std::views::common);  // requires common
// Or C++23 some algorithms directly support sentinel
```

## Lazy View Iterator Category Demotion Rules

View adapter iterator categories are typically lower than the underlying range's category:

```
Iterator Category Demotion Rules
─────────────────────────────────────────────────
Adapter             Underlying    Result Category
─────────────────────────────────────────────────
transform_view    contiguous     random_access
                  random_access  random_access
                  bidirectional  bidirectional
                  forward        forward
                  input          input
─────────────────────────────────────────────────
filter_view       any            forward (at most)
                  input          forward
                  forward        forward
                  random_access  forward (demoted!)
─────────────────────────────────────────────────
join_view         forward(ext)   input
                  bidirectional  bidirectional*
─────────────────────────────────────────────────
zip_view          min(ranges)    min(ranges)
─────────────────────────────────────────────────
```

**Why filter_view is always forward**: filtering requires skipping elements that don't satisfy the predicate during traversal. `operator--` for bidirectional filtering is possible (but complex), and the C++20 standard chose a simple implementation: filter_view's iterator is at most a `forward_iterator`. Even if the underlying range is `random_access`, `operator[]` and `operator-` cannot be efficiently implemented.

**Why transform_view demotes contiguous**: `transform_view`'s `operator*()` returns the function application result, not a reference to the original element. `contiguous_iterator` requires `operator*` to return a reference to the element, but the transformation result is typically not a reference to underlying storage, so `contiguous` demotes to `random_access`.

```cpp
// Verify iterator categories
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

`std::ranges::subrange` has three forms, controlled by the `subrange_kind` enum:

```cpp
enum class subrange_kind { sized, unsized };

template <std::input_or_output_iterator I,
          std::sentinel_for<I> S = I,
          subrange_kind K = sized_and_borrowed<I, S>() ? subrange_kind::sized
                                                       : subrange_kind::unsized>
class subrange;
```

- **`sized`**: subrange can compute `size()` in O(1) (requires `S - I` to be valid)
- **`unsized`**: size can only be computed by traversal (O(n))

```cpp
std::vector v = {1, 2, 3, 4, 5};
// sized subrange (iterator is random_access)
std::ranges::subrange sub1(v.begin() + 1, v.begin() + 4);
static_assert(std::ranges::sized_range<decltype(sub1)>);
sub1.size();  // 3 — O(1)

// unsized subrange (iterator is forward)
std::forward_list fl = {1, 2, 3, 4, 5};
auto it = fl.begin(); std::advance(it, 1);
std::ranges::subrange<
    decltype(it), decltype(fl.end()),
    std::ranges::subrange_kind::unsized
> sub2(it, fl.end());
// sub2.size() is not available — requires traversal
```

## C++23 zip_view and chunk_view Implementation Notes

### zip_view

```cpp
template <typename... Views>
class zip_view : public view_interface<zip_view<Views...>> {
    std::tuple<Views...> bases_;

public:
    class iterator {
        std::tuple<iterator_t<Views>...> current_;
        // operator* returns tuple<reference...>
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

**Implementation difficulties**:
- Iterator category takes the **minimum common category** of all sub-ranges
- `zip_view` ends when any sub-range reaches the end
- `operator*` returns a `tuple` reference type, requiring handling of rvalue/lvalue mixing

### chunk_view

```cpp
// C++23 chunk_view splits into fixed-size chunks
auto chunks = data | std::views::chunk(3);
// Internally maintains a "current chunk" subrange
// Each ++ advances n underlying elements
// The last chunk may have fewer than n elements
```

**Implementation difficulty**: When the underlying iterator is forward, chunk_view's iterator demotes to input — because backtracking to the previous chunk requires re-traversal.

## range-v3 to C++20 Ranges Migration Notes

| Dimension | range-v3 | C++20 Ranges |
|-----------|----------|-------------|
| Header | `<range/v3/view/filter.hpp>` | `<ranges>` |
| Namespace | `ranges::views` (no `std::`) | `std::views` (i.e., `std::ranges::views`) |
| View concept | `view_<T>` | `std::ranges::view<T>` |
| Adapter naming | `view::filter` | `std::views::filter` |
| Convert to container | `ranges::to<vector>()` | C++23: `std::ranges::to<vector>()` |
| zip | `view::zip` (full impl) | C++23: `std::views::zip` |
| chunk/slide | Full impl | C++23: partial impl |
| Iterator basis | `ranges::iterator_t` | `std::ranges::iterator_t` |
| actions | `ranges::actions::sort` | No direct equivalent (`std::ranges::sort` can operate in-place) |

### Migration Strategy

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

// Key differences:
// 1. C++20 has no actions → use ranges:: algorithms for in-place operations
// 2. ranges::to was standardized in C++23
// 3. Some views are missing in C++20 (zip, chunk, slide)
```

## Summary

```
Ranges Internal Architecture Layers
─────────────────────────────────────────────────
User layer:      range | adaptor1 | adaptor2 | adaptor3
                ↓
Pipeline layer:  range_adaptor_closure::operator|
                 pipeline_closure closure composition
                ↓
Adapter layer:   filter_closure::operator()(range)
                 → filter_view(range, pred)
                ↓
View layer:      view_interface<Derived> CRTP
                 auto-generates empty/front/back/size/data
                ↓
Iterator layer:  view::iterator (parent pointer + underlying iterator)
                 category demotion rules
                ↓
Underlying range: begin() / end() / sentinel
─────────────────────────────────────────────────
```

Understanding these internals enables:
- Correctly implementing custom views (inherit `view_interface`, implement `begin/end`)
- Understanding the zero-overhead implementation of pipe operations (CRTP + inline closure composition)
- Predicting how iterator category demotion affects algorithm selection
- Avoiding filter_view begin caching traps and dangling iterator issues
