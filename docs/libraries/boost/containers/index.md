# Boost 容器与数据结构

## Container：flat_map 与 stable_vector

### flat_map：排序向量

`flat_map` 使用排序的连续内存数组而非树结构：

```cpp
boost::container::flat_map<int, std::string> fm;
fm[3] = "three";
fm[1] = "one";
// 内部存储: [(1,"one"), (3,"three")]——有序数组
```

| 操作 | std::map (RB-tree) | flat_map (sorted vector) |
|------|---------------------|---------------------------|
| 查找 | O(log n)，3-4 次指针解引用 | O(log n)，连续内存比较 |
| 插入/删除 | O(log n) | O(log n + n)，需移动后续元素 |
| 遍历 | 节点跳跃（缓存不友好） | **连续内存（缓存友好）** |
| 内存开销 | 每节点 3 指针+1 颜色位 | **无额外指针** |

### stable_vector：指针稳定的节点容器

`stable_vector` 是 `std::vector` 和 `std::deque` 的折中——元素分布在节点中（像 deque），但保证迭代器和引用稳定性（像 list）。每个节点包含一个值和一个指向前一个/后一个节点的指针。

---

## MultiIndex：多索引容器

`multi_index_container` 允许同一组元素维护多个不同类型的索引：

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

元素只存储一份，多个索引各自维护指向元素的节点指针。更新一个索引自动同步所有索引。

索引类型：`ordered_unique/non_unique`（RB-tree）、`hashed_unique/non_unique`（哈希桶）、`random_access`（vector 指针）、`sequenced`（双向链表）。

---

## Graph (BGL)：图算法库

Boost Graph Library 提供图数据结构和算法的通用框架：

```cpp
typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS> Graph;
Graph g(5);
add_edge(0, 1, g);
add_edge(1, 2, g);

// Dijkstra 最短路径
std::vector<int> d(num_vertices(g));
dijkstra_shortest_paths(g, 0, boost::distance_map(&d[0]));
```

BGL 的核心设计：**外部属性映射**（External Property Maps）——图的顶点/边属性不存储在图中，而是通过映射函数从外部存储获取。这使得图结构和属性可以独立变化。

---

## Intrusive：侵入式容器

侵入式容器要求元素自身包含链表指针——不需要额外的节点分配：

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

**优势**：零堆分配、更好的缓存局部性（数据和链表指针在同一对象中）。
**劣势**：元素类型必须继承 hook，元素不能同时属于两个同类型容器。

---

## 其他容器

| 库 | 说明 |
|---|------|
| **Bimap** | 双向映射——可以从左值或右值查找 |
| **CircularBuffer** | 固定大小的环形缓冲区，覆盖最旧数据 |
| **DynamicBitset** | 运行时大小的位集（类似 `vector<bool>` 但更高效） |
| **Heap** | 各种堆实现：斐波那契堆、二项堆、配对堆 |
| **Container** | `small_vector`、`static_vector`、`stable_vector`、`flat_set/map` |
