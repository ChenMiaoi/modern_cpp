---
title: libc++ map/set and variant
topic: libraries
feature: map-variant
standard: C++17
status_checked_at: 2026-06-02
implementation:
  libcxx:
    paths:
      - references/impl/llvm-project/libcxx/include/__tree
      - references/impl/llvm-project/libcxx/include/variant
    symbols:
      - __tree_node
      - __end_node_
      - __variant_detail::__traits
      - __visit_alt_at
exercises: []
solutions: []
---
# libc++ map/set and variant

## std::map/set: Red-Black Tree

### Node Structure

```
__tree_node:
  __left_    (8B)
  __right_   (8B)
  __parent_  (8B)
  __is_black_ (1B + 7B padding)
  ────────────────────
  value_type (T)        ← data at the end of the node
  each node = 32B + sizeof(T)
```

### __end_node_ Sentinel

```
              __end_node_
              ┌──────────┐
              │ __left_──┼──┐ → root node
              └──────────┘  │
                   ↑        ↓
                   │     ┌──────┐
                   │     │ root │ ← __parent_ → __end_node_
                   │     └──┬───┘
                   │    ┌───┴───┐
                   │  ┌─┴─┐  ┌─┴─┐
                   │  │ L │  │ R │
                   │  └───┘  └───┘
  __begin_node_ ───┘ (leftmost node)

  end() → &__end_node_
```

### Rebalancing After Insertion (Three Cases)

- Case 1: Uncle is red → recolor (may propagate to root)
- Case 2: X is a right child → left-rotate parent → becomes Case 3
- Case 3: X is a left child → recolor + right-rotate grandparent

At most 2 rotations, recoloring may take O(log n) steps.

## std::variant: Function Pointer Table Visit

```
variant<int, string, double> v = 42;

Compile-time generated function pointer table:
  __table[0] = [](vis, v) { return vis(get<int>(v)); }
  __table[1] = [](vis, v) { return vis(get<string>(v)); }
  __table[2] = [](vis, v) { return vis(get<double>(v)); }

Runtime: __table[v.index()](visitor, v)
Multiple variants: visit(f, v1, v2) → N×M function pointers
```
