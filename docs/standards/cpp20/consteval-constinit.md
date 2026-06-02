---
title: "`consteval` 与 `constinit`"
topic: unknown
feature: consteval-constinit
standard: N/A
status_checked_at: 2026-06-02
---
# `consteval` 与 `constinit`

## 概述

C++20 新增两个关键字：
- **`consteval`**：立即函数，**必须**在编译期求值，否则编译失败。
- **`constinit`**：强制常量初始化，防止 SIOF，但变量本身**不是** `const`。

## `consteval`：立即函数

```cpp
consteval int square(int n) { return n * n; }

int main() {
    constexpr int a = square(5);   // OK
    int x = 10;
    // int b = square(x);          // 错误：x 非常量

    int arr[square(3)];             // arr[9]
    static_assert(square(4) == 16);
}
```

### `consteval` vs `constexpr`

```cpp
constexpr int f1(int n) { return n * 2; }   // 编译期或运行期
consteval int f2(int n) { return n * 2; }   // 必须编译期

int main() {
    int rt = 42;
    constexpr int a = f1(5);     // OK
    int b = f1(rt);              // OK：运行时
    constexpr int c = f2(5);     // OK
    // int d = f2(rt);           // 错误
}
```

| 特性 | `constexpr` | `consteval` |
|------|-------------|-------------|
| 编译期必须求值 | 否 | **是** |
| 运行期可调用 | 是 | 否 |

## `constinit`：常量初始化

`constinit` 保证变量在**编译期初始化**，避免 SIOF，但不要求不可修改。

### SIOF 问题

```cpp
// a.cpp
int compute() { return 42; }
int g_a = compute();  // 运行期初始化，顺序不确定

// b.cpp
extern int g_a;
int g_b = g_a + 1;   // g_a 可能尚未初始化 → UB
```

### `constinit` 修复

```cpp
constexpr int compute() { return 42; }
constinit int g_value = compute();  // 强制编译期初始化

void update() {
    g_value = 100;  // OK：constinit 不限制可变性
}
```

### `constinit` vs `constexpr` 变量

```cpp
constexpr int ci = 42;     // const + 常量初始化，不可修改
constinit int cni = 42;    // 常量初始化，可修改

void f() {
    // ci = 10;            // 错误：ci 是 const
    cni = 10;              // OK
}
```

## 三者对比

| 特性 | `constexpr` | `consteval` | `constinit` |
|------|-------------|-------------|-------------|
| 适用对象 | 变量 / 函数 | 仅函数 | 仅变量 |
| 必须编译期求值 | 否 | 是 | 是（初始化时） |
| 变量是否 const | 是 | — | 否 |
| 防止 SIOF | 间接 | — | **直接** |
| 首次引入 | C++11 | C++20 | C++20 |

## 实际用例

### 编译期查找表

```cpp
consteval std::array<uint8_t, 256> make_crc_table() {
    std::array<uint8_t, 256> table{};
    for (int i = 0; i < 256; ++i) {
        uint8_t crc = static_cast<uint8_t>(i);
        for (int j = 0; j < 8; ++j)
            crc = (crc & 1) ? (crc >> 1) ^ 0x8C : (crc >> 1);
        table[i] = crc;
    }
    return table;
}

constinit auto crc_table = make_crc_table();
```

### 编译期阶乘

```cpp
template <int N>
consteval int factorial() {
    static_assert(N >= 0);
    int r = 1;
    for (int i = 2; i <= N; ++i) r *= i;
    return r;
}

static_assert(factorial<5>() == 120);
```

### 单例中的 `constinit`

```cpp
struct Config { int timeout; int max_retries; };
constinit Config g_config = {30, 3};

void reconfigure(int t, int r) {
    g_config = {t, r};  // OK：运行期可修改
}
```

## `consteval` 的限制

```cpp
constexpr int helper(int x) { return x + 1; }

consteval int caller(int x) {
    // return helper(x);   // 错误：helper 可能运行期求值
    return helper(5);      // OK：参数为常量
}

// 不能取立即函数地址
// auto fp = &square;      // 错误

// 虚函数不能是 consteval
// struct S { virtual consteval int f(); };  // 错误
```

## 总结

- **`consteval`** 用于必须编译期执行的函数，适合校验和查找表。
- **`constinit`** 强制编译期初始化但不附加 `const`，适合全局变量防 SIOF。
- **`constexpr`** 保持最大灵活性，编译期和运行期均可用。
- 三者互补：`consteval` 生成 → `constinit` 存储 → `constexpr` 运行时复用。
