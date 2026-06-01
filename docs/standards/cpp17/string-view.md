# C++17 `std::string_view`

## 概述

`std::string_view` 是 C++17 引入的非拥有的字符串引用类型，提供对连续字符序列的只读视图，不分配内存、不拷贝数据。它由指针和长度组成，用于替代 `const std::string&` 参数，消除不必要的堆分配，是零成本抽象的典型代表。

## 语法

```cpp
#include <string_view>

std::string_view sv1 = "hello";                 // 从字面量
std::string_view sv2("hello world", 5);          // 从指针 + 长度
std::string_view sv3 = std::string("hello");     // 从 std::string

sv1.size();      // 5
sv1.empty();     // false
sv1[0];          // 'h'
sv1.data();      // const char*（不保证 null 终止）
```

## 非拥有语义与生命周期

```cpp
std::string_view get_view() {
    std::string s = "temporary";
    return s;  // 危险：s 销毁后 view 悬垂
}

std::string_view safe_view(const std::string& s) {
    return s;  // 安全：调用者保证 s 的生命周期
}

// 字面量具有静态存储期，始终安全
std::string_view sv = "compile-time literal";
```

## 空终止感知

`string_view` **不保证** null 终止：

```cpp
std::string_view sv("hello world", 5);  // "hello"
sv.data()[sv.size()];  // 未定义行为：可能不是 '\0'

// 传递给 C API 必须转换
// C_API(sv.data());              // 危险
C_API(std::string(sv).c_str());    // 安全
```

## 子串操作——零分配

```cpp
std::string_view sv = "Hello, World!";
std::string_view sub = sv.substr(7, 5);  // "World"
// sub.data() 指向 sv.data() + 7，只是移动指针

// 对比 std::string::substr——每次分配堆内存
std::string s = "Hello, World!";
std::string sub2 = s.substr(7, 5);       // 堆分配
```

### remove_prefix / remove_suffix

```cpp
std::string_view sv = "  Hello, World!  ";
sv.remove_prefix(2);   // "Hello, World!  "
sv.remove_suffix(2);   // "Hello, World"

// 用于 trim 操作
auto trim_left(std::string_view sv) -> std::string_view {
    while (!sv.empty() && std::isspace(sv.front()))
        sv.remove_prefix(1);
    return sv;
}
```

:::warning 不可逆
`remove_prefix` 和 `remove_suffix` 不能恢复。需要保留原始视图时先拷贝。
:::

## 作为函数参数——性能优势

```cpp
// C++14：字面量隐式构造临时 string，分配堆内存
void process_old(const std::string& s);
process_old("hello");  // 分配

// C++17：零分配
void process_new(std::string_view s);
process_new("hello");           // 零分配
process_new(std::string("x"));  // 零分配
```

### 与 `const char*` 的权衡

| 特性 | `const char*` | `string_view` |
|------|--------------|---------------|
| 包含长度 | 否 | 是 |
| 空终止 | 保证 | 不保证 |
| `size()` 复杂度 | O(n) | O(1) |
| 支持嵌入 `\0` | 否 | 是 |

## 查找操作

```cpp
std::string_view sv = "Hello, World!";
sv.find("World");              // 7
sv.find_first_of("aeiou");     // 1
sv.starts_with("Hello");       // true（C++20）
// 未找到返回 npos
```

## 转换为 std::string

```cpp
std::string_view sv = "hello";
std::string s(sv);              // 显式构造，拷贝数据

// C++17 不支持隐式转换（设计如此）
// C++23 支持直接赋值：std::string s = sv;
```

## 实际应用

### 解析器中的零分配切片

```cpp
struct Token {
    std::string_view text;
    int line, column;
};

std::vector<Token> tokenize(std::string_view source) {
    std::vector<Token> tokens;
    while (!source.empty()) {
        auto pos = source.find_first_of(" \t\n");
        if (pos == std::string_view::npos) pos = source.size();
        tokens.push_back({source.substr(0, pos), 0, 0});
        source.remove_prefix(std::min(pos + 1, source.size()));
    }
    return tokens;
}
```

### 统一函数参数

```cpp
void configure(std::string_view key, std::string_view value) {
    // 可接受：const char*, std::string, string_view, 字面量
}
configure("host", "localhost");
```

## 最佳实践

1. **函数参数优先 `string_view`**：替代 `const std::string&` 和 `const char*`。
2. **存储副本仍用 `std::string`**：类成员不应使用 `string_view`。
3. **传 C API 前转 `std::string`**：`data()` 不以 `'\0'` 结尾。
4. **`substr` 返回 `string_view`（零分配）**，与 `string::substr` 行为不同。

## 常见陷阱

- **悬垂视图**：指向已销毁的 `std::string` 是最常见的错误。
- **不以 `'\0'` 结尾**：不要假设 `data()[size()] == '\0'`。
- **只读**：不能通过 `string_view` 修改底层数据。
- **`remove_prefix/suffix` 不可逆**：需要时先拷贝原始视图。
- **容器存储注意**：`vector<string_view>` 适合短生命周期，长期存储用 `vector<string>`。
