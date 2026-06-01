# Boost 算法与数学

## Multiprecision：任意精度算术

核心设计：**frontend/backend 分离**。Frontend（`number<Backend>`）提供统一的运算符接口，Backend 决定存储和算法。

```cpp
template<typename Backend>
class number {
    Backend backend_;
public:
    number& operator*=(number const& other) {
        backend_.multiply(backend_, other.backend_);
        return *this;
    }
};
```

| Backend | 存储 | 精度 | 适用场景 |
|---------|------|------|---------|
| `cpp_int` | 动态 `vector<limb_type>` | 运行时任意精度 | 通用大整数 |
| `int128_backend` | 栈上 128 位 | 固定 128 位 | 超过 uint64_t |
| `gmp_backend` | GMP 的 `mpz_t` | 任意精度 | 极致性能 |

### 小值优化与乘法策略

```cpp
void multiply(cpp_int_backend& result, ...) {
    if (a.size() <= 2 && b.size() <= 2) {
        // 小值：单条 mul 指令
    } else if (a.size() < 30 || b.size() < 30) {
        // 中等：朴素 O(n*m) 乘法
    } else {
        // 大数：Karatsuba O(n^1.585)
        // 更大：Toom-Cook-3 或 FFT O(n log n)
    }
}
```

### 表达式模板

`a = b * c + d * e` 使用表达式模板避免 3 个临时对象——整个表达式树在赋值时一次性计算到 `a` 的 backend 中。

---

## Math：数学函数

Boost.Math 提供标准数学函数的扩展：

- **特殊函数**：贝塞尔函数、椭圆积分、Gamma/Beta 函数
- **统计分布**：正态分布、泊松分布等的 PDF/CDF/分位数
- **数值积分**：自适应积分、高斯求积
- **常数**：编译期数学常数（π、e、√2 等）

---

## Geometry：计算几何

```cpp
namespace bg = boost::geometry;
using Point = bg::model::d2::point_xy<double>;
using Polygon = bg::model::polygon<Point>;

Polygon poly;
bg::read_wkt("POLYGON((0 0, 0 5, 5 5, 5 0, 0 0))", poly);
std::cout << bg::area(poly);             // 25
std::cout << bg::within(Point(2.5, 2.5), poly);  // 1
```

策略模式允许切换实现：距离计算可选欧氏/曼哈顿距离，点位置测试可选不同精度策略。

---

## Range：范围算法

Boost.Range 是 C++20 Ranges 的前身，提供 pipe-based 范围操作：

```cpp
auto result = data | boost::adaptors::filtered(pred)
                   | boost::adaptors::transformed(fn);
```

---

## 其他算法库

| 库 | 说明 |
|---|------|
| **Algorithm** | 字符串算法（trim、split、join、replace） |
| **CRC** | CRC-16/32/64 校验和 |
| **Conversion** | 类型转换（`lexical_cast`、`numeric_cast`） |
| **Endian** | 字节序转换（`big_to_native`、`little_to_native`） |
