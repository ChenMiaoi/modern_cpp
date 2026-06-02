---
title: "C++17 并行算法（Parallel Algorithms）"
topic: unknown
feature: parallel-algorithms
standard: N/A
status_checked_at: 2026-06-02
---
# C++17 并行算法（Parallel Algorithms）

## 概述

C++17 为 `<algorithm>` 和 `<numeric>` 中的大部分算法引入了 **并行执行策略**，通过传入执行策略参数启用并行化。这是 C++ 标准首次将并行计算能力整合到标准库中。执行策略控制行为——从严格顺序到多线程并行乃至 SIMD 向量化。

## 语法

```cpp
#include <algorithm>
#include <numeric>
#include <execution>

std::sort(std::execution::par, vec.begin(), vec.end());
std::for_each(std::execution::par_unseq, vec.begin(), vec.end(), func);
auto sum = std::reduce(std::execution::par, vec.begin(), vec.end(), 0.0);
```

## 执行策略

```cpp
std::execution::seq        // 顺序执行（C++17）
std::execution::par        // 多线程并行，元素访问不重叠（C++17）
std::execution::par_unseq  // 并行 + SIMD 向量化（C++17）
std::execution::unseq      // 单线程 + SIMD 向量化（C++20）
```

| 策略 | 多线程 | 向量化 | 异常行为 |
|------|--------|--------|---------|
| `seq` | 否 | 否 | 正常传播 |
| `par` | 是 | 可能 | 可能 `terminate` |
| `par_unseq` | 是 | 是 | 可能 `terminate` |

## 支持并行的算法

### 排序与查找

```cpp
std::vector<int> data(1'000'000);
std::iota(data.begin(), data.end(), 0);
std::shuffle(data.begin(), data.end(), std::mt19937{42});

std::sort(std::execution::par, data.begin(), data.end());
auto it = std::find(std::execution::par, data.begin(), data.end(), 999'999);
bool found = std::binary_search(std::execution::par, data.begin(), data.end(), 500'000);
```

### 归约操作

```cpp
std::vector<double> values(10'000'000, 1.0);

double sum = std::reduce(std::execution::par, values.begin(), values.end(), 0.0);

double dot = std::transform_reduce(
    std::execution::par, a.begin(), a.end(), b.begin(), 0.0);

std::vector<int> prefix(n);
std::exclusive_scan(std::execution::par, data.begin(), data.end(), prefix.begin(), 0);
```

### 遍历与变换

```cpp
std::for_each(std::execution::par, items.begin(), items.end(),
    [](auto& item) { item.process(); });  // 回调必须线程安全

std::vector<double> output(input.size());
std::transform(std::execution::par, input.begin(), input.end(), output.begin(),
    [](double x) { return std::sin(x) * std::cos(x); });

auto n = std::count_if(std::execution::par, data.begin(), data.end(),
    [](int x) { return x % 2 == 0; });

bool has_neg = std::any_of(std::execution::par, v.begin(), v.end(),
    [](int x) { return x < 0; });
```

## 实现状态

| 编译器 | 后端 | 状态 |
|--------|------|------|
| GCC 9+ | TBB 或 `std::thread` | 完整支持 |
| MSVC 2019+ | Concurrency Runtime | 完整支持 |
| Clang 13+ | libc++ | 部分支持 |

```bash
# GCC 需要 TBB
sudo apt install libtbb-dev
g++ -std=c++17 -ltbb main.cpp
# 没有 TBB 时 par 策略静默回退到串行——看起来正常但无加速
```

## `reduce` vs `accumulate`

```cpp
std::vector<int> v = {1, 2, 3, 4, 5};

int s1 = std::accumulate(v.begin(), v.end(), 0);     // 严格从左到右
int s2 = std::reduce(std::execution::par, v.begin(), v.end(), 0); // 任意顺序

// 浮点数注意：并行改变求值顺序，结果可能不同
std::vector<double> fv = {1e10, 1.0, -1e10, 1.0};
// accumulate: 2.0, reduce: 可能 0.0
```

## 异常与安全要求

```cpp
// 并行算法中不应抛异常
// par / par_unseq 下未捕获异常可能导致 std::terminate

// 错误：数据竞争
int counter = 0;
std::for_each(std::execution::par, v.begin(), v.end(), [&](int) {
    counter++;  // 未定义行为
});

// 正确：使用原子或 reduce
std::atomic<int> counter{0};
std::for_each(std::execution::par, v.begin(), v.end(), [&](int) { counter++; });
```

迭代器要求：并行算法需要随机访问迭代器。`vector`/`deque`/`array` 支持；`list`/`set`/`map` 不支持。

## 实际应用：并行数据管道

```cpp
void process_dataset(std::vector<Record>& records) {
    std::sort(std::execution::par, records.begin(), records.end(),
        [](const auto& a, const auto& b) { return a.key < b.key; });

    std::vector<Result> results(records.size());
    std::transform(std::execution::par, records.begin(), records.end(),
        results.begin(), [](const Record& r) { return compute(r); });

    double total = std::reduce(std::execution::par,
        results.begin(), results.end(), 0.0,
        [](double acc, const Result& r) { return acc + r.value; });
}
```

## 最佳实践

1. **数据量小时不用并行**：少于 10,000 元素串行通常更快，需基准测试确认。
2. **`reduce` 替代 `accumulate` 做并行归约**。
3. **回调保持无状态和 `noexcept`**：避免共享状态和异常。
4. **浮点归约注意精度差异**：并行 `reduce` 改变求值顺序。
5. **优先 `par_unseq`**：给编译器更多优化自由度。
6. **不要假设并行一定更快**：对每个场景做基准测试。

## 常见陷阱

- **数据竞争**：修改共享变量是最常见的错误。
- **异常导致 terminate**：`par`/`par_unseq` 下抛异常可能终止程序。
- **链表/关联容器不支持**：并行算法要求随机访问迭代器。
- **TBB 未链接时静默回退**：GCC 没有 TBB，`par` 回退到串行，无警告。
- **`par_unseq` 要求纯函数式回调**：否则行为未定义。
- **浮点非结合性**：`(a+b)+c ≠ a+(b+c)` 对浮点成立，并行结果可能不同。
