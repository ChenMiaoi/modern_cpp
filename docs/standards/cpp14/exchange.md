# C++14 std::exchange

## 概述

`std::exchange` 将一个变量设置为新值，同时返回其旧值。它是一个原子性的"替换并取回"操作，在实现移动语义、状态机转换和容器操作时非常有用。虽然可以用三行代码手动实现，但标准库提供了一个高效、通用的版本。

## 语法

```cpp
template <class T, class U = T>
constexpr T exchange(T& obj, U&& new_value);
```

- `obj`：要被替换的变量
- `new_value`：新值
- 返回 `obj` 的旧值

头文件：`<utility>`

## 实现原理

```cpp
// 标准库的简化实现
template <class T, class U = T>
constexpr T exchange(T& obj, U&& new_value) {
    T old_value = std::move(obj);
    obj = std::forward<U>(new_value);
    return old_value;
}
```

## 代码示例

### 基本用法

```cpp
#include <utility>
#include <iostream>

int main() {
    int x = 10;
    int old = std::exchange(x, 42);

    std::cout << "old: " << old << ", new: " << x << '\n';
    // 输出: old: 10, new: 42
}
```

### 移动构造函数中的应用

```cpp
#include <utility>
#include <cstddef>

template <typename T>
class Buffer {
    T*     data_;
    size_t size_;
    size_t capacity_;

public:
    // 默认构造
    Buffer() : data_(nullptr), size_(0), capacity_(0) {}

    // 构造函数、析构函数等省略...

    // 移动构造：使用 exchange 将源对象置为安全状态
    Buffer(Buffer&& other) noexcept
        : data_(std::exchange(other.data_, nullptr))
        , size_(std::exchange(other.size_, 0))
        , capacity_(std::exchange(other.capacity_, 0))
    {}

    // 不使用 exchange 的等价写法（更冗长）
    // Buffer(Buffer&& other) noexcept
    //     : data_(other.data_), size_(other.size_), capacity_(other.capacity_)
    // {
    //     other.data_ = nullptr;
    //     other.size_ = 0;
    //     other.capacity_ = 0;
    // }
};
```

### 移动赋值运算符

```cpp
#include <utility>

template <typename T>
class UniqueHandle {
    T handle_;

public:
    UniqueHandle() : handle_(T{}) {}
    explicit UniqueHandle(T h) : handle_(h) {}

    ~UniqueHandle() { /* release handle_ */ }

    // 移动赋值：利用 swap 语义实现强异常安全
    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        // 先接管新资源，再释放旧资源
        // exchange 确保 this->handle_ 获得新值，other.handle_ 变为无效
        T old = std::exchange(handle_, std::exchange(other.handle_, T{}));
        // old 在此作用域结束时被析构（通过 RAII）
        (void)old;
        return *this;
    }
};
```

### 替代手动 swap 模式

```cpp
#include <utility>

// 传统 swap 模式
void traditional_swap(int& a, int& b) {
    int tmp = a;
    a = b;
    b = tmp;
}

// 使用 exchange 的等价写法
void exchange_swap(int& a, int& b) {
    b = std::exchange(a, b);
    // 等价于：tmp = a; a = b; b = tmp;
}

// 更复杂的例子：轮转
void rotate(int& a, int& b, int& c) {
    // a → b → c → a
    int old_a = std::exchange(a, std::exchange(b, std::exchange(c, a)));
    (void)old_a;
}
```

### 状态机转换

```cpp
#include <utility>
#include <string>

enum class State { Idle, Connecting, Connected, Disconnecting };

class Connection {
    State state_ = State::Idle;

public:
    // 返回旧状态，设置新状态
    State transition(State new_state) {
        return std::exchange(state_, new_state);
    }

    void connect() {
        State prev = transition(State::Connecting);
        // prev 是 Idle — 可用于日志或断言
    }

    void disconnect() {
        State prev = transition(State::Disconnecting);
        if (prev != State::Connected) {
            // 非预期状态转换
        }
    }
};
```

### 与容器操作配合

```cpp
#include <vector>
#include <utility>

// 从 vector 中取出所有元素
template <typename T>
std::vector<T> drain(std::vector<T>& vec) {
    return std::exchange(vec, {});  // 取走内容，vec 变为空
}

// 取出并替换配置
struct Config {
    int timeout = 30;
    int retries = 3;
};

Config apply_config(Config& current, Config new_config) {
    return std::exchange(current, std::move(new_config));
}
```

### constexpr 使用

```cpp
#include <utility>

// C++14 起 constexpr 支持局部变量修改
constexpr int example() {
    int x = 1;
    int old = std::exchange(x, 42);  // constexpr OK
    return old + x;  // 1 + 42 = 43
}

static_assert(example() == 43);
```

## 最佳实践

1. **移动构造函数首选 `exchange`**：将源对象成员逐个用 `exchange` 置为默认值，比先拷贝再手动清除更简洁、不易遗漏。
2. **理解返回旧值语义**：`exchange` 的核心价值在于同时获取旧值并设置新值，单方面的赋值不需要它。
3. **`exchange` 不是线程安全的**：它不是原子操作，仅是 `move` + `assign` 的组合。需要原子操作请使用 `std::atomic::exchange`。
4. **注意移动开销**：对 POD 类型（int、指针等），`exchange` 编译为零开销；对复杂类型，确保移动构造高效。
5. **状态机场景天然适合**：任何"保存旧值、设置新值、根据旧值决定行为"的模式都可以用 `exchange` 简化。
6. **不要用于 `std::swap`**：虽然 `exchange` 可以实现 swap，但 `std::swap` 语义更清晰且经过优化。`exchange` 更适合单向替换场景。
