---
title: Abseil Status and StatusOr
topic: libraries
feature: status
standard: N/A
status_checked_at: 2026-06-02
implementation:
  abseil:
    paths:
      - references/impl/abseil-cpp/absl/status/status.h
      - references/impl/abseil-cpp/absl/status/statusor.h
    symbols:
      - Status
      - StatusOr
      - StatusCode
exercises: []
solutions: []
---
# Abseil Status and StatusOr: Explicit Error Handling

> Source path: `references/impl/abseil-cpp/absl/status/status.h`, `status/statusor.h`

Google's C++ style guide **prohibits exceptions** (in most scenarios)—exceptions make control flow difficult to reason about in large codebases and complicate performance analysis tools. `absl::Status` is the core type for explicit error returns.

## Status Internal Layout

`Status` uses a tagged pointer design, inlining common status codes into the pointer value to avoid heap allocation:

```cpp
// Simplified representation in status.h (actual implementation is more complex)
class Status {
  uintptr_t rep_;  // Tagged pointer:
  // - Low bits distinguish "inline canonical status code" from "heap-allocated StatusRep"
  // - Inline mode: directly encodes StatusCode (e.g., OK, CANCELLED, etc.)
  // - Heap mode: points to StatusRep (contains code + message + payloads + source location)
  struct StatusRep {
    absl::StatusCode code_;
    std::string message_;
    // payloads, source location, etc.
  };
};
```

Key design: OK and common error codes are inlined directly in `rep_`, with zero heap allocation. Only complex errors carrying messages or additional data allocate heap memory. `sizeof(Status)` = one pointer size (8 bytes).

```cpp
// Usage example
absl::Status ReadFile(const std::string& path, std::string* contents) {
  if (!FileExists(path)) {
    return absl::NotFoundError("File not found: " + path);
  }
  // ... read file ...
  if (read_error) {
    return absl::InternalError("Read failed");
  }
  return absl::OkStatus();  // state_ = nullptr, zero overhead
}
```

## StatusOr\<T\>: Tagged Union

`StatusOr<T>` is a tagged union, internally held by `StatusOrData<T>`:

```
  StatusOr<T> inheritance tree:

  ┌───────────────────────────────────────────────────────────────────────────┐
  │  StatusOr<T> : OperatorBase<T>, StatusOrData<T>,                         │
  │                CopyCtorBase<T>, MoveCtorBase<T>,                         │
  │                CopyAssignBase<T>, MoveAssignBase<T>                      │
  │                                                                         │
  │  Each Base uses SFINAE (std::enable_if) to control:                      │
  │    T not copyable → copy construct/assign = delete                       │
  │    T not movable → move construct/assign = delete                        │
  │    static_assert(!is_same<T, Status>)  prevents ambiguity                │
  └───────────────────────────────────────────────────────────────────────────┘

  StatusOrData<T> core layout:

  ┌─── StatusOrData<T> ──────────────────────────────────────────────────┐
  │                                                                       │
  │  union Storage {                                                      │
  │    char dummy_;              // When unused                           │
  │    T value_;                 // Stored valid value                    │
  │  };                                                                   │
  │  Storage data_;                                                        │
  │  Status status_;             // OK when state_=nullptr (8B)           │
  │                                                                       │
  │  sizeof(StatusOr<T>) = max(sizeof(T), 8) + 8 (Status) + padding     │
  │                                                                       │
  │  State determination:                                                 │
  │    status_.ok() == true  → data_ contains valid value T              │
  │    status_.ok() == false → data_ is dummy_, status_ has error info   │
  └───────────────────────────────────────────────────────────────────────┘
```

```cpp
// Usage example
absl::StatusOr<int> ParseInt(absl::string_view s) {
  int result;
  if (!absl::SimpleAtoi(s, &result)) {
    return absl::InvalidArgumentError("Not a number: " + std::string(s));
  }
  return result;  // Implicitly constructs StatusOr<int>(result)
}

// Caller
auto value = ParseInt("42");
if (!value.ok()) {
  LOG(ERROR) << value.status();
  return;
}
int x = *value;  // Or value.value()
```

## Error Propagation: `RETURN_IF_ERROR` Macro

Google internally uses macros to simplify error propagation:

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

This makes code look like exception style (each failure point returns automatically), but is actually explicit return—the compiler can check return types, with no hidden control flow.

## Status vs std::expected Comparison

| Dimension | `absl::Status` / `StatusOr` | `std::expected` (C++23) |
|-----------|---------------------------|------------------------|
| Error type | Fixed `Status` (with StatusCode + string) | Template parameter `E` |
| OK state overhead | `nullptr`, 8 bytes | Value in union, same size as T |
| Error attachments | Payloads (key-value pairs) | None (requires extending type E) |
| Use case | Google's large codebase | General libraries |

`StatusOr`'s design better fits large service code—error codes, messages, and attachments (such as request IDs, stack traces) can all be carried. `std::expected` is more generic—the error type is a template parameter and can be any type.

## Payload Mechanism

`Status` supports attaching arbitrary key-value pair data for carrying context through error propagation chains:

```cpp
absl::Status s = absl::InternalError("disk full");
s.SetPayload("path", absl::Cord("/data/db"));
s.SetPayload("errno", absl::Cord(std::to_string(ENOSPC)));

// Check payload
auto path = s.GetPayload("path");
```

This is particularly useful in distributed systems—as errors propagate from the bottom to the top, each layer can attach its own context information, and the final log can show the complete error chain.
