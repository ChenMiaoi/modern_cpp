---
title: "静态断言 (static_assert)"
topic: unknown
feature: static-assert
standard: N/A
status_checked_at: 2026-06-02
---
# 静态断言 (static_assert)

## 概述

`static_assert` 是 C++11 引入的编译期断言机制，允许开发者在编译时检查条件是否成立。当条件为 `false` 时，编译器产生一条包含指定消息的编译错误。与运行时 `assert` 不同，`static_assert` 在编译阶段执行，零运行时开销，且能捕获仅在特定模板实例化或平台配置下才暴露的设计错误。

`static_assert` 最常用于：类型特征检查、结构体大小/对齐验证、模板参数约束、枚举值范围校验，以及作为 SFINAE 的补充手段进行早期诊断。

## 基本语法

```cpp
static_assert(constant-expression, string-literal);
```

- **constant-expression**：必须是能在编译期求值的常量表达式，通常涉及 `sizeof`、`alignof`、类型特征、模板参数、`constexpr` 函数等
- **string-literal**：断言失败时显示的错误消息（C++11 要求必须提供；C++17 起可省略）

```cpp
static_assert(sizeof(int) == 4, "int must be 4 bytes");
static_assert(sizeof(void*) == 8, "64-bit pointer required");
```

## C++17 无消息版本

C++17 放宽了语法要求，允许省略消息字符串：

```cpp
// C++11: 必须提供消息
static_assert(sizeof(int) == 4, "int must be 4 bytes");

// C++17: 消息可选
static_assert(sizeof(int) == 4);
```

省略时，部分编译器仍会输出可读的诊断信息（如表达式文本）。

## 与类型特征配合

`static_assert` 与 `<type_traits>` 结合使用是最常见的编译期约束手段：

```cpp
#include <type_traits>

// 约束模板参数为算术类型
template <typename T>
T safe_add(T a, T b) {
    static_assert(std::is_arithmetic<T>::value,
                  "safe_add requires arithmetic types");
    return a + b;
}

// 约束为非 const、可移动类型
template <typename T>
void process(T&& value) {
    static_assert(!std::is_const<std::remove_reference_t<T>>::value,
                  "process does not accept const values");
    static_assert(std::is_move_constructible<T>::value,
                  "T must be move-constructible");
    // ...
}
```

### 常用类型特征断言

```cpp
// 指针类型检查
static_assert(std::is_pointer<int*>::value, "must be a pointer");

// 继承关系检查
static_assert(std::is_base_of<Base, Derived>::value,
              "Derived must inherit from Base");

// 可平凡复制（确保 memcpy 安全）
template <typename T>
void binary_copy(T* dst, const T* src, size_t n) {
    static_assert(std::is_trivially_copyable<T>::value,
                  "binary_copy requires trivially copyable types");
    std::memcpy(dst, src, n * sizeof(T));
}

// 无抛出析构
static_assert(std::is_nothrow_destructible<Widget>::value,
              "Widget must have a non-throwing destructor");
```

## 模板中的静态断言

在模板中，`static_assert` 仅在模板实例化时求值——未实例化的模板不会触发断言。这使得它成为模板约束的精确工具：

```cpp
template <typename T, size_t N>
class FixedVector {
    static_assert(N > 0, "FixedVector size must be positive");
    static_assert(N <= 1024, "FixedVector size exceeds maximum (1024)");
    static_assert(std::is_default_constructible<T>::value,
                  "T must be default-constructible for FixedVector");

    T data_[N];
    size_t size_ = 0;

public:
    void push_back(const T& value) {
        // ...
    }
};

// FixedVector<int, 0>    → 编译错误: "size must be positive"
// FixedVector<int, 2048> → 编译错误: "exceeds maximum"
```

与 SFINAE 不同，`static_assert` 提供清晰的错误消息，但会**硬性阻断**编译。SFINAE 使重载候选"静默退出"，而 `static_assert` 则明确宣告"这是非法用法"。

### static_assert vs SFINAE

```cpp
// SFINAE 风格：重载消失，编译器可能给出模糊的"no matching function"错误
template <typename T,
          typename = std::enable_if_t<std::is_arithmetic<T>::value>>
T add(T a, T b) { return a + b; }

// static_assert 风格：明确报错消息
template <typename T>
T multiply(T a, T b) {
    static_assert(std::is_arithmetic<T>::value,
                  "multiply only works with arithmetic types");
    return a * b;
}
```

实践建议：**SFINAE 用于函数重载决议**（有其他候选可选时），**`static_assert` 用于"绝不允许"的约束**（非法即报错）。

## 结构体大小与对齐断言

确保数据布局与外部 ABI 或序列化协议一致：

```cpp
struct PacketHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;
    uint32_t length;
};

// 确保无 padding 导致布局变化
static_assert(sizeof(PacketHeader) == 12,
              "PacketHeader must be exactly 12 bytes (no padding)");

// 确保对齐满足硬件 DMA 要求
static_assert(alignof(PacketHeader) >= 4,
              "PacketHeader must be at least 4-byte aligned");
```

对齐配合 `alignas` 使用：

```cpp
struct alignas(64) CacheLine {
    char data[64];
};

static_assert(sizeof(CacheLine) == 64, "must fit one cache line");
static_assert(alignof(CacheLine) == 64, "must be cache-line aligned");
```

## 枚举值校验

编译期验证枚举的值域范围和特性：

```cpp
enum class Priority : int {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

// 确保底层类型是 int
static_assert(sizeof(Priority) == sizeof(int),
              "Priority must have int underlying type");

// 验证枚举值范围（利用 C++17 的 std::to_underlying 或直接 cast）
static_assert(static_cast<int>(Priority::Critical) == 3,
              "Priority::Critical must equal 3");

// 确保枚举值覆盖 0..N-1 的连续范围（用于数组索引）
static_assert(static_cast<int>(Priority::Low) == 0, "Low must be 0");
static_assert(static_cast<int>(Priority::Normal) == 1, "Normal must be 1");
static_assert(static_cast<int>(Priority::High) == 2, "High must be 2");
static_assert(static_cast<int>(Priority::Critical) == 3, "Critical must be 3");
```

## 辅助技巧

### 多条件断言拆分

对多个独立条件分别断言，确保每个条件有独立的错误消息：

```cpp
template <typename T>
class Container {
    static_assert(std::is_object<T>::value,
                  "Container requires object types (not references/void)");
    static_assert(!std::is_abstract<T>::value,
                  "Container does not support abstract types");
    static_assert(std::is_destructible<T>::value,
                  "T must be destructible");
};
```

### 编译期计算验证

```cpp
constexpr size_t fibonacci(size_t n) {
    return (n <= 1) ? n : fibonacci(n - 1) + fibonacci(n - 2);
}

static_assert(fibonacci(0) == 0, "fib(0) == 0");
static_assert(fibonacci(1) == 1, "fib(1) == 1");
static_assert(fibonacci(10) == 55, "fib(10) == 55");

// 验证编译期数学常量
constexpr double PI_APPROX = 3.14159265358979;
// 注意：浮点 static_assert 需要容忍精度差异
// static_assert(PI_APPROX == 3.14159265358979, "");  // OK，字面量相同
```

### 条件消息（C++11 兼容）

```cpp
// C++11 不支持无消息版本，可以用宏简化
#define STATIC_CHECK(cond) static_assert(cond, #cond)

STATIC_CHECK(sizeof(int) == 4);
// 编译错误消息: "sizeof(int) == 4"
```

## 最佳实践

1. **每条断言附带诊断消息**：消息应说明约束**原因**，而非仅重复条件。
   ```cpp
   // ❌ 消息无信息量
   static_assert(sizeof(T) > 0, "sizeof(T) > 0");

   // ✅ 解释为什么
   static_assert(sizeof(T) > 0,
                 "T must be a complete type for pointer arithmetic");
   ```

2. **放在类/函数作用域的顶部**：让约束早于任何逻辑出现，便于快速定位编译错误来源。

3. **优先 `static_assert` 而非 `#if` + `#error`**：`static_assert` 对模板参数、类型特征等运行时不可见的条件有效，且不依赖预处理器。

4. **与 `constexpr` 函数结合**：将复杂编译期逻辑封装为 `constexpr` 函数，然后用 `static_assert` 验证其结果。

5. **不要替代运行时检查**：`static_assert` 仅适用于编译期可知的条件。用户输入、动态配置等仍需运行时断言。

## 常见陷阱

### 陷阱 1：非编译期常量表达式

```cpp
int x = 42;
// ❌ 编译错误：x 不是常量表达式
// static_assert(x == 42, "x must be 42");

const int y = 42;
static_assert(y == 42, "y must be 42");  // ✅
```

`static_assert` 的条件必须是常量表达式。`const` 但非 `constexpr` 的变量不是常量表达式。

### 陷阱 2：不完整类型

```cpp
class Forward;

// ❌ 编译错误：Forward 是不完整类型，sizeof 不可用
// static_assert(sizeof(Forward) > 0, "");

// 必须在 Forward 完整定义之后：
class Forward { int x; };
static_assert(sizeof(Forward) >= sizeof(int), "");
```

### 陷阱 3：`static_assert` 在函数作用域中对非实例化模板无效

```cpp
template <typename T>
void foo() {
    static_assert(sizeof(T) > 4, "T must be larger than 4 bytes");
}

// 如果从未调用 foo<char>()，此断言不会触发
// 这不一定是问题，但需理解其"惰性"本质
```

### 陷阱 4：浮点比较精度

```cpp
// ⚠️ 浮点常量表达式比较可能因精度问题失败
constexpr double a = 1.0 / 3.0;
constexpr double b = 0.3333333333333333;

// 这取决于编译器对 constexpr 浮点的精度处理
// static_assert(a == b, "");  // 可能成功也可能失败
```

浮点 `static_assert` 应避免直接相等比较，改用整数比较或容差。

### 陷阱 5：消息中的宏展开

```cpp
#define MSG "check failed"

// ✅ 宏展开为字符串字面量
static_assert(sizeof(int) == 4, MSG);

// ❌ 不能用非字面量作消息
// const char* msg = "oops";
// static_assert(sizeof(int) == 4, msg);
```

消息必须是字符串字面量，不能是变量或运行时字符串。
