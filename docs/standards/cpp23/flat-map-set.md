# std::flat_map / std::flat_set

C++23 引入 `std::flat_map` 和 `std::flat_set`，底层使用排序的 `std::vector` 实现关联容器。相比红黑树实现的 `std::map`/`std::set`，它们在小型数据集和缓存敏感场景中性能更优。

## 基本用法

```cpp
#include <flat_map>
#include <flat_set>
#include <iostream>

int main() {
    std::flat_map<std::string, int> scores;
    scores["Alice"] = 95;
    scores["Bob"] = 87;
    scores["Charlie"] = 92;

    for (const auto& [name, score] : scores) {
        std::cout << name << ": " << score << "\n";
    }
    // 按键排序: Alice, Bob, Charlie

    std::flat_set<int> ids = {3, 1, 4, 1, 5, 9, 2, 6};
    // ids = {1, 2, 3, 4, 5, 6, 9}
    std::cout << ids.contains(4) << "\n";
}
```

## 内部结构

```cpp
// flat_map 底层是两个平行的排序 vector
template <typename Key, typename T, typename Compare = std::less<Key>,
          typename KeyContainer = std::vector<Key>,
          typename MappedContainer = std::vector<T>>
class flat_map {
    KeyContainer keys_;
    MappedContainer values_;  // keys_[i] 对应 values_[i]
};
```

## 构造方式

```cpp
std::flat_map<std::string, int> fm;  // 默认构造

// 从已排序容器构造（O(n)，跳过排序）
std::vector<std::pair<std::string, int>> sorted = {
    {"a", 1}, {"b", 2}, {"c", 3}
};
std::flat_map<std::string, int> fm_sorted(std::sorted_unique, sorted);

// 从未排序数据构造（O(n log n)）
std::vector<std::pair<int, std::string>> unsorted = {
    {3, "c"}, {1, "a"}, {2, "b"}, {1, "x"}
};
std::flat_map<int, std::string> fm_unsorted(unsorted.begin(), unsorted.end());
// {1: "x"}, {2: "b"}, {3: "c"}
```

## 查找操作

```cpp
std::flat_map<int, std::string> fm = {{1, "a"}, {3, "c"}, {5, "e"}};

auto it = fm.find(3);              // O(log n)
bool has = fm.contains(4);         // false
auto lb = fm.lower_bound(2);      // 指向 (3, "c")
size_t c = fm.count(3);            // 1
```

## 插入与删除

```cpp
std::flat_map<int, std::string> fm;

// 插入 — O(n) 最坏（需移动元素）
fm.insert({3, "c"});
fm.emplace(5, "e");

// hint 插入（更快）
auto hint = fm.end();
fm.insert(hint, {4, "d"});

// 删除 — O(n)
fm.erase(3);

// 批量插入
std::vector<std::pair<int, std::string>> batch = {{6, "f"}, {7, "g"}};
fm.insert_range(batch);
```

## 与 std::map / std::set 对比

| 特性 | `std::map`/`set` | `std::flat_map`/`flat_set` |
|------|------------------|---------------------------|
| 底层结构 | 红黑树 | 排序 vector |
| 查找 | O(log n) | O(log n)，常数因子更小 |
| 插入/删除 | O(log n) | O(n)（需移动元素） |
| 缓存局部性 | 差（节点分散） | 好（连续内存） |
| 迭代器稳定性 | 稳定 | 不稳定 |
| 内存开销 | 每节点有指针 | 紧凑，无额外指针 |

性能经验：
- **< 100 元素**：flat 几乎总是更快
- **100-1000**：取决于读写比例
- **> 1000 且写入频繁**：map 通常更优

## flat_multimap / flat_multiset

允许重复键的版本：

```cpp
std::flat_multimap<int, std::string> fmm;
fmm.insert({1, "a"});
fmm.insert({1, "b"});  // 允许重复

std::flat_multiset<int> fms = {1, 1, 2, 2, 3};
```

## 适用场景

```
✓ 配置表（键少，读多写少）
✓ 查找表/LUT（构建后只读）
✓ 缓存小型映射（< 1000 条目）
✗ 频繁插入/删除（用 map/set）
✗ 需要迭代器/引用稳定性（用 map/set）
```

## 注意事项

- 插入/删除会使所有迭代器、指针、引用失效
- `keys()` 和 `values()` 提供对底层容器的 const 访问
- 比较运算符按字典序比较（与 `std::map` 一致）
