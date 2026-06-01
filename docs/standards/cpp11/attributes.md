# 属性 (Attributes)

## 概述

C++11 引入标准属性语法 `[[attr]]`，为编译器元信息提供统一、可移植的书写方式。此前依赖各编译器私有扩展（GCC 的 `__attribute__((...))`，MSVC 的 `__declspec(...)`），互不兼容。C++11 标准化了两个属性：`[[noreturn]]` 和 `[[carries_dependency]]`；C++14 加入 `[[deprecated]]`；C++17 一次性加入 `[[nodiscard]]`、`[[maybe_unused]]`、`[[fallthrough]]`。

## `[[noreturn]]`

标注函数**不会将控制流返回调用者**——要么终止程序，要么抛异常，要么无限循环。

```cpp
[[noreturn]] void fatal_error(const char* msg) {
    std::fprintf(stderr, "FATAL: %s\n", msg);
    std::abort(); // never returns
}
```

编译器利用此信息：① 若函数体存在正常返回路径则报错；② 调用点后的代码标记为不可达，消除死分支。

### 典型场景

**仅抛异常的函数：**

```cpp
[[noreturn]] void throw_invalid_arg(const std::string& detail) {
    throw std::invalid_argument(detail);
}
```

**终止程序的包装函数：**

```cpp
[[noreturn]] void panic(const char* file, int line, const char* msg) {
    std::fprintf(stderr, "PANIC at %s:%d: %s\n", file, line, msg);
    std::abort();
}
#define PANIC(msg) panic(__FILE__, __LINE__, msg)
```

**无限循环驱动的线程：**

```cpp
[[noreturn]] void event_loop() {
    while (true) { /* poll events, process queue */ }
}
```

标注错误（函数实际可能返回）会导致**未定义行为**——编译器假设后续代码不可达。

## `[[carries_dependency]]`

背景是 C++11 的 `memory_order_consume`：意图利用硬件数据依赖排序避免不必要的内存屏障。该属性允许依赖链**跨越函数调用**传递。

```cpp
extern std::atomic<int*> global_ptr;

// Parameter-level: dependency flows in through 'p'
void use_value(int* [[carries_dependency]] p) {
    int val = *p; // compiler may elide acquire barrier
}

// Return-level: dependency flows out
int* [[carries_dependency]] load_and_chain() {
    return global_ptr.load(std::memory_order_consume);
}
```

实践中几乎所有编译器都将 `consume` 提升为 `acquire`（保留依赖链极为困难），该属性在当前生态中几乎没有实际效果。新代码应直接使用 `memory_order_acquire`。

## 属性放置规则

```cpp
// 1. 声明前 — 适用于被声明的实体
[[noreturn]] void abort_process();

// 2. 声明符后 — 同样有效
void abort_process() [[noreturn]];

// 3. 类/枚举声明
struct [[deprecated("Use NewParser")]] OldParser {};
enum class [[deprecated]] LegacyMode { A, B };

// 4. 多属性叠加
[[noreturn, cold]] void unreachable_path();
```

属性**不影响类型系统**。两个仅属性不同的函数声明是同一实体（ODR-equivalent），不产生重载歧义。

## 属性与模板

属性可出现在模板声明上，作用于特化后的实体：

```cpp
template <typename T>
[[deprecated("Use process_v2<T>")]]
void process(T value) { /* ... */ }
// process(42); — warning: 'process<int>' is deprecated

// Specialization can independently carry attributes
template <>
[[deprecated("Binary serialization replaced by JSON")]]
void serialize<int>(int val);
```

类模板的属性适用于类本身，不自动传播到成员。

## 编译器私有属性对比

**GCC/Clang `__attribute__((...))`：**

```cpp
__attribute__((noreturn)) void die();
__attribute__((format(printf, 1, 2))) void log(const char* fmt, ...);
__attribute__((hot)) void critical_path();
```

**MSVC `__declspec(...)`：**

```cpp
__declspec(noreturn) void die();
__declspec(dllexport) void public_api();
__declspec(align(64)) struct CacheLine { char data[64]; };
```

标准属性已覆盖的场景（`noreturn`、`deprecated`）**不应再使用私有语法**。标准未覆盖的功能（`format`、`visibility`、`dllexport` 等），用条件编译宏封装：

```cpp
#if defined(__GNUC__) || defined(__clang__)
#  define HOT_FUNC __attribute__((hot))
#elif defined(_MSC_VER)
#  define HOT_FUNC
#endif
```

## C++14 / C++17 属性

**`[[deprecated]]`**（C++14）— 标记废弃 API，可附带迁移消息：

```cpp
[[deprecated("Use NewEngine::init()")]] void initialize_legacy();
```

**`[[nodiscard]]`**（C++17）— 防止忽略返回值：

```cpp
[[nodiscard]] bool validate(const Config& cfg);
[[nodiscard("error code must be checked")]] ErrorCode open(const char* path);
validate(cfg);  // warning: discarding return value
```

**`[[maybe_unused]]`**（C++17）— 抑制未使用警告：

```cpp
void callback(int event, [[maybe_unused]] void* user_data) { /* ... */ }
```

**`[[fallthrough]]`**（C++17）— 显式标注 switch 穿透：

```cpp
switch (state) {
    case State::Init:
        setup();
        [[fallthrough]]; // must be on its own statement with semicolon
    case State::Ready:
        run();
        break;
}
```

## 最佳实践

1. **`[[noreturn]]` 是契约** — 只标注真正不返回的函数，标注错误导致 UB。
2. **优先使用标准属性** — 跨编译器工作，无需宏封装。标准已覆盖的场景弃用私有语法。
3. **`[[carries_dependency]]` 实战中几乎无效** — 了解即可，新代码用 `memory_order_acquire`。
4. **废弃 API 带迁移消息** — `[[deprecated("use new_func()")]]` 比裸属性有用得多。
5. **属性不影响 ABI** — 移除属性不改变函数签名和链接。
6. **属性可叠加** — `[[nodiscard, deprecated("use v2")]]` 或分行书写均可。
7. **C++17 三属性应成为日常习惯** — `[[nodiscard]]` 防忽略返回值，`[[fallthrough]]` 标注有意穿透，`[[maybe_unused]]` 消除合法未使用警告。配合 `-Wall -Werror` 拦截隐藏 bug。
