---
title: C++26 契约 (Contracts)
status_checked_at: 2026-06-01
topic: unknown
feature: contracts
standard: N/A
---


# C++26 契约 (Contracts)

## 概述

契约（Contracts）是 C++26 的语言级代码正确性基础设施，允许在函数接口和逻辑中声明前置条件、后置条件和断言，具有标准化违规处理和可配置构建模式。

**提案状态：** P2900R14 已被 C++26 接受。

## 语法

### 前置条件 (pre)

```cpp
int safe_divide(int a, int b)
    pre (b != 0)
    pre (a >= 0)
{ return a / b; }
```

### 后置条件 (post)

```cpp
int absolute_value(int x)
    post (r: r >= 0)          // r 绑定返回值
{ return x < 0 ? -x : x; }
```

### 断言 (contract_assert)

```cpp
void process(std::vector<int>& data) pre (!data.empty()) {
    std::ranges::sort(data);
    contract_assert(std::ranges::is_sorted(data));
}
```

## 违规处理

```cpp
void my_handler(std::contract_violation const& v) {
    std::cerr << "契约违规 at " << v.source_location().line() << "\n";
    if (v.is_terminating()) std::abort();
}
std::set_contract_violation_handler(my_handler);
```

## 构建模式

| 模式 | 行为 | 场景 |
|------|------|------|
| **default** | 实现定义 | 一般开发 |
| **ignore** | 不求值，零开销 | 发布构建 |
| **observe** | 求值并调用处理器，不终止 | 日志/监控 |

```bash
g++ -fcontract-mode=observe main.cpp
```

## 与 assert/static_assert 对比

| 特性 | `assert` | `static_assert` | 契约 |
|------|----------|-----------------|------|
| 求值时机 | 运行时 | 编译时 | 运行时（可配置） |
| 发布构建 | 被移除 | 始终存在 | 可选 |
| 位置 | 函数体内 | 任意声明处 | 函数接口 + 函数体 |
| 违规处理 | `abort` | 编译错误 | 自定义处理器 |

```cpp
template <typename T>
T clamp(T val, T lo, T hi) {
    static_assert(std::is_arithmetic_v<T>);   // 编译期
    assert(lo <= hi);                          // 仅 Debug
    contract_assert(lo <= hi);                // 可配置
    return val < lo ? lo : (val > hi ? hi : val);
}
```

## 虚函数契约

```cpp
class Shape {
public:
    virtual double area() const post (r: r >= 0.0);
};

class Circle : public Shape {
    double radius_;
public:
    Circle(double r) : radius_(r) pre (r > 0.0) {}
    double area() const override post (r: r > 0.0)
    { return 3.14159265358979 * radius_ * radius_; }
};
```

规则：派生类前置条件不可更强（只可放宽），后置条件不可更弱（只可收紧），确保 Liskov 替换原则。

## 完整示例

```cpp
#include <vector>
#include <contract>

class Stack {
    std::vector<int> data_;
public:
    void push(int val) post: !data_.empty()
    { data_.push_back(val); }

    int pop() pre (!data_.empty()) {
        int val = data_.back();
        data_.pop_back();
        return val;
    }

    bool empty() const { return data_.empty(); }
};

int main() {
    Stack s;
    s.push(42);
    contract_assert(!s.empty());
    int val = s.pop();
    contract_assert(val == 42);
}
```

## 实现状态

| 编译器 | 状态 |
|--------|------|
| GCC | 开发中，部分语法支持 |
| Clang | 实验性分支 |
| MSVC | 尚未公开实现 |

## 总结

C++26 契约将正确性检查从宏和注释提升为语言特性。通过标准化前置/后置条件、可自定义违规处理器和三种构建模式，契约在开发阶段提供保护，在发布阶段以零或可控开销运行。
