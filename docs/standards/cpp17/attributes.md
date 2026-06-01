# C++17 属性：`[[nodiscard]]`、`[[maybe_unused]]`、`[[fallthrough]]`

## 概述

C++17 标准化了三个实用属性，为编译器提供语义提示。`[[nodiscard]]` 防止忽略重要返回值，`[[maybe_unused]]` 消除未使用变量的警告，`[[fallthrough]]` 明确标注 switch 穿透的意图。

## 语法

```cpp
[[nodiscard]] int compute();
[[maybe_unused]] int debug_val = 42;
void callback([[maybe_unused]] int event_type) { /* ... */ }

switch (state) {
    case A:
        do_something();
        [[fallthrough]];    // 分号不可省略
    case B:
        handle_b();
        break;
}
```

## `[[nodiscard]]`

### 基本用法

```cpp
[[nodiscard]] bool validate(const Data& d);
[[nodiscard]] std::unique_ptr<Resource> acquire();

void process() {
    validate(data);           // 警告：忽略了 nodiscard 返回值
    auto ok = validate(data); // 正确
    acquire();                // 警告
    auto r = acquire();       // 正确
}
```

### 修饰类和枚举

```cpp
// 类类型——返回该类型的所有函数自动 nodiscard
[[nodiscard]] struct ErrorCode {
    int code;
    explicit operator bool() const { return code == 0; }
};

ErrorCode do_work();  // 自动 nodiscard

// 枚举类型
enum [[nodiscard]] class Status { Ok, Error, Timeout };
Status connect(const std::string& host);
connect("localhost");  // 警告
```

### `[[nodiscard]]` 与 `void`

`[[nodiscard]]` 仅在返回类型非 `void` 时有效。`void` 函数无返回值可忽略。

### 显式丢弃

```cpp
[[nodiscard]] int important();
static_cast<void>(important()); // 显式丢弃，无警告
(void)important();              // 同上
```

## `[[maybe_unused]]`

### 变量与参数

```cpp
void debug_example() {
    [[maybe_unused]] int x = compute_something(); // 仅在 debug 使用
}

// 参数——省略参数名通常更简洁
void on_event([[maybe_unused]] int event_type, void* user_data) {
    auto* ctx = static_cast<Context*>(user_data);
    ctx->notify();
}
```

### 类型和静态成员

```cpp
[[maybe_unused]] using DebugType = std::vector<int>;

struct Traits {
    [[maybe_unused]] static constexpr bool is_special = true;
};
```

大多数情况下，省略参数名比 `[[maybe_unused]]` 更简洁：`void f(int /*type*/, void* ctx)`。

## `[[fallthrough]]`

```cpp
void interpret(Token tok) {
    switch (tok) {
        case Token::Number:
            read_number();
            [[fallthrough]];  // 故意穿透
        case Token::Plus:
        case Token::Minus:
            apply_operator();
            break;
    }
}
```

语法注意：`[[fallthrough]]` 后必须有分号，它是属性语句而非控制流语句。

## 多属性组合

```cpp
[[nodiscard, maybe_unused]] int legacy_api();

// 等价于
[[nodiscard]]
[[maybe_unused]]
int legacy_api();
```

## C++20 增强

C++20 允许 `[[nodiscard]]` 附加诊断消息：

```cpp
[[nodiscard("memory will leak if result is discarded")]]
void* allocate(std::size_t size);

allocate(1024);  // 警告消息：memory will leak if result is discarded
```

## 最佳实践

1. **错误码/状态返回值使用 `[[nodiscard]]`**：`[[nodiscard]] bool save(const Document& doc);`
2. **拥有所有权的返回值使用 `[[nodiscard]]`**：`[[nodiscard]] std::unique_ptr<Connection> connect();`
3. **`[[fallthrough]]` 是必须的**：所有故意穿透的 case 都应标注，`-Wimplicit-fallthrough` 会强制要求。
4. **switch 中优先避免穿透**：用独立函数调用或重构逻辑。
5. **用参数省略替代 `[[maybe_unused]]`**：`void f(int /*type*/, void* ctx);`

## 常见陷阱

- **`[[nodiscard]]` 仅是警告**：不能替代正确性检查，需要 `-Werror` 才能变成错误。
- **`[[maybe_unused]]` 可能掩盖 bug**：如果变量本应使用但遗漏了，属性会掩盖逻辑错误。
- **`[[fallthrough]]` 位置必须正确**：必须在 case 块末尾，中间放置无效。
- **属性不影响语义**：三个属性都不改变程序行为，去掉后程序行为完全相同。
- **`static_cast<void>(expr)` 是丢弃返回值的惯用方式**：用于确实不需要返回值的场景。
