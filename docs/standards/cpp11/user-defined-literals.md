# 用户自定义字面量 (User-Defined Literals)

## 概述

C++11 引入了用户自定义字面量 (User-Defined Literals, UDL)，允许开发者为字面量后缀定义自定义运算符。这使得代码能够以直观的语法表达领域概念，例如 `42_km`、`3.14_rad`、`"hello"_sv` 等，在保持类型安全的同时大幅提升可读性。

UDL 的核心价值在于：将"值 + 单位/语义"的绑定从运行时提升到编译期，同时让类型转换发生在编译阶段而非运行时，减少样板代码。

## 基本语法

用户自定义字面量通过 `operator""_suffix` 定义，其中 `_suffix` 是字面量后缀名称：

```cpp
// 返回类型 operator"" _后缀名(参数类型)
long double operator"" _km(long double value) {
    return value * 1000.0;  // convert km to meters
}

auto distance = 5.0_km;  // distance == 5000.0
```

> **命名规则**：C++11 要求用户自定义后缀必须以下划线 `_` 开头（不带下划线的后缀保留给标准库使用）。C++11 标准中保留了若干无下划线前缀（如 `_i`、`_id`、`_if`），但实践中建议一律使用 `_` 前缀。

## 参数形式

`operator""` 支持以下几种参数形式，每种适用于不同的字面量输入：

### 1. 整数字面量参数

```cpp
// unsigned long long 形式：匹配整数字面量
unsigned long long operator"" _bin(unsigned long long value) {
    // value 已经是十进制数值，此处仅做传递
    return value;
}

auto flags = 1010_bin;  // flags == 10 (binary 1010)
```

编译器将字面量中的数字解析为 `unsigned long long` 传入。此形式无法真正做二进制解析（进位已在编译器侧完成），但可用于语义标记。

### 2. 浮点字面量参数

```cpp
long double operator"" _deg(long double degrees) {
    return degrees * 3.14159265358979323846L / 180.0L;
}

constexpr long double angle = 90.0_deg;  // π/2 radians
```

### 3. 字符字面量参数

```cpp
// char 形式：匹配单个字符字面量
char operator"" _rot13(char c) {
    if (c >= 'a' && c <= 'm') return c + 13;
    if (c >= 'n' && c <= 'z') return c - 13;
    if (c >= 'A' && c <= 'M') return c + 13;
    if (c >= 'N' && c <= 'Z') return c - 13;
    return c;
}

auto encrypted = 'H'_rot13;  // 'U'
```

### 4. 字符串字面量参数

```cpp
// const char* + size_t 形式：匹配字符串字面量
std::string operator"" _upper(const char* str, size_t len) {
    std::string result(str, len);
    for (auto& c : result)
        c = static_cast<char>(std::toupper(c));
    return result;
}

auto greeting = "hello"_upper;  // "HELLO"
```

### 5. 原始字符串/数字参数（模板形式）

```cpp
// 模板形式：匹配字面量的原始字符序列
template <char... Chars>
constexpr int operator"" _hex() {
    return hex_to_int<Chars...>::value;
}
```

这是最灵活但也最复杂的形式，字面量的每个字符作为模板参数传入，需要递归模板元编程来解析。

## constexpr 用户自定义字面量

将 UDL 声明为 `constexpr` 可以在编译期完成计算：

```cpp
struct Distance {
    long long meters;
};

constexpr Distance operator"" _km(unsigned long long v) {
    return Distance{static_cast<long long>(v) * 1000};
}

constexpr Distance operator"" _m(unsigned long long v) {
    return Distance{static_cast<long long>(v)};
}

constexpr Distance operator"" _cm(unsigned long long v) {
    return Distance{static_cast<long long>(v) / 100};
}

// 编译期计算
constexpr auto marathon = 42_km + 195_m;
static_assert(marathon.meters == 42195, "marathon distance");
```

`constexpr` UDL 将所有计算下推到编译期，运行时零开销。

## 实际应用案例

### 案例 1：时间单位

```cpp
using namespace std::chrono_literals;

// 标准库已提供：operator""s, operator""ms, operator""us, operator""ns
auto timeout = 500ms;
auto duration = 2s + 300ms;
std::this_thread::sleep_for(1s);
```

### 案例 2：二进制字面量辅助

```cpp
namespace bin_literals {

// 编译期解析二进制字符串
template <char... Chars>
struct bin_parser;

template <char High, char... Rest>
struct bin_parser<High, Rest...> {
    static_assert(High == '0' || High == '1', "Only 0 and 1 allowed");
    static constexpr unsigned long long value =
        ((High - '0') << (1 + sizeof...(Rest) - 1)) +
        bin_parser<Rest...>::value;  // Note: simplified; real impl needs proper shift
};

template <>
struct bin_parser<> {
    static constexpr unsigned long long value = 0;
};

template <char... Chars>
constexpr unsigned long long operator"" _b() {
    return bin_parser<Chars...>::value;
}

} // namespace bin_literals

// Usage: using namespace bin_literals;
// constexpr auto mask = 1010_b;
```

### 案例 3：字符串视图字面量（标准库 `sv`）

```cpp
using namespace std::string_literals;
using namespace std::string_view_literals;

auto str = "hello"s;          // std::string
auto view = "hello"sv;        // std::string_view (C++17)
auto multi = "hello"
             " world"s;        // std::string "hello world"
```

### 案例 4：复数字面量（标准库 `i`）

```cpp
using namespace std::complex_literals;

auto z = 1.0 + 2.0i;          // std::complex<double>
auto pure = 3.0i;              // std::complex<double>(0, 3.0)
auto f = 1.0f + 2.0if;        // std::complex<float>
```

## 标准库提供的用户自定义字面量

| 头文件 | 后缀 | 返回类型 | 引入标准 |
|--------|------|----------|----------|
| `<chrono>` | `h`, `min`, `s`, `ms`, `us`, `ns` | 对应 `chrono::duration` | C++14 |
| `<string>` | `s` | `std::string` | C++14 |
| `<string_view>` | `sv` | `std::string_view` | C++17 |
| `<complex>` | `i`, `if`, `il` | `std::complex<float/double/long double>` | C++14 |

注意：`operator""s` 在 C++11 中属于 `<chrono>` 且作用于 `const char*`，C++14 才将其移入 `<string>` 并增加 `size_t` 重载。

## 最佳实践

1. **以下划线开头命名后缀**：标准库后缀不带下划线（如 `s`、`sv`），用户定义后缀带下划线（如 `_km`、`_deg`）。这避免命名冲突。

2. **声明为 `constexpr`**：尽量让 UDL 在编译期求值，零运行时开销。

3. **提供强类型而非原始类型**：
   ```cpp
   // ❌ 弱：返回 double，丢失单位信息
   double operator"" _km(long double v) { return v * 1000; }

   // ✅ 强：返回类型安全的距离类型
   constexpr Distance operator"" _km(unsigned long long v) {
       return Distance{v * 1000};
   }
   ```

4. **在命名空间中封装**：避免全局命名空间污染：
   ```cpp
   namespace units {
       constexpr Distance operator"" _km(unsigned long long v) { /*...*/ }
       constexpr Distance operator"" _m(unsigned long long v) { /*...*/ }
   }

   // using namespace units;  // 按需引入
   ```

5. **仅用于提升可读性**：如果 UDL 不比普通构造函数更清晰，就不要使用。

## 常见陷阱

### 陷阱 1：整数与浮点参数形式的选择

```cpp
// 只接受浮点字面量
long double operator"" _r(long double v) { return v; }
// 5.0_r ✅   5_r ❌ 编译错误

// 只接受整数字面量
unsigned long long operator"" _r(unsigned long long v) { return v; }
// 5_r ✅   5.0_r ❌ 编译错误
```

如果需要同时接受两种类型，必须定义两个重载。

### 陷阱 2：字符串字面量的存储期

```cpp
// ⚠️ 危险：返回指向临时 buffer 的指针
const char* operator"" _warn(const char* str, size_t) {
    static char buf[256];
    snprintf(buf, 256, "WARNING: %s", str);
    return buf;  // 非线程安全，且下次调用覆盖
}
```

字符串 UDL 的 `const char*` 参数指向的是编译期静态存储，生命周期合法；但返回值应使用 `std::string` 或 `std::string_view` 管理。

### 陷阱 3：`operator""` 与保留后缀冲突

```cpp
// ❌ 不允许：标准保留的无下划线后缀
// long double operator"" km(long double v) { return v; }

// ✅ 正确：以下划线开头
long double operator"" _km(long double v) { return v; }
```

### 陷阱 4：模板形式的编译时间开销

```cpp
template <char... Chars>
constexpr int operator"" _as_int() {
    // 递归模板展开——大量字面量时编译慢
    // 编译器对模板递归深度有限制
    return parse_int<Chars...>::value;
}
```

对长字面量字符串使用模板形式 UDL 会显著增加编译时间。优先使用 `const char*, size_t` 形式配合运行时解析，或用 `constexpr` 函数处理。

### 陷阱 5：ADL 与 `using namespace` 交互

```cpp
namespace A {
    struct Meter { double v; };
    constexpr Meter operator"" _m(unsigned long long v) { return Meter{static_cast<double>(v)}; }
}

namespace B {
    struct Meter { double v; };
    constexpr Meter operator"" _m(unsigned long long v) { return Meter{static_cast<double>(v)}; }
}

// using namespace A;
// using namespace B;
// auto x = 5_m;  // ❌ 歧义：两个候选
```

将 UDL 放在独立命名空间并按需 `using` 引入，避免歧义。
