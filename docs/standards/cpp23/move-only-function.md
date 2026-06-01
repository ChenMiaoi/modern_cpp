# std::move_only_function

C++23 引入 `std::move_only_function`，是一个仅支持移动的可调用对象包装器，解决了 `std::function` 要求被包装的可调用对象必须可拷贝的限制。

## 基本用法

```cpp
#include <functional>
#include <memory>
#include <iostream>

int main() {
    auto ptr = std::make_unique<int>(42);
    std::move_only_function<int()> fn = [p = std::move(ptr)]() {
        return *p;
    };
    std::cout << fn() << "\n";  // 42

    auto fn2 = std::move(fn);  // 移动构造
    // fn 现在为空
}
```

## 与 std::function 的对比

```cpp
auto ptr = std::make_unique<int>(42);

// std::function — 编译失败！unique_ptr 不可拷贝
// std::function<int()> bad = [p = std::move(ptr)]() { return *p; };

// move_only_function — 合法
std::move_only_function<int()> good = [p = std::move(ptr)]() { return *p; };
```

| 特性 | `std::function` | `std::move_only_function` |
|------|-----------------|--------------------------|
| 可拷贝 | 是 | 否（仅移动） |
| 存储 move-only callable | 否 | 是 |
| 空调用抛异常 | `bad_function_call` | `bad_function_call` |
| 小对象优化 | 是 | 是 |

## 函数签名

```cpp
std::move_only_function<int(int, int)> add = [](int a, int b) { return a + b; };
std::move_only_function<void(const std::string&)> printer =
    [](const std::string& s) { std::cout << s << "\n"; };
std::move_only_function<double()> rand_gen =
    [engine = std::mt19937{}]() mutable {
        return std::uniform_real_distribution<>(0.0, 1.0)(engine);
    };

// noexcept 版本
std::move_only_function<int() noexcept> safe = []() noexcept { return 42; };
```

## const 与引用限定

```cpp
std::move_only_function<int() const> cfn = []() { return 1; };
std::move_only_function<int() &> lfn = []() { return 1; };
```

## 实际应用场景

### 异步回调

```cpp
#include <functional>
#include <queue>
#include <memory>

class TaskQueue {
    std::queue<std::move_only_function<void()>> tasks_;
public:
    void push(std::move_only_function<void()> task) {
        tasks_.push(std::move(task));
    }
    void run_all() {
        while (!tasks_.empty()) { tasks_.front()(); tasks_.pop(); }
    }
};

TaskQueue queue;
auto resource = std::make_unique<Database>();
queue.push([r = std::move(resource)]() { r->query("SELECT 1"); });
queue.run_all();
```

### scope_exit 模式

```cpp
class scope_exit {
    std::move_only_function<void()> action_;
public:
    explicit scope_exit(std::move_only_function<void()> a) : action_(std::move(a)) {}
    ~scope_exit() { if (action_) action_(); }
    scope_exit(const scope_exit&) = delete;
    scope_exit& operator=(const scope_exit&) = delete;
    scope_exit(scope_exit&&) = default;
    scope_exit& operator=(scope_exit&&) = default;
};

void process() {
    auto* fd = open_file("data.txt");
    scope_exit cleanup([fd]() { close_file(fd); });
    // ... 使用 fd
}  // cleanup 析构时执行清理
```

## noexcept 语义

```cpp
std::move_only_function<int() noexcept> safe;
std::move_only_function<int()> maybe_throws;

// 类型不同，不能互相赋值
// safe = maybe_throws;  // 编译错误
```

## 替代方案对比

```cpp
// 1. move_only_function（推荐）
std::move_only_function<int()> fn = [p = std::move(ptr)]() { return *p; };

// 2. 模板参数（零开销但无法存储异构 callable）
template <typename F> void call(F&& f) { f(); }

// 3. shared_ptr 包装绕过限制（有额外开销）
std::function<int()> fn2 = [p = std::shared_ptr<int>(std::move(ptr))]() {
    return *p;
};
```

## 注意事项

- 空 `move_only_function` 调用时抛 `std::bad_function_call`，可用 `operator bool` 检查
- 移动后的对象处于合法但未指定状态（通常为空）
- SBO（小对象优化）意味着小 lambda 不会分配堆内存
- 支持有状态的 `mutable` lambda
