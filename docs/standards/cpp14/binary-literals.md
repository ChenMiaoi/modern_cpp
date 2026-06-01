# C++14 二进制字面量 (Binary Literals)

## 概述

C++14 引入了 `0b` 前缀的二进制字面量，允许直接用二进制形式书写整数常量。这在位操作密集的代码（硬件寄存器配置、协议解析、权限标志）中极大提高了可读性。二进制字面量可以与 `constexpr` 结合使用，实现编译期位运算。

## 语法

```cpp
0b二进制数字      // int 类型（至少 32 位）
0B二进制数字      // 大写形式，等价
0b二进制数字u     // unsigned int
0b二进制数字l     // long
0b二进制数字ll    // long long
0b二进制数字ull   // unsigned long long
```

合法数字为 `0` 和 `1`。

## 代码示例

### 基本用法

```cpp
#include <iostream>

int main() {
    int a = 0b1010;       // 10
    int b = 0B1111'0000;  // 240 — 使用 C++14 分隔符
    unsigned c = 0b11111111u; // 255

    std::cout << a << ' ' << b << ' ' << c << '\n';
    // 输出: 10 240 255
}
```

### 位标志定义

```cpp
// 权限系统 — 每一位代表一种权限
constexpr int PERM_READ    = 0b0001;  // bit 0
constexpr int PERM_WRITE   = 0b0010;  // bit 1
constexpr int PERM_EXEC    = 0b0100;  // bit 2
constexpr int PERM_ADMIN   = 0b1000;  // bit 3

constexpr int PERM_ALL     = 0b1111;
constexpr int PERM_NONE    = 0b0000;

// 检查权限
constexpr bool has_permission(int user_perm, int check) {
    return (user_perm & check) == check;
}

// 编译期验证
static_assert(has_permission(PERM_ALL, PERM_READ));
static_assert(has_permission(PERM_ALL, PERM_WRITE));
static_assert(!has_permission(PERM_READ, PERM_WRITE));
```

### 硬件寄存器配置

```cpp
#include <cstdint>

// UART 控制寄存器位域
constexpr uint8_t UART_ENABLE      = 0b0000'0001;  // bit 0: 使能
constexpr uint8_t UART_TX_ENABLE   = 0b0000'0010;  // bit 1: 发送使能
constexpr uint8_t UART_RX_ENABLE   = 0b0000'0100;  // bit 2: 接收使能
constexpr uint8_t UART_PARITY_EVEN = 0b0000'1000;  // bit 3: 偶校验
constexpr uint8_t UART_STOP_BITS_2 = 0b0001'0000;  // bit 4: 2 个停止位
constexpr uint8_t UART_DATA_8BIT   = 0b0110'0000;  // bit 6-5: 8 位数据

// 组合配置
constexpr uint8_t UART_CONFIG_8N1 =
    UART_ENABLE | UART_TX_ENABLE | UART_RX_ENABLE | UART_DATA_8BIT;
// 0b0110'0111 = 0x67

// 配置函数
constexpr uint8_t configure_uart(bool parity_even, bool stop_2) {
    uint8_t cfg = UART_ENABLE | UART_TX_ENABLE | UART_RX_ENABLE | UART_DATA_8BIT;
    if (parity_even) cfg |= UART_PARITY_EVEN;
    if (stop_2)      cfg |= UART_STOP_BITS_2;
    return cfg;
}

static_assert(configure_uart(true, false) == 0b0000'1111);
```

### 网络子网掩码

```cpp
#include <cstdint>

constexpr uint32_t MASK_24 = 0b11111111'11111111'11111111'00000000; // /24
constexpr uint32_t MASK_16 = 0b11111111'11111111'00000000'00000000; // /16
constexpr uint32_t MASK_8  = 0b11111111'00000000'00000000'00000000; // /8

constexpr bool in_subnet(uint32_t ip, uint32_t subnet, uint32_t mask) {
    return (ip & mask) == (subnet & mask);
}
```

### 位操作工具

```cpp
#include <iostream>
#include <bitset>

// 模板化的位计数（编译期）
template <typename T>
constexpr int popcount(T value) {
    int count = 0;
    while (value) {
        count += (value & 1);
        value >>= 1;
    }
    return count;
}

static_assert(popcount(0b1010) == 2);
static_assert(popcount(0b1111'1111) == 8);

// 打印二进制表示
void print_binary(unsigned char byte) {
    std::cout << std::bitset<8>(byte) << '\n';
}

int main() {
    print_binary(0b1010'0101);  // 输出: 10100101
}
```

## 最佳实践

1. **用二进制字面量替代十六进制表示位域**：`0b0010` 比 `0x2` 在位标志上下文中更直观。
2. **搭配数字分隔符提高可读性**：`0b1111'0000'1010'0101` 按字节分组，比无分隔版本更容易审查。
3. **位运算结果建议 `constexpr`**：编译期计算可消除运行时开销，并在编译期捕获错误。
4. **注意类型后缀**：二进制字面量默认为 `int`，对大值需加 `u`、`ull` 等后缀避免符号问题。
5. **不要滥用二进制字面量**：对于 `0`、`1`、`255` 等简单值，十进制或十六进制更常见，不需要改成二进制。
6. **配合 `std::bitset` 使用**：运行时打印调试信息时，`std::bitset<8>(value)` 可直接输出二进制字符串。
