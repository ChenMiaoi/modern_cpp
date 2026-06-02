---
title: "编译期计算"
topic: unknown
feature: compile-time-computation
standard: N/A
status_checked_at: 2026-06-02
---
# 编译期计算

## constexpr 的演进

`constexpr` 从 C++11 的受限函数开始，每个标准版本都显著扩展了可在编译期执行的代码范围。

### C++11：单 return 语句

```cpp
constexpr int factorial(int n) {
    return n <= 1 ? 1 : n * factorial(n - 1);
}
static_assert(factorial(5) == 120);
```

### C++14：完整函数体

允许局部变量、循环、if/switch：

```cpp
constexpr int gcd(int a, int b) {
    while (b != 0) { int t = b; b = a % b; a = t; }
    return a;
}
constexpr auto sq = [](int x) { return x * x; };
```

### C++17：if constexpr 与编译期容器操作

```cpp
template <typename T>
constexpr auto absolute(T val) {
    if constexpr (std::is_unsigned_v<T>) return val;
    else return val < 0 ? -val : val;
}

constexpr auto make_lookup_table() {
    std::array<int, 10> table{};
    for (int i = 0; i < 10; ++i) table[i] = i * i;
    return table;
}
constexpr auto sq_table = make_lookup_table();
```

### C++20：consteval、constinit 与重大扩展

允许 dynamic allocation（编译期内不得泄漏）、virtual 函数、union、try-catch：

```cpp
constexpr auto make_vector() {
    std::vector<int> v{1, 2, 3};
    v.push_back(4);
    return v;
}

struct Base { constexpr virtual int value() const = 0; };
struct Derived : Base { constexpr int value() const override { return 42; } };
constexpr int v = Derived{}.value();
```

### C++23：constexpr string/vector

C++20/23 允许在常量求值期间使用 `std::string` 和 `std::vector`——它们的构造、修改和析构操作都可以在编译期执行。这使得编译期计算从"受限的标量操作"走向"可以使用动态容器"。但需注意：编译期内存分配有上限，且动态分配的存储通常不能跨越该次常量求值存活。

```cpp
constexpr auto sorted = [] {
    std::array data{5, 3, 1, 4, 2};
    std::ranges::sort(data);
    return data;
}();
```

## consteval 与 constinit（C++20）

```cpp
// consteval — 必须在编译期求值
consteval unsigned fourcc(char a, char b, char c, char d) {
    return unsigned(a) | (unsigned(b) << 8) | (unsigned(c) << 16) | (unsigned(d) << 24);
}
constexpr auto jpeg = fourcc('J','F','I','F');

// constinit — 保证编译期初始化，避免静态初始化顺序惨案
constinit auto global_config = Config{};
```

## 编译期反射（C++26）

```cpp
consteval std::size_t enum_count(auto enum_type) {
    return std::meta::enumerators_of(enum_type).size();
}
enum class Color { Red, Green, Blue };
static_assert(enum_count(^^Color) == 3);

// 编译期生成 enum→string
constexpr std::string_view color_name(Color c) {
    template for (constexpr auto e : std::meta::enumerators_of(^^Color)) {
        if (c == [:e:]) return std::meta::name_of(e);
    }
    return "unknown";
}

// 编译期遍历结构体成员生成序列化
template <typename T>
std::string to_json(const T& obj) {
    std::string result = "{";
    bool first = true;
    // nonstatic_data_members_of 只遍历数据成员，排除成员函数等
    template for (constexpr auto mem : std::meta::nonstatic_data_members_of(^^T)) {
        if (!first) result += ", "; first = false;
        result += "\"" + std::string(std::meta::name_of(mem)) + "\"";
        result += ": " + serialize(obj.[:mem:]);
    }
    return result + "}";
}
```

## 实用场景

### 查找表生成

```cpp
constexpr auto crc32_table = [] {
    std::array<uint32_t, 256> table{};
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t crc = i;
        for (int j = 0; j < 8; ++j)
            crc = (crc >> 1) ^ (0xEDB88320u & (-(crc & 1)));
        table[i] = crc;
    }
    return table;
}();
```

### 编译期字符串解析

```cpp
constexpr uint32_t parse_hex(std::string_view sv) {
    uint32_t result = 0;
    for (char c : sv) {
        result <<= 4;
        if (c >= '0' && c <= '9') result |= c - '0';
        else if (c >= 'a' && c <= 'f') result |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') result |= c - 'A' + 10;
    }
    return result;
}
constexpr auto color = parse_hex("FF8800");

// 编译期格式字符串验证
consteval bool valid_format(std::string_view fmt) {
    bool percent = false;
    for (char c : fmt) {
        if (c == '%') { percent = !percent; continue; }
        if (percent && c != 'd' && c != 's' && c != '%') return false;
        percent = false;
    }
    return !percent;
}
```

### 编译期配置验证

```cpp
struct Config {
    static constexpr int max_conn = 100;
    static constexpr int timeout_ms = 5000;
};
consteval bool validate_config() {
    static_assert(Config::max_conn > 0);
    static_assert(Config::timeout_ms < 60000, "timeout too large");
    return true;
}
```

编译期计算的核心价值：**将运行时错误提前到编译期，将运行时计算提前到编译期**。前者提升正确性，后者提升性能。"尽可能 constexpr" 已成为现代 C++ 共识。
