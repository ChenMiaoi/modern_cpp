# Boost 函数式编程

## Function

`boost::function` 是 `std::function` 的前身——可调用对象的类型擦除包装器。C++11 直接将其标准化。

## Signals2：线程安全信号槽

```cpp
boost::signals2::signal<void(int)> on_value_changed;

// 连接槽
on_value_changed.connect([](int v) { std::cout << "New: " << v; });
on_value_changed.connect([](int v) { log(v); });

// 发射信号
on_value_changed(42);  // 所有连接的槽被调用
```

**线程安全**：使用读写锁保护信号的连接/断开列表。发射时使用共享锁（可并发读），连接/断开时使用独占锁。

**自动连接管理**：使用 `track()` 跟踪对象生命周期，对象析构时自动断开连接。

## Outcome：结果/错误类型

Boost.Outcome 是 `std::expected` 的前身，提供三种结果类型：

- `result<T, EC>`：值或错误码（无异常）
- `outcome<T, EC, EP>`：值、错误码或异常
- `BOOST_OUTCOME_TRY`：错误传播宏

```cpp
outcome::result<int> parse(std::string_view s) {
    // ...
    if (error) return outcome::failure(std::errc::invalid_argument);
    return 42;
}

auto val = BOOST_OUTTRY(parse("42"));  // 失败时自动返回
```

## Bind 与 Lambda

`boost::bind` 和 `boost::lambda` 是 C++11 lambda 的前身。现代项目应直接使用 C++11 lambda。
