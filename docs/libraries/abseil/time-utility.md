# Abseil Time 与基础工具

> 源码路径：`references/impl/abseil-cpp/absl/time/`, `absl/types/`, `absl/container/`

## absl::Time 与 absl::Duration

Abseil 的时间库是 Google 日历、Spanner 等产品的基础。核心类型：

```cpp
// Duration：纳秒精度的时间间隔
class Duration {
  int64_t rep_hi_;  // 秒的高位
  uint32_t rep_lo_;  // 纳秒的低位（0-999999999）
};

// Time：Unix 纪元以来的绝对时间点
class Time {
  Duration d_;  // 相对于 Unix 纪元
};

// TimeZone：IANA 时区数据库的封装
class TimeZone {
  // 内部引用 cctz 库（civil_time），Google 自己的时区库
  // 支持 DST 转换、历史时区规则
};
```

**为什么不用 `std::chrono`？** `std::chrono` 在 C++11 时代缺少时区支持（直到 C++20 才有 `std::chrono::time_zone`）。Abseil 的时间库从 2011 年就在 Google 内部使用，比标准早了近十年。其 API 更直观：

```cpp
absl::Time now = absl::Now();
absl::Time tomorrow = now + absl::Hours(24);

// 时区转换
absl::TimeZone tz = absl::time_internal::LoadTimeZone("America/New_York");
absl::CivilMinute cm = absl::ToCivilMinute(now, tz);
```

## absl::Span

`absl::Span<T>` 是 `std::span<T>` 的前身——对连续内存的非拥有视图：

```cpp
template <typename T>
class Span {
  T* data_;
  size_t size_;
};

// 用法
void Process(absl::Span<const int> data) {
  for (int x : data) { ... }
}

Process({1, 2, 3});                      // 从 initializer_list
Process(std::vector<int>{1, 2, 3});      // 从 vector
Process(absl::MakeConstSpan(arr, 3));    // 从数组
```

## absl::optional

`absl::optional<T>` 是 `std::optional<T>` 的前身。实现策略：

```cpp
template <typename T>
class optional {
  union {
    char dummy_;
    T value_;
  };
  bool engaged_;
};
```

当 `engaged_ = true` 时，`value_` 中有有效值；当 `engaged_ = false` 时，`value_` 处于未析构状态（union 的 `dummy_` 成员活跃）。

## absl::flat_hash_set / flat_hash_map

除了底层的 SwissTable 机制（见 [SwissTable 章节](/libraries/abseil/swisstable)），Abseil 还提供一系列容器特化：

| 容器 | 特性 |
|------|------|
| `flat_hash_set/map` | 内联存储，缓存友好，迭代器不稳定 |
| `node_hash_set/map` | 节点分配，引用/迭代器稳定 |
| `btree_set/map` | B-tree 有序容器，比 `std::set/map` 更快 |
| `btree_map` | cache 友好的有序 map，节点更大（减少树高度） |

`btree_set/map` 是 Abseil 对 `std::set/map`（红黑树）的替代——B-tree 的每个节点存储多个元素（通常填满一个 cache line），减少了树高度和指针追逐次数。在有序容器场景下，`absl::btree_map` 通常比 `std::map` 快 2-5 倍。
