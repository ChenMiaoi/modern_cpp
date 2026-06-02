---
title: "Boost 函数式编程"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# Boost Functional Programming

## Function

`boost::function` is the predecessor of `std::function` — a type-erasing wrapper for callable objects. C++11 standardized it directly.

## Signals2: Thread-Safe Signal-Slot

```cpp
boost::signals2::signal<void(int)> on_value_changed;

// 连接槽
on_value_changed.connect([](int v) { std::cout << "New: " << v; });
on_value_changed.connect([](int v) { log(v); });

// 发射信号
on_value_changed(42);  // 所有连接的槽被调用
```

**Thread-safe**: Uses a read-write lock to protect the signal's connection/disconnection list. A shared lock is used during emission (allowing concurrent reads), and an exclusive lock is used for connect/disconnect operations.

**Automatic connection management**: Use `track()` to monitor object lifetime; connections are automatically severed when the tracked object is destroyed.

## Outcome: Result/Error Type

Boost.Outcome is the predecessor of `std::expected`, providing three result types:

- `result<T, EC>`: value or error code (no exceptions)
- `outcome<T, EC, EP>`: value, error code, or exception
- `BOOST_OUTCOME_TRY`: error propagation macro

```cpp
outcome::result<int> parse(std::string_view s) {
    // ...
    if (error) return outcome::failure(std::errc::invalid_argument);
    return 42;
}

auto val = BOOST_OUTTRY(parse("42"));  // 失败时自动返回
```

## Bind and Lambda

`boost::bind` and `boost::lambda` are predecessors of C++11 lambdas. Modern projects should use C++11 lambdas directly.
