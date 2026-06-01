# 异常安全

## 三级保证

### Nothrow Guarantee（不抛异常保证）

操作保证不抛出异常。`noexcept` 函数、析构函数、swap 通常提供此保证。

### Basic Guarantee（基本保证）

如果操作抛出异常，程序处于**有效但未指定的状态**——没有资源泄漏，但对象的值可能已改变。

### Strong Guarantee（强保证）

如果操作抛出异常，程序状态**回滚到操作之前**——就像操作从未发生过一样（事务语义）。

```cpp
// 强保证的典型实现：先在副本上操作，再 swap
void Widget::update(const Data& d) {
  Widget copy(*this);       // 先拷贝
  copy.data_ = d;           // 在副本上操作（如果抛异常，原对象不变）
  swap(copy);               // noexcept swap
}
```

## RAII（Resource Acquisition Is Initialization）

C++ 的基石——资源在构造函数中获取，析构函数中释放：

```cpp
{
  std::lock_guard<std::mutex> lk(mtx);    // 构造时加锁
  // ... 操作共享数据 ...
}  // 析构时自动解锁——即使发生异常

{
  auto file = std::unique_ptr<File>(open("data.bin"));  // 构造时打开
  // ... 使用文件 ...
}  // 析构时自动关闭
```

RAII 使得异常安全变得自然——只要资源被 RAII 对象管理，析构函数保证在栈展开时被调用。

## Stack Unwinding（栈展开）

当异常被抛出时，运行时沿着调用栈向上查找匹配的 `catch` 块。每经过一个函数，该函数中的局部变量按构造的逆序析构：

```
main()
  → foo()
    → bar()
      → throw std::runtime_error("oops");
    ← bar() 的局部变量析构
  ← foo() 的局部变量析构
  → catch (const std::exception& e) { ... }
```

## Scope Guard

一种确保代码在作用域退出时执行的惯用法（无论是否发生异常）：

```cpp
// C++11 的简单实现
auto guard = finally([]{ cleanup(); });

// C++20 可以用 jthread 的析构函数达到类似效果
```

## 异常规格的历史

```
C++98:  void f() throw(std::runtime_error);  // 动态异常规格（已废弃）
C++11:  void f() noexcept;                    // noexcept 说明符
C++17:  移除动态异常规格
```

**noexcept 的二元性**：它既是函数签名的一部分（参与重载决议），又是编译器的优化提示。移动构造函数不标记 `noexcept` 会导致标准库回退到拷贝。
