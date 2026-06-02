# fmt 高级特性

> 源码路径：`references/impl/fmt/include/fmt/chrono.h`, `ranges.h`, `compile.h`

## 自定义类型

```cpp
struct Point {
  double x, y;
};

template <> struct fmt::formatter<Point> : formatter<double> {
  auto format(const Point& p, format_context& ctx) const {
    return format_to(ctx.out(), "({}, {})", p.x, p.y);
  }
};

fmt::format("{}", Point{1.5, 2.5});  // "(1.5, 2.5)"
```

自定义类型通过 `custom_value<Context>` 存储——`value` 构造函数检测到用户特化了 `formatter<T>`，进入 `custom_tag` 路径，将对象指针和格式化函数指针打包到 16 字节 union 中。

## 编译期格式化

```cpp
// fmt::format_string 在编译期验证格式串
constexpr auto s = fmt::format<int, double>("{} {}", 42, 3.14);
```

## Chrono 格式化

```cpp
auto now = std::chrono::system_clock::now();
fmt::format("{:%Y-%m-%d %H:%M}", now);  // "2024-01-15 14:30"

std::chrono::seconds dur(3661);
fmt::format("{:%H:%M:%S}", dur);  // "01:01:01"
```

## Ranges 格式化

```cpp
std::vector<int> v = {1, 2, 3};
fmt::format("{}", v);  // "[1, 2, 3]"
fmt::format("{::#x}", v);  // "[0x1, 0x2, 0x3]"
```

## 浮点格式化：Dragonbox

fmt 使用 Dragonbox 算法进行浮点到字符串的转换，比 `std::to_chars` 快 2-5 倍：

- **Dragonbox**：基于 IEEE 754 浮点数的数学特性，直接计算最短表示
- **Grisu-Exact**：备选算法，保证精确舍入
- **Ryu**：另一种快速算法，fmt 早期使用

## 用户 API

本文覆盖的用户入口包括 `fmt::format`、自定义 `formatter<T>`、chrono/ranges 格式化与编译期格式串检查；现有正文已经直接展开这些高级特性。

## 标准语义

待补：补上 fmt 相对 `std::format` / `std::print` 的兼容语义、扩展点与不兼容细节。

## 对象布局

待补：这里主要关心参数类型擦除后的对象布局，尤其是自定义 formatter、`custom_value` 与编译期格式串对象的存储方式。

## 核心源码路径

本文开头已给出 `chrono.h`、`ranges.h`、`compile.h`；后续补 `format.h` / `base.h` 中与这些特性衔接的入口。

## 核心类 / 函数

待补：统一整理 `formatter<T>`、`custom_value<Context>`、`format_string`、chrono formatter 与 range formatter 的关键实现节点。

## 关键算法

待补：补充自定义 formatter 分发、编译期格式串检查、chrono token 解析与 Dragonbox 浮点转换在这篇里的串联关系。

## ABI 约束

待补：说明 fmt 主要依赖头文件模板实例化与内联代码，不提供标准库式长期 ABI 约束。

## 异常安全

待补：补充分配失败、格式串不合法、用户 formatter 抛异常时的传播与回滚边界。

## iterator / reference invalidation

待补：本文不是容器分析，但需要补 `format_arg_store`、ranges 适配器与内部缓冲扩容后 view/reference 的有效期说明。

## 性能模型

待补：补上编译期格式串检查、自定义 formatter 间接层、Dragonbox 浮点路径与 ranges 遍历开销的性能模型。

## libstdc++ vs libc++ vs MSVC

待补：这里主要与各家 `std::format` 实现对照，说明 fmt 在功能覆盖、编译期检查与性能上的差异。

## 最小复现代码

```cpp
#include <chrono>
#include <fmt/chrono.h>
#include <fmt/format.h>

struct Point {
  int x;
  int y;
};

template <>
struct fmt::formatter<Point> : fmt::formatter<int> {
  auto format(const Point& p, fmt::format_context& ctx) const {
    return fmt::format_to(ctx.out(), "({}, {})", p.x, p.y);
  }
};

int main() {
  return static_cast<int>(fmt::format("{}", Point{1, 2}).size());
}
```

## 编译 / 反汇编 / benchmark 证据

待补：补上自定义 formatter、chrono/ranges 格式化与 Dragonbox 路径的 benchmark/反汇编证据。

## cpplings 练习入口

- [`format1` — std::format 格式化](../../../exercises/cpp20/format1.cpp)
- [`print23` — std::print / std::println 格式化输出](../../../exercises/cpp23/print23.cpp)
- [`chrono1` — chrono 时间库](../../../exercises/cpp11-std/chrono1.cpp)
- [`ranges1` — Ranges 基础`](../../../exercises/cpp20/ranges1.cpp)
