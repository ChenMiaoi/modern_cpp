# std::random 随机数库

## 概述

C++11 引入了 `<random>` 头文件，将随机数生成拆分为**引擎**（产生原始随机比特）和**分布**（将比特映射到特定分布）两个独立组件。这种设计比 C 风格的 `rand()` 更灵活、更可控、更高质量。

`rand()` 的问题：全局状态导致线程不安全、实现定义的质量与范围、取模偏差。C++11 的 `<random>` 彻底解决了这些问题。

## API 概览

| 组件 | 说明 |
|------|------|
| `std::mt19937` | 32 位梅森旋转引擎（周期 2^19937-1） |
| `std::mt19937_64` | 64 位梅森旋转引擎 |
| `std::random_device` | 硬件/操作系统熵源 |
| `std::uniform_int_distribution<T>` | 均匀整数分布 |
| `std::uniform_real_distribution<T>` | 均匀实数分布 |
| `std::normal_distribution<T>` | 正态分布 |
| `std::bernoulli_distribution` | 伯努利分布（概率布尔值） |

## 引擎与分布的分离

```cpp
#include <random>
#include <iostream>

int main() {
    std::mt19937 engine(42);                       // 固定种子，可复现
    std::uniform_int_distribution<int> dist(1, 6); // 映射到 [1, 6]
    for (int i = 0; i < 10; ++i) {
        std::cout << dist(engine) << ' ';  // 每次运行输出相同
    }
}
```

## 为什么 rand() 是坏的

```cpp
#include <cstdlib>

// 1. 范围太小：RAND_MAX 通常只有 32767
// 2. 取模偏差：rand() % 6 在 RAND_MAX 非 6 倍数时不均匀
int biased = std::rand() % 6;
// 3. 全局状态，多线程调用是数据竞争
// 4. 某些实现的低比特周期极短

// C++11 的正确做法：
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<int> dist(0, 5);  // 精确均匀
```

## std::mt19937：默认推荐引擎

```cpp
std::mt19937 gen32;      // 32 位输出，适合绝大多数场景
std::mt19937_64 gen64;   // 64 位输出，适合大范围随机数

static_assert(std::mt19937::min() == 0);
static_assert(std::mt19937::max() == 4294967295);  // 2^32 - 1
```

## 种子最佳实践

```cpp
// 方式 1：std::random_device 作为种子（推荐）
std::random_device rd;
std::mt19937 gen(rd());

// 方式 2：seed_seq 混合多个熵值（更健壮）
std::seed_seq seed{rd(), rd(), rd(), rd()};
std::mt19937 gen2(seed);

// 方式 3：固定种子用于调试和测试
std::mt19937 gen3(42);  // 每次运行产生相同序列
```

> **`std::random_device` 注意：** 标准不保证它是非确定性的。MinGW 的旧版本中它是确定性的。生产环境中应验证你的工具链。

## 常用分布

```cpp
std::mt19937 gen(std::random_device{}());

// 均匀整数分布：[1, 6] 闭区间
std::uniform_int_distribution<int> dice(1, 6);
int roll = dice(gen);

// 均匀实数分布：[0.0, 1.0) 左闭右开
std::uniform_real_distribution<double> unit(0.0, 1.0);
double p = unit(gen);  // 永远不会生成 1.0

// 正态分布：均值 100，标准差 15
std::normal_distribution<double> iq(100.0, 15.0);
double score = iq(gen);

// 伯努利分布：70% 概率返回 true
std::bernoulli_distribution coin(0.7);
if (coin(gen)) { /* 70% 的概率执行 */ }

// 离散分布：按权重选择
std::discrete_distribution<int> weighted({60, 20, 15, 5});
int category = weighted(gen);  // 返回 0, 1, 2, 3
```

## 线程局部生成器

全局引擎在多线程环境下存在竞争。每个线程应拥有独立的引擎：

```cpp
#include <thread>
#include <vector>

thread_local std::mt19937 tl_gen(std::random_device{}());

int generate_random() {
    std::uniform_int_distribution<int> dist(1, 100);
    return dist(tl_gen);  // 线程安全，无竞争
}

int main() {
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) threads.emplace_back([] {
        for (int j = 0; j < 1000; ++j) generate_random();
    });
    for (auto& t : threads) t.join();
}
```

## 生成随机容器数据

```cpp
#include <vector>
#include <algorithm>
#include <numeric>

std::mt19937 gen(std::random_device{}());

// 生成随机向量
std::vector<int> v(100);
std::uniform_int_distribution<int> dist(1, 1000);
std::generate(v.begin(), v.end(), [&] { return dist(gen); });

// 随机打乱（Fisher-Yates 洗牌）
std::vector<int> deck(52);
std::iota(deck.begin(), deck.end(), 0);
std::shuffle(deck.begin(), deck.end(), gen);
```

## constexpr 随机（编译期）

C++ 标准库的随机引擎不支持 `constexpr`。编译期可使用简单的 LCG：

```cpp
constexpr unsigned lcg(unsigned seed) {
    return seed * 1664525u + 1013904223u;  // Numerical Recipes 参数
}

constexpr unsigned r1 = lcg(42);
constexpr unsigned r2 = lcg(r1);  // 编译期计算完成
// 不是密码学安全的，仅适用于模板元编程等编译期场景
```

## 最佳实践

1. **使用 `std::mt19937` 作为默认引擎**：质量足够好，速度足够快。
2. **用 `std::random_device` 播种**：除非你需要可复现的序列。
3. **引擎和分布分离使用**：不要退回到 `rand()`。
4. **每个线程使用独立引擎**：通过 `thread_local` 或参数传递。
5. **需要可复现结果时固定种子**：测试和仿真中的关键需求。
6. **用 `std::seed_seq` 增加种子熵**：当单个 32 位种子不够用时。

## 常见陷阱

- **取模偏差**：`gen() % n` 仍有偏差。**必须使用 `std::uniform_int_distribution`**。
- **`std::random_device` 可能是确定性的**：MinGW 的旧实现退化为伪随机。
- **分布不是线程安全的**：分布对象内部维护状态缓存，不要在线程间共享。
- **`uniform_real_distribution` 的区间是左闭右开**：`[a, b)`，永远不会生成 `b`。
- **引擎的 `seed()` 重置全部状态**：不要在生成过程中意外重置引擎。
- **不要用 `std::time(nullptr)` 做种子**：分辨率低，可预测，快速连续创建会得到相同种子。
