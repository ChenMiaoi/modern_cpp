---
title: libc++ map/set 与 variant
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
# libc++ map/set 与 variant

## std::map/set：红黑树

### 节点结构

```
__tree_node:
  __left_    (8B)
  __right_   (8B)
  __parent_  (8B)
  __is_black_ (1B + 7B padding)
  ────────────────────
  value_type (T)        ← 数据在节点末尾
  每个节点 = 32B + sizeof(T)
```

### __end_node_ 哨兵

```
              __end_node_
              ┌──────────┐
              │ __left_──┼──┐ → 根节点
              └──────────┘  │
                   ↑        ↓
                   │     ┌──────┐
                   │     │ root │ ← __parent_ → __end_node_
                   │     └──┬───┘
                   │    ┌───┴───┐
                   │  ┌─┴─┐  ┌─┴─┐
                   │  │ L │  │ R │
                   │  └───┘  └───┘
  __begin_node_ ───┘ (最左节点)

  end() → &__end_node_
```

### 插入后重平衡（三种情况）

- Case 1: 叔叔红 → 变色（可能传播到根）
- Case 2: X 是右孩子 → 左旋父节点 → 变成 Case 3
- Case 3: X 是左孩子 → 变色 + 右旋祖父

最多旋转 2 次，变色可能 O(log n) 次。

## std::variant：函数指针表 visit

```
variant<int, string, double> v = 42;

编译期生成函数指针表：
  __table[0] = [](vis, v) { return vis(get<int>(v)); }
  __table[1] = [](vis, v) { return vis(get<string>(v)); }
  __table[2] = [](vis, v) { return vis(get<double>(v)); }

运行时：__table[v.index()](visitor, v)
多 variant：visit(f, v1, v2) → N×M 个函数指针
```
