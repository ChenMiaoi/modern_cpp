# EASTL 侵入式容器与哈希表

> 源码路径：`references/impl/EASTL/include/EASTL/intrusive_list.h`, `internal/hashtable.h`

## 侵入式容器

侵入式容器要求元素自身包含链表节点：

```cpp
struct Task : public eastl::intrusive_list_node {
    int id;
    std::string name;
};

eastl::intrusive_list<Task> task_list;
Task t1{1, "build"}, t2{2, "test"};
task_list.push_back(t1);  // 零分配
task_list.push_back(t2);
```

优势：零堆分配、数据和链表指针在同一对象内（缓存友好）、不需要额外节点管理。

## 哈希表

EASTL 的哈希表使用开放寻址 + 线性探测：

```cpp
// hashtable.h 中的 bucket 结构
struct node {
    node* next;      // 链式溢出
    hash_node hash_data;  // key + value
};

// 桶数组
node** mpBucketArray;  // 指针数组
size_t mnBucketCount;
size_t mnElementCount;
```

EASTL 的 `hash_map` 和 `hash_set` 支持侵入式版本（`intrusive_hash_map`、`intrusive_hash_set`），元素自带哈希值和链表指针。

## 红黑树

EASTL 的有序容器（`map`、`set`）基于 `red_black_tree`（`internal/red_black_tree.h`），实现与标准库类似但无异常处理。
