# constexpr

## 概述

`constexpr` 是 C++11 引入的关键字，用于声明**在编译期求值**的变量和函数。它将计算从运行时提前到编译期，实现零运行时开销，并保留完整的类型安全——是宏和模板元编程的现代替代。

## `constexpr` 变量

```cpp
constexpr int max_size = 100;
constexpr double pi = 3.1415926535;
constexpr int arr_size = max_size + 1;  // 依赖其他 constexpr

int arr[max_size];           // OK: 数组大小
constexpr int* p = nullptr;  // OK: 指针本身是 constexpr
```

### `constexpr` vs `const`

```cpp
const int a = runtime_function();     // OK: 运行时初始化，只读
constexpr int b = runtime_function(); // 错误: 编译期无法求值
```

`const` 关注"能不能改"，`constexpr` 关注"能不能在编译期算出来"。

| 特性 | `const` | `constexpr` |
|------|---------|-------------|
| 含义 | 只读 | 编译期常量 |
| 初始化时机 | 运行时也可以 | 必须编译期 |
| 隐含 `const` | — | 是（变量） |
| 用于数组/模板 | 仅当编译期常量 | 总是 |

## `constexpr` 函数

C++11 严格限制：函数体只能包含**一条 `return` 语句**（加上 `static_assert` 和类型别名）。

```cpp
constexpr int square(int x) { return x * x; }

constexpr int val = square(5);   // 编译期: 25
int arr[square(5)];              // OK: 数组大小

int runtime_val = 42;
int result = square(runtime_val); // 也允许运行时调用
```

### C++11: 用递归替代循环

```cpp
// 错误: C++11 不允许局部变量和循环
// constexpr int sum(int n) {
//     int s = 0; for (int i = 0; i <= n; ++i) s += i; return s;
// }

// 正确: 递归 + 三元表达式
constexpr int sum(int n) {
    return n <= 0 ? 0 : n + sum(n - 1);
}
```

## 编译期计算实例

### 斐波那契与阶乘

```cpp
constexpr long long fib(int n) {
    return n <= 1 ? n : fib(n - 1) + fib(n - 2);
}

constexpr unsigned long long factorial(int n) {
    return n <= 1 ? 1ULL : static_cast<unsigned long long>(n) * factorial(n - 1);
}

static_assert(fib(10) == 55, "");
static_assert(factorial(5) == 120, "");
```

### 编译期哈希（FNV-1a）

```cpp
constexpr uint32_t fnv1a_hash(const char* str, uint32_t basis = 2166136261u) {
    return *str == '\0'
        ? basis
        : fnv1a_hash(str + 1, (basis ^ static_cast<uint32_t>(*str)) * 16777619u);
}

// switch-case 需要常量表达式标签
void process(const char* tag) {
    switch (fnv1a_hash(tag)) {
        case fnv1a_hash("login"):  handle_login();  break;
        case fnv1a_hash("logout"): handle_logout(); break;
        default:                   handle_unknown(); break;
    }
}
```

## `constexpr` 与 `#define` 宏

```cpp
#define SQUARE(x) ((x) * ((x)))   // 文本替换，有副作用风险
constexpr int square(int x) { return x * x; }  // 类型安全，标准语义
```

| 特性 | `#define` | `constexpr` |
|------|-----------|-------------|
| 类型安全 | 否 | 是 |
| 作用域 | 文件全局 | 遵循作用域 |
| 参数多次求值 | 是 | 否 |
| 可调试 | 否 | 是 |

## `constexpr` 构造函数与成员函数

```cpp
struct Point {
    int x, y;
    constexpr Point(int x, int y) : x(x), y(y) {}
    constexpr int manhattan() const {
        return (x >= 0 ? x : -x) + (y >= 0 ? y : -y);
    }
};

constexpr Point p(3, -4);
constexpr int d = p.manhattan();  // 7
```

## 常见使用场景

| 场景 | 示例 |
|------|------|
| 数组大小 | `int buf[constexpr_size];` |
| 模板参数 | `std::array<int, fib(10)>` |
| `case` 标签 | `case fnv1a_hash("tag"):` |
| 替代枚举常量 | `constexpr double timeout = 30.0;`（enum 只能整型） |
| 替代模板元编程 | `constexpr int v = f(n);` 比 `F<N>::value` 直观 |

## C++14 放宽

```cpp
// C++14: 允许局部变量、循环、多条语句
constexpr int sum(int n) {
    int result = 0;
    for (int i = 1; i <= n; ++i) result += i;
    return result;
}
```

C++17 进一步允许 `constexpr if`，C++20 允许 `constexpr` 动态分配和虚函数。

## 最佳实践

| 实践 | 说明 |
|------|------|
| 优先 `constexpr` 替代 `#define` | 类型安全、可调试 |
| `constexpr` 优于 `const` | 当值确实可在编译期确定时 |
| 编译期计算替代模板元编程 | `constexpr` 函数比递归模板更直观 |
| 使用 `constexpr` 构造函数 | 使自定义类型可用于编译期上下文 |

## 常见陷阱

**`constexpr` 不意味着"必须"编译期求值**——运行时调用也合法：

```cpp
constexpr int f(int x) { return x * 2; }
int n; std::cin >> n;
int r = f(n);  // OK: 运行时求值
```

**C++11 递归深度限制**——编译器通常限制 256-512 层递归，超出则编译失败（非栈溢出）。

**C++11 不允许副作用**——`std::cout`、赋值等在 `constexpr` 函数中均非法。

## 与 C++11 之前的对比

| 特性 | C++03 | C++11 `constexpr` |
|------|-------|-------------------|
| 编译期常量 | `enum` / `#define` | `constexpr` 变量 |
| 编译期计算 | 模板元编程 | `constexpr` 函数 |
| 浮点常量 | `#define` | `constexpr double` |
| 类型安全 | 宏无类型 | 完整类型系统 |
| 可读性 | 极差 | 与普通函数一致 |

`constexpr` 是从宏和模板元编程的黑暗时代走向类型安全编译期计算的关键一步。
