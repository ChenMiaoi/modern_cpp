# std::array 固定大小数组容器

## 概述

`std::array<T, N>` 是 C++11 引入的固定大小数组容器，对原生 C 数组的零开销封装。与 `std::vector` 不同，其大小在编译期确定，不会在堆上分配内存；与 C 数组不同，它拥有完整的容器接口、支持拷贝赋值、并携带大小信息。

`std::array` 是"知道自身大小的数组"，适合替代所有需要固定大小序列的场景。

## API 概览

| 成员函数 | 说明 |
|----------|------|
| `at(i)` | 带边界检查，越界抛 `std::out_of_range` |
| `operator[]` | 无检查的元素访问 |
| `front()` / `back()` | 首/末元素引用 |
| `data()` | 底层数组指针，兼容 C API |
| `fill(v)` | 用值 `v` 填充所有元素 |
| `size()` | 元素数量（`constexpr`） |
| `empty()` | 是否为空（`N == 0` 时为 true） |
| `begin()` / `end()` | 正向迭代器 |
| `rbegin()` / `rend()` | 反向迭代器 |

## 基本使用与初始化

```cpp
#include <array>

// 聚合初始化（与 C 数组语法一致）
std::array<int, 5> a1 = {1, 2, 3, 4, 5};

// C++14 起支持更简洁的初始化
std::array<int, 5> a2{10, 20, 30, 40, 50};

// 部分初始化，剩余元素值初始化为 0
std::array<int, 5> a3 = {1, 2};  // {1, 2, 0, 0, 0}

// 全零初始化
std::array<int, 5> a4{};  // {0, 0, 0, 0, 0}
```

## 与 C 数组和 std::vector 的对比

```cpp
// C 数组：传参退化为指针，丢失大小信息，不可拷贝赋值
int c_arr[5] = {1, 2, 3, 4, 5};

// std::array：值语义，可拷贝，大小是类型的一部分
std::array<int, 5> arr = {1, 2, 3, 4, 5};
std::array<int, 5> arr2 = arr;  // 合法的完整拷贝

// std::vector：动态大小，堆分配，适合运行期确定大小
std::vector<int> vec = {1, 2, 3, 4, 5};
```

**选择指南：**
- 大小在编译期已知且固定 → `std::array`
- 大小在运行期确定或需要动态调整 → `std::vector`
- 需要与 C API 交互且大小固定 → `std::array`（`data()` 返回裸指针）

## constexpr 大小与编译期特性

`std::array` 的 `size()` 返回 `constexpr` 值，可用于模板参数和编译期计算：

```cpp
constexpr std::array<int, 4> ca = {1, 2, 3, 4};

static_assert(ca.size() == 4, "size must be 4");  // 编译期检查

template <typename T, std::size_t N>
constexpr std::size_t array_size(const std::array<T, N>&) {
    return N;  // 编译期获取大小
}

static_assert(array_size(ca) == 4, "");
```

## 元素访问

```cpp
std::array<int, 5> arr = {10, 20, 30, 40, 50};

int val = arr[2];    // 30，无检查，越界是未定义行为

try {
    int v = arr.at(10);  // 越界抛 std::out_of_range
} catch (const std::out_of_range&) { /* 处理 */ }

int first = arr.front();  // 10
int last  = arr.back();   // 50
int* ptr  = arr.data();   // 底层数组指针，可用于 C API
```

## fill、迭代与 STL 算法

`std::array` 是完整的 STL 容器，可与所有标准算法配合使用：

```cpp
#include <algorithm>
#include <numeric>

std::array<int, 5> arr{};
arr.fill(42);  // {42, 42, 42, 42, 42}

// 范围 for
for (const auto& elem : arr) { std::cout << elem << ' '; }

// 排序、查找、累加
std::array<int, 5> data = {5, 3, 1, 4, 2};
std::sort(data.begin(), data.end());       // {1, 2, 3, 4, 5}

auto it = std::find(data.begin(), data.end(), 3);
auto idx = std::distance(data.begin(), it);  // 2

int sum = std::accumulate(data.begin(), data.end(), 0);  // 15

// minmax_element
auto [min_it, max_it] = std::minmax_element(data.begin(), data.end());

// count_if
auto cnt = std::count_if(data.begin(), data.end(),
    [](int x) { return x > 3; });  // 2

// transform
std::array<int, 5> squared;
std::transform(data.begin(), data.end(), squared.begin(),
    [](int x) { return x * x; });
```

## 结构化绑定（C++17）

```cpp
// C++17 允许对 std::array 使用结构化绑定
std::array<int, 3> point = {10, 20, 30};
auto [x, y, z] = point;  // x=10, y=20, z=30
```

## 多维固定数组

```cpp
// 二维数组：array of array
std::array<std::array<int, 3>, 2> matrix = {{{1, 2, 3}, {4, 5, 6}}};
int val = matrix[1][2];  // 6

for (const auto& row : matrix) {
    for (int elem : row) { std::cout << elem << ' '; }
    std::cout << '\n';
}
```

## 最佳实践

1. **优先 `std::array` 而非 C 数组**：类型安全，可拷贝，不退化为指针。
2. **用 `at()` 做调试检查，`operator[]` 做生产路径**。
3. **利用 `data()` 与 C API 交互**：返回连续内存指针。
4. **多维场景嵌套 `std::array`**：`std::array<std::array<T, C>, R>` 优于 `T[R][C]`。
5. **空 `std::array<T, 0>` 是合法类型**：`size()` 为 0，可用于模板边界情况。

## 常见陷阱

- **不可动态调整大小**：需要 `push_back` 则应使用 `std::vector`。
- **`at()` 有运行时开销**：每次访问都检查边界，热路径中可能成为瓶颈。
- **大小是类型的一部分**：`std::array<int, 3>` 和 `std::array<int, 4>` 是不同类型。
- **初始化语法陷阱**：`std::array<int, 3> a = {1};` 只初始化第一个元素为 1，其余为 0。花括号嵌套 `{{}}` 在 C++11 中有时是必需的。
- **没有 `push_back` / `insert` / `erase`**：固定大小容器不支持动态修改大小。
