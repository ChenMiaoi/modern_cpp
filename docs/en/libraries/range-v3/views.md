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
# range-v3 View Implementation

> Source paths: `references/impl/llvm-project/libcxx/include/__ranges/filter_view.h`, `transform_view.h`, `take_view.h`

## View Lazy Evaluation Data Flow

```
  vec | views::filter(pred) | views::transform(fn) | views::take(5)

  Construction phase (zero overhead, only assembling descriptors):
  +-----------+     +----------------+     +-----------------+     +----------+
  |   vec     |---->| filter_view    |---->| transform_view  |---->| take_view|
  | (underlying)    | stores: pred   |     | stores: fn      |     | stores: 5|
  +-----------+     | stores: &vec   |     | stores: &filter |     | count    |
                    +----------------+     +-----------------+     +----------+

  Iteration phase (demand-driven, pull-based):
  Traversing take_view → calls transform_view::operator*() → calls filter_view::operator++()
  → internal ranges::find_if scans element by element, skipping non-matching ones

  No intermediate storage, no heap allocation, each element goes through filter → transform → take before output
```

## filter_view

```cpp
template <input_range _View, indirect_unary_predicate<iterator_t<_View>> _Pred>
class filter_view : public view_interface<filter_view<_View, _Pred>> {
  _View __base_ = _View();
  __movable_box<_Pred> __pred_;           // optional stores the predicate
  __non_propagating_cache<iterator_t<_View>> __cached_begin_;  // cached for forward_range
};
```

**begin() implementation**: On first call, performs a linear scan to find the first matching element; subsequent calls hit the cache (only for `forward_range`).

**operator++ skip**: Calls `ranges::find_if` starting from the next position. Each `++` is a linear scan, but total cost is O(n) — each element is visited exactly once.

## transform_view

```cpp
template <input_range _View, copy_constructible _Fn>
class transform_view : public view_interface<transform_view<_View, _Fn>> {
  _View __base_ = _View();
  __movable_box<_Fn> __fn_;
};
```

**operator* implementation**: Directly dereferences the underlying iterator and applies the function, no caching.

```cpp
constexpr auto operator*() const {
  return std::invoke(*__parent_->__fn_, *__current_);
}
```

## Multiple Traversal Pitfall

```cpp
auto v = vec | views::filter(pred);
auto sz = ranges::distance(v);   // first traversal: O(n)
for (auto&& x : v) {}            // second traversal: O(n) starts over!
// Each traversal re-executes the entire chain from scratch; views are stateless (no result caching)
```

## Standard Semantics

range-v3 views follow C++20 ranges standard semantics. Core concepts include:

- **viewable_range**: Either an lvalue range or a view itself. Rvalue non-view ranges do not satisfy this concept, preventing dangling iterators.
- **view**: Satisfies `range` and is default-constructible, move-constructible, copyable (required by C++20), with no extra copy overhead (marked via the `enable_view` trait).
- **borrowed_range**: Iterators remain valid after the range is destroyed. `take_view` inherits the borrowed property of the underlying range via `enable_borrowed_range` specialization.
- **lazy evaluation**: All view adapters store only descriptors (predicate, function, count) at construction time; operations are executed element-by-element during traversal, with no intermediate container allocation.

`view_interface<Derived>` serves as a CRTP base class, providing default implementations for all views:
- `empty()`: Prefers `size() == 0` (if sized_range), otherwise `begin() == end()`
- `operator bool()`: Delegates to `!empty()`
- `front()` / `back()`: Require forward_range / bidirectional_range + common_range respectively
- `operator[]`: Requires random_access_range, delegates to `begin()[index]`
- `data()`: Requires contiguous_iterator, returns `to_address(begin())`
- `size()`: Requires forward_range + sized_sentinel_for, returns `end() - begin()`

## Core Source Paths

```
libcxx/include/__ranges/
├── view_interface.h          # CRTP base class, provides empty/front/back/operator[]/size
├── range_adaptor.h           # __range_adaptor_closure + __pipeable + operator|
├── filter_view.h             # filter_view: stores predicate + caches begin iterator
├── transform_view.h          # transform_view: stores transform function + lazy application
├── take_view.h               # take_view: count + counted_iterator/sentinel
├── movable_box.h             # __movable_box: optional wrapper for storing predicates/functions
├── non_propagating_cache.h   # __non_propagating_cache: caches begin iterator
├── all.h                     # views::all_t: converts range to view (ref_view/owning_view)
└── concepts.h                # view/range/borrowed_range and other concept definitions
```

## Core Classes / Functions

### `filter_view<View, Pred>`
- **Members**: `__base_` (underlying view), `__pred_` (`__movable_box<_Pred>` stores the predicate), `__cached_begin_` (`__non_propagating_cache` caches the first matching iterator)
- **begin()**: On first call, executes `ranges::find_if` linear scan to find the first matching element; caches the result for forward_range
- **operator++**: Internally calls `ranges::find_if(++current, end, pred)` to skip non-matching elements
- **operator--**: Only available for bidirectional_range; scans backwards until the predicate matches

### `transform_view<View, Fn>`
- **Members**: `__base_` (underlying view), `__func_` (`__movable_box<_Fn>` stores the transform function)
- **operator***: Directly `std::invoke(*__parent_->__func_, *__current_)`, no caching
- **value_type**: `remove_cvref_t<invoke_result_t<Fn&, range_reference_t<View>>>`, i.e., the pure type of the function's return value
- **iterator_category**: If the function returns a reference and the underlying range is contiguous_tag, it is demoted to random_access_tag; otherwise it inherits the underlying category

### `take_view<View>`
- **Members**: `__base_` (underlying view), `__count_` (`range_difference_t<View>` count value)
- **begin()**: Three strategies —
  1. random_access + sized_range: directly returns `begin(__base_)` (no wrapping needed)
  2. sized_range: returns `counted_iterator(begin, size)`
  3. Otherwise: returns `counted_iterator(begin, count)`
- **end()**: random_access + sized_range returns `begin + size` (iterator type); sized_range returns `default_sentinel`; otherwise returns `__sentinel`
- **size()**: `min(ranges::size(__base_), __count_)`

### `view_interface<Derived>`
CRTP base class that accesses the derived class's begin()/end() via `static_cast<Derived&>(*this)`, providing default implementations for empty, operator bool, front, back, operator[], data, and size.

### range_adaptor_closure and Pipe Operator
- `__range_adaptor_closure<_Tp>`: CRTP marker base class, no data members
- `__pipeable<_Fn>`: Inherits from `_Fn` and `__range_adaptor_closure`, wrapping any function object into a pipeable closure
- `operator|(range, closure)`: Equivalent to `closure(range)`
- `operator|(closure1, closure2)`: Composes into a new `__pipeable` via `std::compose`, implementing `g(x) = closure2(closure1(x))`

## Key Algorithms

### filter_view Iteration Advancement
```
operator++ implementation (simplified):
  current = ranges::find_if(
    std::move(++current),          // start from next position
    ranges::end(parent->base),     // up to end of underlying range
    std::ref(*parent->pred)        // apply predicate
  )
  // Total time complexity O(n): each element is visited exactly once
```

### transform_view Lazy Evaluation
```
operator* implementation:
  return std::invoke(*parent->func, *current)
  // Calls the function once per dereference, no caching
  // If function returns by value (non-reference), produces a temporary object each time
```

### take_view Count Termination
```
sentinel comparison (non-sized_range path):
  return iter.count() == 0 || iter.base() == sentinel.end()
  // counted_iterator internally maintains remaining count; terminates when count=0
  // Even if the underlying range hasn't reached the end, stops when count reaches zero
```

### Iterator Category Demotion Rules
- **filter_view**: Inherits the underlying iterator_category, but bidirectional and above remain unchanged (filter does not add capabilities)
- **transform_view**: If the function returns a non-reference (value type), contiguous_tag is demoted to random_access_tag, forward_tag is demoted to input_tag
- **take_view**: Uses counted_iterator wrapper, preserves the underlying iterator_concept

## Iterator / Reference Invalidation

### filter_view
- **Iterator invalidation**: After the underlying range is modified, the cached `__cached_begin_` may become invalid. `__non_propagating_cache` automatically clears the cache on view assignment.
- **Reference stability**: filter_view does not hold elements; references point directly to underlying range elements. If the underlying element is invalidated, the reference becomes invalid.
- **begin() semantics**: First call triggers a linear scan; subsequent calls return the cached iterator. If the underlying range structure changes, the cached iterator may dangle.

### transform_view
- **Iterator invalidation**: Internally stores a parent pointer + underlying iterator. If the underlying iterator is invalidated, the transform_view iterator is invalidated.
- **Reference stability**: If the function returns by value (non-reference), each `operator*` produces a temporary object; what is returned is a reference to the temporary (dangerous!). If the function returns an lvalue reference, it directly references the underlying element.
- **value_type semantics**: `remove_cvref_t` ensures value_type is a pure type with no reference qualifiers.

### take_view
- **Iterator invalidation**: `counted_iterator` wraps the underlying iterator; if the underlying iterator is invalidated, so is the counted iterator.
- **Sentinel invalidation**: `__sentinel` stores the underlying sentinel; if the underlying sentinel is invalidated, take_view's end() is invalidated.
- **borrowed_range propagation**: `enable_borrowed_range<take_view<T>> = enable_borrowed_range<T>`; `take_view` is borrowed only when the underlying range is borrowed.

### General Rules
- View adapters themselves do not own elements (except owning_view); iterator/reference stability depends entirely on the underlying range.
- `__movable_box` and `__non_propagating_cache` implement non-propagating semantics on view move/assignment, avoiding dangling pointers.

## Performance Model

### Time Complexity
| Operation | filter_view | transform_view | take_view |
|-----------|-------------|----------------|-----------|
| Construction | O(1) | O(1) | O(1) |
| begin() | O(n) first call, O(1) cached | O(1) | O(1) |
| operator++ | O(1) amortized (each element visited exactly once) | O(1) | O(1) |
| Full traversal | O(n) | O(n) | O(min(n, count)) |
| size() | O(n) (requires traversal) | O(1) (delegates to underlying) | O(1) (min computation) |

### Space Overhead
- **filter_view**: Stores view + `__movable_box<Pred>` + optional `__non_propagating_cache<iterator>` (for forward_range)
- **transform_view**: Stores view + `__movable_box<Fn>`
- **take_view**: Stores view + `difference_type` count value
- All view adapters are stack-allocated with no heap allocation (unless the underlying range itself allocates)

### Cache Behavior
- **filter_view**: Jumps across underlying elements during traversal; cache-unfriendly. Predicate call overhead may dominate.
- **transform_view**: Sequentially accesses underlying elements; cache-friendly. Function call overhead may dominate.
- **take_view**: Sequentially accesses the first N elements; cache-friendly. counted_iterator is a lightweight wrapper.

### Composition Overhead
```cpp
vec | views::filter(pred) | views::transform(fn) | views::take(5)
// Construction: 3 view constructions, O(1)
// Traversal: each element goes through filter → transform → take triple-check
// No intermediate containers, no heap allocation
// But each operator++ may trigger multi-layer indirect calls
```

### Comparison with Eager Algorithms
```cpp
// eager (C++17 style):
vector<int> filtered;
copy_if(vec.begin(), vec.end(), back_inserter(filtered), pred);  // O(n) + heap allocation
vector<int> transformed;
transform(filtered.begin(), filtered.end(), back_inserter(transformed), fn);  // O(k) + heap allocation
vector<int> result(transformed.begin(), transformed.begin() + min(5, transformed.size()));  // O(5) + heap allocation

// lazy (C++20 views):
auto result = vec | views::filter(pred) | views::transform(fn) | views::take(5);
// Construction O(1), traversal O(n) but no heap allocation, short-circuits (stops after finding 5)
```
