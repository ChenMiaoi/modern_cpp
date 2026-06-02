---
title: "constexpr"
topic: unknown
feature: constexpr
standard: N/A
status_checked_at: 2026-06-02
---
# constexpr

## Overview

`constexpr` is a keyword introduced in C++11 for declaring variables and functions that are **evaluated at compile time**. It shifts computation from runtime to compile time, achieving zero runtime overhead while preserving full type safety — a modern replacement for macros and template metaprogramming.

## `constexpr` Variables

```cpp
constexpr int max_size = 100;
constexpr double pi = 3.1415926535;
constexpr int arr_size = max_size + 1;  // depends on other constexpr

int arr[max_size];           // OK: array size
constexpr int* p = nullptr;  // OK: the pointer itself is constexpr
```

### `constexpr` vs `const`

```cpp
const int a = runtime_function();     // OK: runtime-initialized, read-only
constexpr int b = runtime_function(); // error: cannot evaluate at compile time
```

`const` is about "can it be modified"; `constexpr` is about "can it be computed at compile time."

| Property | `const` | `constexpr` |
|----------|---------|-------------|
| Meaning | Read-only | Compile-time constant |
| Initialization timing | Can be runtime | Must be compile time |
| Implies `const` | — | Yes (for variables) |
| Usable in arrays/templates | Only when compile-time constant | Always |

## `constexpr` Functions

C++11 strictly restricts: the function body can only contain **a single `return` statement** (plus `static_assert` and type aliases).

```cpp
constexpr int square(int x) { return x * x; }

constexpr int val = square(5);   // compile time: 25
int arr[square(5)];              // OK: array size

int runtime_val = 42;
int result = square(runtime_val); // runtime call is also allowed
```

### C++11: Use Recursion Instead of Loops

```cpp
// Error: C++11 does not allow local variables or loops
// constexpr int sum(int n) {
//     int s = 0; for (int i = 0; i <= n; ++i) s += i; return s;
// }

// Correct: recursion + ternary expression
constexpr int sum(int n) {
    return n <= 0 ? 0 : n + sum(n - 1);
}
```

## Compile-Time Computation Examples

### Fibonacci and Factorial

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

### Compile-Time Hash (FNV-1a)

```cpp
constexpr uint32_t fnv1a_hash(const char* str, uint32_t basis = 2166136261u) {
    return *str == '\0'
        ? basis
        : fnv1a_hash(str + 1, (basis ^ static_cast<uint32_t>(*str)) * 16777619u);
}

// switch-case requires constant expression labels
void process(const char* tag) {
    switch (fnv1a_hash(tag)) {
        case fnv1a_hash("login"):  handle_login();  break;
        case fnv1a_hash("logout"): handle_logout(); break;
        default:                   handle_unknown(); break;
    }
}
```

## `constexpr` vs `#define` Macros

```cpp
#define SQUARE(x) ((x) * ((x)))   // textual substitution, side-effect risk
constexpr int square(int x) { return x * x; }  // type-safe, standard semantics
```

| Property | `#define` | `constexpr` |
|----------|-----------|-------------|
| Type safe | No | Yes |
| Scope | File-global | Follows scope |
| Multiple evaluation of arguments | Yes | No |
| Debuggable | No | Yes |

## `constexpr` Constructors and Member Functions

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

## Common Use Cases

| Use Case | Example |
|----------|---------|
| Array size | `int buf[constexpr_size];` |
| Template parameters | `std::array<int, fib(10)>` |
| `case` labels | `case fnv1a_hash("tag"):` |
| Replacing enum constants | `constexpr double timeout = 30.0;` (enum can only hold integers) |
| Replacing template metaprogramming | `constexpr int v = f(n);` is more intuitive than `F<N>::value` |

## C++14 Relaxations

```cpp
// C++14: allows local variables, loops, multiple statements
constexpr int sum(int n) {
    int result = 0;
    for (int i = 1; i <= n; ++i) result += i;
    return result;
}
```

C++17 further allows `constexpr if`, and C++20 allows `constexpr` dynamic allocation and virtual functions.

## Best Practices

| Practice | Explanation |
|----------|-------------|
| Prefer `constexpr` over `#define` | Type-safe, debuggable |
| `constexpr` over `const` | When the value is truly determinable at compile time |
| Compile-time computation over template metaprogramming | `constexpr` functions are more intuitive than recursive templates |
| Use `constexpr` constructors | Makes custom types usable in compile-time contexts |

## Common Pitfalls

**`constexpr` does not mean "must" evaluate at compile time** — runtime calls are also legal:

```cpp
constexpr int f(int x) { return x * 2; }
int n; std::cin >> n;
int r = f(n);  // OK: runtime evaluation
```

**C++11 recursion depth limits** — compilers typically limit 256–512 levels of recursion; exceeding this causes compilation failure (not stack overflow).

**C++11 does not allow side effects** — `std::cout`, assignments, etc. are illegal in `constexpr` functions.

## Comparison with Pre-C++11

| Feature | C++03 | C++11 `constexpr` |
|---------|-------|-------------------|
| Compile-time constants | `enum` / `#define` | `constexpr` variables |
| Compile-time computation | Template metaprogramming | `constexpr` functions |
| Floating-point constants | `#define` | `constexpr double` |
| Type safety | Macros have no type | Full type system |
| Readability | Extremely poor | Consistent with normal functions |

`constexpr` is a key step from the dark ages of macros and template metaprogramming toward type-safe compile-time computation.
