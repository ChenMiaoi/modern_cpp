# Abseil Status 与 StatusOr：显式错误处理

> 源码路径：`references/impl/abseil-cpp/absl/status/status.h`, `status/statusor.h`

Google C++ 风格指南**禁止使用异常**（在大多数场景下）——异常在大型代码库中难以推理控制流，且使性能分析工具难以工作。`absl::Status` 是显式错误返回的核心类型。

## Status 内部布局

```cpp
// status.h 中的简化表示
class Status {
  // OK 状态：指针为 nullptr，零开销
  // 非 OK 状态：指向堆分配的 State 结构
  struct State {
    absl::StatusCode code_;        // 错误码枚举
    std::string message_;          // 错误消息
    absl::InlinedVector<Payload, 1> payloads_;  // 附加数据
  };
  State* state_;                   // nullptr = OK
};
```

关键设计：OK 状态时 `state_` 为 `nullptr`，`sizeof(Status)` = 一个指针（8 字节）。只有出错时才分配堆内存。这是**错误路径有开销、成功路径零开销**的设计哲学。

```cpp
// 使用示例
absl::Status ReadFile(const std::string& path, std::string* contents) {
  if (!FileExists(path)) {
    return absl::NotFoundError("File not found: " + path);
  }
  // ... 读取文件 ...
  if (read_error) {
    return absl::InternalError("Read failed");
  }
  return absl::OkStatus();  // state_ = nullptr, 零开销
}
```

## StatusOr\<T\>：tagged union

`StatusOr<T>` 是一个 tagged union，内部由 `StatusOrData<T>` 持有数据：

```
  StatusOr<T> 继承树:

  ┌───────────────────────────────────────────────────────────────────────────┐
  │  StatusOr<T> : OperatorBase<T>, StatusOrData<T>,                         │
  │                CopyCtorBase<T>, MoveCtorBase<T>,                         │
  │                CopyAssignBase<T>, MoveAssignBase<T>                      │
  │                                                                         │
  │  每个 Base 通过 SFINAE (std::enable_if) 控制:                           │
  │    T 不可拷贝 → 拷贝构造/赋值 = delete                                  │
  │    T 不可移动 → 移动构造/赋值 = delete                                  │
  │    static_assert(!is_same<T, Status>)  禁止二义性                       │
  └───────────────────────────────────────────────────────────────────────────┘

  StatusOrData<T> 核心布局:

  ┌─── StatusOrData<T> ──────────────────────────────────────────────────┐
  │                                                                       │
  │  union Storage {                                                      │
  │    char dummy_;              // 未使用时                              │
  │    T value_;                 // 存储的有效值                          │
  │  };                                                                   │
  │  Storage data_;                                                        │
  │  Status status_;             // OK 时 state_=nullptr (8B)             │
  │                                                                       │
  │  sizeof(StatusOr<T>) = max(sizeof(T), 8) + 8 (Status) + padding     │
  │                                                                       │
  │  状态判断:                                                            │
  │    status_.ok() == true  → data_ 中有有效值 T                        │
  │    status_.ok() == false → data_ 是 dummy_，status_ 中有错误信息     │
  └───────────────────────────────────────────────────────────────────────┘
```

```cpp
// 使用示例
absl::StatusOr<int> ParseInt(absl::string_view s) {
  int result;
  if (!absl::SimpleAtoi(s, &result)) {
    return absl::InvalidArgumentError("Not a number: " + std::string(s));
  }
  return result;  // 隐式构造 StatusOr<int>(result)
}

// 调用方
auto value = ParseInt("42");
if (!value.ok()) {
  LOG(ERROR) << value.status();
  return;
}
int x = *value;  // 或 value.value()
```

## 错误传播：`RETURN_IF_ERROR` 宏

Google 内部使用宏简化错误传播：

```cpp
#define RETURN_IF_ERROR(expr)                  \
  do {                                         \
    auto _status = (expr);                     \
    if (!_status.ok()) return _status;         \
  } while (0)

#define ASSIGN_OR_RETURN(lhs, rexpr)           \
  auto _status_or = (rexpr);                   \
  if (!_status_or.ok()) return _status_or.status(); \
  lhs = std::move(_status_or).value()
```

```cpp
absl::StatusOr<Data> ProcessFile(const std::string& path) {
  std::string contents;
  RETURN_IF_ERROR(ReadFile(path, &contents));
  
  ASSIGN_OR_RETURN(auto parsed, ParseData(contents));
  
  return Transform(parsed);
}
```

这使得代码看起来像异常风格（每个失败点自动返回），但实际上是显式返回——编译器可以检查返回类型，不会有隐藏的控制流。

## Status 与 std::expected 的对比

| 维度 | `absl::Status` / `StatusOr` | `std::expected` (C++23) |
|------|---------------------------|------------------------|
| 错误类型 | 固定 `Status`（带 StatusCode + string） | 模板参数 `E` |
| OK 状态开销 | `nullptr`，8 字节 | 值在 union 中，与 T 同大小 |
| 错误附加信息 | payloads（键值对） | 无（需扩展 E 类型） |
| 适用场景 | Google 大型代码库 | 通用库 |

`StatusOr` 的设计更贴合大型服务代码——错误码、消息、附加数据（如请求 ID、堆栈追踪）都可以携带。`std::expected` 更通用——错误类型是模板参数，可以是任何类型。

## Payload 机制

`Status` 支持附加任意键值对数据，用于在错误传播链中携带上下文：

```cpp
absl::Status s = absl::InternalError("disk full");
s.SetPayload("path", absl::Cord("/data/db"));
s.SetPayload("errno", absl::Cord(std::to_string(ENOSPC)));

// 检查 payload
auto path = s.GetPayload("path");
```

这在分布式系统中特别有用——错误从底层一路传播到顶层时，每层都可以附加自己的上下文信息，最终日志中可以看到完整的错误链条。
