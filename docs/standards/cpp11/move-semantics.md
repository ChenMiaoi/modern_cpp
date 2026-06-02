---
title: "右值引用与移动语义"
topic: unknown
feature: move-semantics
standard: N/A
status_checked_at: 2026-06-02
---
# 右值引用与移动语义

## 核心问题

在 C++11 之前，从函数返回大对象或交换两个对象时不可避免地触发深拷贝：

```cpp
std::vector<int> create_data() {
    std::vector<int> v(1000000, 42);
    return v;  // C++98：深拷贝整个 vector
}
```

移动语义的本质：**将资源（堆内存、文件句柄等）从一个对象"偷"到另一个对象，而不是复制。**

## 值类别基础

| 值类别 | 含义 | 示例 |
|--------|------|------|
| 左值 (lvalue) | 有身份、可取地址 | 变量、`*p`、`++i` |
| 亡值 (xvalue) | 有身份、即将被销毁 | `std::move(x)`、`static_cast<T&&>(x)` |
| 纯右值 (prvalue) | 无身份、临时值 | 字面量 `42`、`a + b`、函数返回值 |

右值引用 (`T&&`) 只能绑定到亡值和纯右值。

## 移动构造函数与移动赋值运算符

```cpp
class Buffer {
    int* data_;
    size_t size_;
public:
    // 移动构造函数
    Buffer(Buffer&& other) noexcept
        : data_(other.data_), size_(other.size_)
    {
        other.data_ = nullptr;  // 置空源对象
        other.size_ = 0;
    }

    // 移动赋值运算符
    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            delete[] data_;        // 释放自己的资源
            data_ = other.data_;   // 偷走对方的资源
            size_ = other.size_;
            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    ~Buffer() { delete[] data_; }
};
```

## `std::move`

`std::move` 本身不移动任何东西——它只是将左值强制转换为右值引用，表示"我不再需要这个对象了，你可以移动它的资源"：

```cpp
std::string s1 = "hello";
std::string s2 = std::move(s1);  // s1 的内容被移动到 s2
// s1 现在处于"合法但未指定"状态
```

## 完美转发

`std::forward<T>(arg)` 根据模板参数 `T` 决定是转发为左值还是右值引用：

```cpp
template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
```

引用折叠规则：
- `T& &` → `T&`
- `T& &&` → `T&`
- `T&& &` → `T&`
- `T&& &&` → `T&&`

## 移动语义的经济学

```cpp
// 场景 1：返回局部对象（编译器自动移动/RVO）
std::vector<int> make_vec() {
    std::vector<int> v = {1, 2, 3};
    return v;  // 移动或 RVO，零拷贝
}

// 场景 2：插入到容器
std::vector<std::string> vec;
std::string s = "hello world";
vec.push_back(std::move(s));  // 移动而非拷贝

// 场景 3：交换
std::string a = "hello", b = "world";
std::swap(a, b);  // 内部使用 std::move，三次移动而非三次深拷贝
```

## `noexcept` 的重要性

移动操作**必须**标记 `noexcept`。标准库容器（如 `vector` 扩容）只在移动构造函数承诺不抛异常时才使用移动：

```cpp
class Widget {
public:
    Widget(Widget&&) noexcept;            // ✓ vector 扩容时会移动
    Widget(Widget&&);                     // ✗ vector 扩容时会拷贝！
};
```

## 何时不使用 `std::move`

```cpp
// 错误：对返回值使用 move 会阻止 RVO（Return Value Optimization）
std::vector<int> make_vec() {
    std::vector<int> v = {1, 2, 3};
    return std::move(v);  // 别这样！直接 return v; 更好
}
```

## Rule of Five

如果你定义了以下五个特殊成员函数中的任何一个，通常需要定义全部：

1. **析构函数**
2. **拷贝构造函数**
3. **拷贝赋值运算符**
4. **移动构造函数**
5. **移动赋值运算符**

```cpp
class Resource {
public:
    ~Resource();
    Resource(const Resource&);
    Resource& operator=(const Resource&);
    Resource(Resource&&) noexcept;
    Resource& operator=(Resource&&) noexcept;
};
```

## 与智能指针的关系

- `std::unique_ptr`：只可移动，不可拷贝
- `std::shared_ptr`：可拷贝也可移动，移动比拷贝便宜（少一次原子操作）
