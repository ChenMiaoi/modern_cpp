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
