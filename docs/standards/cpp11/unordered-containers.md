---
title: "C++11 无序容器"
topic: unknown
feature: unordered-containers
standard: N/A
status_checked_at: 2026-06-02
---
# C++11 无序容器

## 概述

C++11 引入四种无序关联容器：`unordered_map`、`unordered_set`、`unordered_multimap`、`unordered_multiset`，定义在 `<unordered_map>` 和 `<unordered_set>` 中。基于哈希表实现，提供平均 O(1) 的查找、插入和删除，替代了 C++98 中需要手动维护的 `std::map`/`std::set`（红黑树，O(log n)）。

底层是桶数组。每个元素通过哈希函数映射到某个桶，桶内以链表存储冲突元素。当负载因子超过阈值时自动 rehash——增加桶数并重新分配元素。

## 核心 API

### 四种无序容器

| 容器 | 允许重复键 | 映射关系 |
|------|-----------|----------|
| `unordered_map<K,V>` | 否 | 键→值 |
| `unordered_set<K>` | 否 | 仅键 |
| `unordered_multimap<K,V>` | 是 | 键→值 |
| `unordered_multiset<K>` | 是 | 仅键 |

### 基本用法

```cpp
#include <unordered_map>
#include <unordered_set>
#include <string>

// unordered_map — 键值对，键唯一
std::unordered_map<std::string, int> scores;
scores["Alice"] = 95;
scores.insert({"Charlie", 92});
scores.emplace("Diana", 88);      // 原地构造

auto it = scores.find("Alice");
if (it != scores.end()) {
    std::cout << it->first << ": " << it->second << "\n";  // Alice: 95
}

// C++20 有 contains()，C++11 用 count()
if (scores.count("Bob") > 0) { /* ... */ }

scores.erase("Bob");

// unordered_set — 仅存储键
std::unordered_set<std::string> names;
names.insert("Alice");
names.insert("Alice");  // 重复插入，无效果
std::cout << names.size() << "\n";  // 1

// unordered_multimap — 允许重复键
std::unordered_multimap<std::string, int> course_grades;
course_grades.emplace("Alice", 95);
course_grades.emplace("Alice", 88);
// equal_range 获取同一键的所有值
auto range = course_grades.equal_range("Alice");
for (auto it = range.first; it != range.second; ++it) {
    std::cout << it->first << ": " << it->second << "\n";
}
```

### 性能特征

| 操作 | 平均 | 最坏 |
|------|------|------|
| 插入/删除/查找 | O(1) | O(n) |

最坏情况 O(n) 发生在所有元素哈希到同一个桶时。好的哈希函数可有效避免。

## 哈希函数定制

### 为自定义类型特化 std::hash

```cpp
#include <functional>

struct Point {
    int x, y;
    bool operator==(const Point& o) const {
        return x == o.x && y == o.y;
    }
};

namespace std {
template<>
struct hash<Point> {
    size_t operator()(const Point& p) const {
        size_t h1 = std::hash<int>{}(p.x);
        size_t h2 = std::hash<int>{}(p.y);
        return h1 ^ (h2 * 2654435761u + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};
}

// 现在可直接使用
std::unordered_set<Point> points;
points.insert({1, 2});
```

### 使用自定义哈希函数对象

当无法修改 `std` 命名空间（如第三方类型），通过模板参数传入：

```cpp
struct Record { std::string name; int id; };

struct RecordHash {
    size_t operator()(const Record& r) const {
        size_t h1 = std::hash<std::string>{}(r.name);
        size_t h2 = std::hash<int>{}(r.id);
        return h1 ^ (h2 * 2654435761u + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};

struct RecordEqual {
    bool operator()(const Record& a, const Record& b) const {
        return a.name == b.name && a.id == b.id;
    }
};

std::unordered_map<Record, double, RecordHash, RecordEqual> prices;
prices[{"apple", 1}] = 3.5;
```

## 桶接口与负载管理

### 桶（Bucket）接口

```cpp
std::unordered_map<std::string, int> map = {
    {"alpha", 1}, {"beta", 2}, {"gamma", 3}
};

std::cout << "桶数量: " << map.bucket_count() << "\n";
std::cout << "负载因子: " << map.load_factor() << "\n";
std::cout << "alpha 在桶: " << map.bucket("alpha") << "\n";

// 遍历单个桶中的元素
size_t idx = map.bucket("alpha");
for (auto it = map.begin(idx); it != map.end(idx); ++it) {
    std::cout << "  " << it->first << ": " << it->second << "\n";
}
```

### 负载因子与 reserve

```cpp
std::unordered_map<std::string, int> map;

map.max_load_factor(0.75);  // 更低 = 更少冲突 = 更多内存

// 预留空间——避免多次 rehash（最重要的性能优化）
map.reserve(1000);  // 一次性分配足够的桶

// 手动触发 rehash
map.rehash(1024);   // 确保桶数 >= 1024

// 预留的典型用法
std::unordered_map<int, std::string> lookup;
lookup.reserve(10000);
for (int i = 0; i < 10000; ++i) {
    lookup[i] = "item_" + std::to_string(i);
}
```

## 有序 vs 无序容器的选择

```cpp
// 需要范围查询 → 用 ordered
std::map<int, std::string> ordered;
// 查找 [2, 6) 范围内元素
auto lo = ordered.lower_bound(2);
auto hi = ordered.upper_bound(6);
for (auto it = lo; it != hi; ++it) { /* ... */ }

// 纯查找/存在性检查 → 用 unordered
std::unordered_set<int> fast_lookup;
fast_lookup.insert(42);
bool exists = fast_lookup.count(42) > 0;  // O(1)
```

**选 unordered**：需要 O(1) 查找，不需排序遍历，键有好哈希，数据量较大。
**选 ordered**：需要有序遍历、范围查询、键无好哈希、内存敏感、需要稳定的迭代器。

## 自定义类型作为键——完整示例

```cpp
#include <unordered_map>
#include <string>

struct StudentId {
    int grade, class_num, number;
    bool operator==(const StudentId& o) const {
        return grade == o.grade && class_num == o.class_num && number == o.number;
    }
};

struct StudentIdHash {
    size_t operator()(const StudentId& id) const {
        size_t seed = std::hash<int>{}(id.grade);
        seed ^= std::hash<int>{}(id.class_num)
              + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<int>{}(id.number)
              + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};

int main() {
    std::unordered_map<StudentId, std::string, StudentIdHash> students;
    students[{3, 2, 15}] = "张三";
    students[{3, 2, 8}]  = "李四";

    auto it = students.find({3, 2, 15});
    if (it != students.end()) std::cout << it->second << "\n";  // 张三
}
```

## 最佳实践

1. **提前 `reserve`**：知道大致元素数量时调用 `reserve(n)`，避免反复 rehash。最简单有效的性能优化。
2. **哈希函数质量决定性能**：简单 XOR 所有字段是反模式——应为不同字段使用不同混合偏移。
3. **`emplace` 优于 `insert`**：原地构造，避免临时 pair 的拷贝/移动。
4. **`operator[]` 会插入默认值**：仅查询时用 `find()` 或 `count()`，避免污染容器。
5. **批量插入后手动 rehash**：`map.rehash(0)` 根据当前 size 和 max_load_factor 调整桶数。

## 常见陷阱

- **`operator[]` 的隐式插入**：`auto& v = map[key];` 当 key 不存在时插入默认值。只读查询用 `find()`。
- **rehash 使迭代器失效**：insert/erase 导致 rehash 时，所有迭代器和引用失效。遍历中修改容器要格外小心。
- **哈希冲突导致 O(n)**：质量差的自定义哈希函数比 `std::map` 更慢。
- **`unordered_multimap::erase` 行为**：`erase(it)` 删除一个元素，`erase(key)` 删除所有匹配键的元素。
- **内存开销比有序容器大**：需要桶数组 + 链表指针。少量元素时 `std::map` 可能更省内存且更快。
- **浮点数作键**：`std::hash<double>` 对 NaN 行为未定义。避免浮点数作为无序容器的键。
