---
title: "Boost 容器与数据结构"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# Boost Containers and Data Structures

## Container: flat_map and stable_vector

### flat_map: Sorted Vector

`flat_map` uses a sorted contiguous memory array instead of a tree structure:

```cpp
boost::container::flat_map<int, std::string> fm;
fm[3] = "three";
fm[1] = "one";
// 内部存储: [(1,"one"), (3,"three")]——有序数组
```

| Operation | std::map (RB-tree) | flat_map (sorted vector) |
|-----------|---------------------|---------------------------|
| Lookup | O(log n), 3-4 pointer dereferences | O(log n), contiguous memory comparison |
| Insert/Delete | O(log n) | O(log n + n), requires shifting subsequent elements |
| Traversal | Node hopping (cache-unfriendly) | **Contiguous memory (cache-friendly)** |
| Memory overhead | 3 pointers + 1 color bit per node | **No extra pointers** |

### stable_vector: Pointer-Stable Node Container

`stable_vector` is a compromise between `std::vector` and `std::deque` — elements are distributed across nodes (like deque), but it guarantees iterator and reference stability (like list). Each node contains a value and pointers to the previous/next node.

---

## MultiIndex: Multi-Index Container

`multi_index_container` allows maintaining multiple indexes of different types over the same set of elements:

```cpp
using employee_set = mi::multi_index_container<
    employee,
    mi::indexed_by<
        mi::ordered_unique<mi::member<employee, int, &employee::id>>,
        mi::ordered_non_unique<mi::member<employee, std::string, &employee::name>>,
        mi::hashed_non_unique<mi::member<employee, int, &employee::age>>
    >>;

auto& id_index = employees.get<0>();  // 按 id 排序
auto& name_index = employees.get<1>(); // 按 name 排序
auto& age_index = employees.get<2>();  // 按 age 哈希
```

Elements are stored only once; each index maintains its own node pointers to the elements. Updating one index automatically synchronizes all indexes.

Index types: `ordered_unique/non_unique` (RB-tree), `hashed_unique/non_unique` (hash buckets), `random_access` (vector of pointers), `sequenced` (doubly-linked list).

---

## Graph (BGL): Graph Algorithm Library

The Boost Graph Library provides a generic framework for graph data structures and algorithms:

```cpp
typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS> Graph;
Graph g(5);
add_edge(0, 1, g);
add_edge(1, 2, g);

// Dijkstra 最短路径
std::vector<int> d(num_vertices(g));
dijkstra_shortest_paths(g, 0, boost::distance_map(&d[0]));
```

BGL's core design: **External Property Maps** — vertex/edge properties are not stored within the graph itself, but are retrieved from external storage via mapping functions. This allows the graph structure and its properties to evolve independently.

---

## Intrusive: Intrusive Containers

Intrusive containers require elements to contain linked-list pointers within themselves — no extra node allocation is needed:

```cpp
struct Task : boost::intrusive::list_base_hook<> {
    int id;
    std::string name;
};

boost::intrusive::list<Task> task_list;
Task t1{1, "build"}, t2{2, "test"};
task_list.push_back(t1);  // 零分配！直接用 t1 内部的 hook
task_list.push_back(t2);
```

**Advantages**: zero heap allocation, better cache locality (data and list pointers reside in the same object).
**Disadvantages**: element types must inherit from the hook; an element cannot belong to two containers of the same type simultaneously.

---

## Other Containers

| Library | Description |
|---------|-------------|
| **Bimap** | Bidirectional map — lookup from either the left or right value |
| **CircularBuffer** | Fixed-size ring buffer that overwrites the oldest data |
| **DynamicBitset** | Runtime-sized bitset (similar to `vector<bool>` but more efficient) |
| **Heap** | Various heap implementations: Fibonacci heap, binomial heap, pairing heap |
| **Container** | `small_vector`, `static_vector`, `stable_vector`, `flat_set/map` |
