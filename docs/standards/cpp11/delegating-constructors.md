# 委托构造函数 (Delegating Constructors)

## 概述

C++11 引入了委托构造函数，允许一个构造函数在其初始化列表中调用同一类的另一个构造函数。这一特性消除了长期以来 C++ 中构造函数代码重复的问题——在 C++11 之前，多个构造函数往往需要复制相同的初始化逻辑，通常通过抽取 `init()` 私有成员函数来缓解，但这种方式无法在初始化列表阶段完成初始化，导致成员变量经历"先默认构造再赋值"的低效路径。

委托构造函数使得构造函数之间形成清晰的委托链，每个构造函数只需编写一次初始化逻辑，其他构造函数通过委托复用。

## 语法

```cpp
class Foo {
public:
    // 目标构造函数（被委托的）
    Foo(int a, int b) : a_(a), b_(b) {}

    // 委托构造函数：通过初始化列表调用目标构造函数
    Foo() : Foo(0, 0) {}

    // 委托给另一个委托构造函数，形成链式委托
    Foo(int a) : Foo(a, 0) {}

private:
    int a_;
    int b_;
};
```

核心语法要点：

- 委托出现在**成员初始化列表**的位置，但不能与成员初始化混用
- 委托的目标必须是同一类的构造函数
- 委托构造函数的初始化列表中**只能**包含委托调用，不能同时初始化成员变量

## 委托链 (Delegation Chains)

多个构造函数可以形成链式委托，最终汇聚到一个"目标构造函数"：

```cpp
class Connection {
public:
    // 目标构造函数——所有初始化逻辑的汇聚点
    Connection(const std::string& host, int port, int timeout)
        : host_(host), port_(port), timeout_(timeout), state_(State::Disconnected)
    {
        validate_parameters();
    }

    // 链式委托：委托给上面的目标构造函数
    Connection(const std::string& host, int port)
        : Connection(host, port, 30)  // default timeout
    {}

    // 再一层委托
    Connection(const std::string& host)
        : Connection(host, 80)        // delegates to the two-param version
    {}

    // 最终默认构造
    Connection()
        : Connection("localhost")     // delegates to the one-param version
    {}

private:
    std::string host_;
    int port_;
    int timeout_;
    enum class State { Connected, Disconnected } state_;

    void validate_parameters() {
        if (port_ < 0 || port_ > 65535)
            throw std::invalid_argument("Invalid port");
    }
};
```

委托链方向：`Connection()` → `Connection(string)` → `Connection(string, int)` → `Connection(string, int, int)`。

注意：C++ 标准**不禁止循环委托**，但循环委托会导致运行时未定义行为（通常是无限递归），编译器不会诊断此问题。

## 与成员初始化的关系

委托构造函数的初始化列表中**不能同时包含成员初始化和委托调用**。这是编译器强制的规则：

```cpp
class Widget {
public:
    int x_;
    int y_;
    double scale_;

    // ✅ 合法：委托构造函数只能委托，不能初始化成员
    Widget() : Widget(0, 0) {}

    // ❌ 编译错误：不能混合委托与成员初始化
    // Widget(int x) : Widget(x, 0), scale_(1.0) {}

    // ✅ 合法：目标构造函数可以初始化所有成员
    Widget(int x, int y) : x_(x), y_(y), scale_(1.0) {}
};
```

这意味着目标构造函数必须负责**所有**成员的初始化。如果某些成员没有在目标构造函数中显式初始化，它们会被默认初始化（对 POD 类型来说可能是未定义值）。

## 与基类初始化列表的交互

委托构造函数不能在委托的同时初始化基类。基类初始化也必须在目标构造函数中完成：

```cpp
class Base {
public:
    explicit Base(int id) : id_(id) {}
    int id_;
};

class Derived : public Base {
public:
    // 目标构造函数：同时初始化基类和自身成员
    Derived(int id, const std::string& name)
        : Base(id), name_(name)
    {}

    // 委托构造函数——基类初始化由目标构造函数处理
    Derived(int id)
        : Derived(id, "unnamed")
    {}

    // 默认构造——完整的委托链
    Derived()
        : Derived(42)
    {}

private:
    std::string name_;
};
```

## 代码重复消除：实际案例

### 没有委托构造函数时（C++03 风格）

```cpp
class Matrix {
public:
    Matrix(int rows, int cols)
        : rows_(rows), cols_(cols), data_(new double[rows * cols]())
    {
        if (rows <= 0 || cols <= 0)
            throw std::invalid_argument("Invalid dimensions");
    }

    Matrix(int n)  // square matrix
        : rows_(n), cols_(n), data_(new double[n * n]())
    {
        if (n <= 0)
            throw std::invalid_argument("Invalid dimensions");
    }

    Matrix()  // default 4x4
        : rows_(4), cols_(4), data_(new double[16]())
    {}

    // ❌ 每个构造函数重复 new[]、验证逻辑
private:
    int rows_, cols_;
    std::unique_ptr<double[]> data_;
};
```

### 使用委托构造函数（C++11）

```cpp
class Matrix {
public:
    // 目标构造函数：所有逻辑集中于此
    Matrix(int rows, int cols)
        : rows_(rows), cols_(cols), data_(new double[rows * cols]())
    {
        if (rows <= 0 || cols <= 0)
            throw std::invalid_argument("Invalid dimensions");
    }

    // 委托：方阵
    explicit Matrix(int n) : Matrix(n, n) {}

    // 委托：默认 4×4
    Matrix() : Matrix(4) {}

private:
    int rows_, cols_;
    std::unique_ptr<double[]> data_;
};
```

验证逻辑和资源分配只存在于一处——维护成本显著降低。

## 与 `constexpr` 构造函数的配合

委托构造函数可以是 `constexpr` 的：

```cpp
struct Point {
    int x, y, z;

    constexpr Point(int x, int y, int z) : x(x), y(y), z(z) {}

    // constexpr 委托构造函数
    constexpr Point() : Point(0, 0, 0) {}

    constexpr Point(int v) : Point(v, v, v) {}
};

// 编译期计算可用
constexpr Point origin;
constexpr Point unit(1);
constexpr Point custom(1, 2, 3);
static_assert(custom.z == 3, "z must be 3");
```

## 最佳实践

1. **设计一个"完备"的目标构造函数**：将所有成员初始化和不变式检查集中在一个构造函数中，其余构造函数委托给它。

2. **优先委托而非 `init()` 函数**：`init()` 模式在构造函数体执行时成员已被默认构造，效率更低，且无法用于 `const` 或引用成员的初始化。

3. **保持委托链短而浅**：深层委托链（超过 3 层）会降低可读性，考虑合并中间层。

4. **确保目标构造函数初始化所有成员**：遗漏初始化会导致委托路径上出现未初始化成员。

5. **对 `explicit` 语义保持一致**：如果目标构造函数是 `explicit` 的，委托构造函数不影响此语义——委托是类内部行为，不受 `explicit` 约束。

## 常见陷阱

### 陷阱 1：委托与成员初始化混用

```cpp
class Bad {
    int a_, b_;
public:
    // ❌ 编译错误
    // Bad(int a) : Bad(a, 0), a_(a) {}
    //
    // ✅ 正确写法
    Bad(int a) : Bad(a, 0) {}

    Bad(int a, int b) : a_(a), b_(b) {}
};
```

### 陷阱 2：循环委托

```cpp
class Infinite {
public:
    Infinite() : Infinite(0) {}    // delegates to Infinite(int)
    Infinite(int) : Infinite() {}  // delegates to Infinite()
    // ⚠️ 编译通过，运行时栈溢出
};
```

### 陷阱 3：异常安全问题

```cpp
class Resource {
    int* data_;
public:
    Resource(int n) : data_(new int[n]) {
        throw std::runtime_error("oops");  // data_ 泄漏！
    }

    // 委托构造函数不能捕获目标构造函数的异常来清理
    Resource() : Resource(10) {}
};
```

目标构造函数抛出异常时，委托构造函数的 `catch` 块**不会执行**（因为委托构造函数体还未开始）。目标构造函数必须自行保证异常安全。

### 陷阱 4：委托构造函数中的代码在目标构造函数**之后**执行

```cpp
class Order {
    int priority_;
public:
    Order(int p) : priority_(p) {
        std::cout << "Target ctor body\n";
    }

    Order() : Order(42) {
        // This body runs AFTER Order(int) completes
        std::cout << "Delegating ctor body\n";
    }
};
// Output:
//   Target ctor body
//   Delegating ctor body
```

理解执行顺序对于正确放置日志、注册等副作用至关重要。
