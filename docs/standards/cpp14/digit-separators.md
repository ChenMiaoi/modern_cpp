---
title: "C++14 数字分隔符 (Digit Separators)"
topic: unknown
feature: digit-separators
standard: N/A
status_checked_at: 2026-06-02
---
# C++14 数字分隔符 (Digit Separators)

## 概述

C++14 引入单引号 `'` 作为数字字面量的分隔符，允许在数字中插入分组标记以提高可读性。分隔符不影响数值本身，编译器会完全忽略它们。这解决了 C++11 中大数字可读性差的问题。

## 语法

```cpp
整数类型：  1'000'000
浮点类型：  3.141'592'653
十六进制：  0xFF'FF'FF'FF
二进制：    0b1111'0000'1010'0101
八进制：    0'777'777
自定义分组：100'00  // 合法，但不推荐无规律分组
```

规则：
- 分隔符 `'` 可以出现在数字的任意两个数字之间（含 `0x`/`0b`/`0` 前缀之后）。
- 分隔符不能出现在数字的开头或末尾。
- 分隔符不能相邻（`1''000` 非法）。
- 分隔符不影响数值——`1'000` 等于 `1000`。

## 代码示例

### 提高大数可读性

```cpp
#include <cstdint>

// 难以审查的版本
constexpr uint64_t max_memory_b = 17179869184;

// 使用千位分隔符
constexpr uint64_t max_memory_b2 = 17'179'869'184;  // ~16 GB

// 系统常量
constexpr int us_population_approx = 331'000'000;
constexpr double speed_of_light_ms = 299'792'458.0;  // m/s
constexpr uint64_t avogadro = 602'214'076'000'000'000'000'000ULL;
```

### 十六进制与二进制

```cpp
#include <cstdint>

// IPv4 地址的 32 位值
constexpr uint32_t localhost = 0x7F'00'00'01;  // 127.0.0.1

// 颜色值 (RGBA)
constexpr uint32_t color_red   = 0xFF'00'00'FF;
constexpr uint32_t color_green = 0x00'FF'00'FF;
constexpr uint32_t color_alpha_50 = 0xFF'FF'FF'80;

// 二进制标志
constexpr uint16_t flags = 0b0000'0001'1010'0101;

// 位域掩码
constexpr uint64_t mask = 0xFFFF'FFFF'0000'0000;
```

### 浮点数

```cpp
// pi — 按每三位分组
constexpr double pi = 3.141'592'653'589'793;

// 科学计数法
constexpr double planck = 6.626'070'15e-34;   // J·s
constexpr double boltzmann = 1.380'649e-23;    // J/K

// 货币金额（用千位分隔）
constexpr long price_cents = 1'299'99;  // $1,299.99（分为单位）

// 精度分组（按逻辑意义）
constexpr double fine_structure = 0.007'297'352'5693;
```

### 八进制

```cpp
// Unix 文件权限
constexpr int perm_all = 0777;
constexpr int perm_owner_only = 0700;
constexpr int perm_read_write = 0644;  // rw-r--r--

// 使用分隔符的八进制（较少见）
constexpr int perm_grouped = 0'7'5'5;  // rwxr-xr-x
```

### 实际场景：查找表

```cpp
#include <cstdint>

// CRC-32 查找表片段
constexpr uint32_t crc32_table[] = {
    0x0000'0000, 0x7707'3096, 0xEE0E'612C, 0x9909'51BA,
    0x076D'C419, 0x706A'F48F, 0xE963'A535, 0x9E64'95A3,
    // ...
};

// 魔数常量
constexpr uint64_t fnv_offset = 0xCBF2'9CE4'8422'2325;
constexpr uint64_t fnv_prime  = 0x0000'0100'0000'01B3;
```

## 最佳实践

1. **按语义分组**：千位（`1'000'000`）或字节（`0xFF'AB'CD'EF`），选择与业务语义一致的分组方式，而非随意放置。
2. **二进制字面量推荐使用分隔符**：每 4 或 8 位一组，`0b1111'0000` 远比 `0b11110000` 易读。
3. **十六进制按字节分组**：`0x7F'00'00'01` 比 `0x7F000001` 更容易与网络地址对应。
4. **不要过度使用**：`1'0` 比 `10` 更难读。只在数字超过 5-6 位时使用分隔符。
5. **浮点数按精度语义分组**：科学常数按传统精度分组（3 位），货币金额按千位分组。
6. **不支持在字符串或宏中使用**：分隔符只适用于数字字面量，不能用于 `"1'000"` 字符串或 `#define` 的替换文本中的非字面量部分。
