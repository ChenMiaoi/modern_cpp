# 值类别（Value Categories）

> "Every expression in C++ has two independent properties: a **type** and a **value category**."——标准

## 五个值类别

C++17 起，值类别构成一个层次结构：

```
            expression
           ┌─────┴─────┐
        glvalue      rvalue
       ┌───┴───┐    ┌───┴───┐
    lvalue   xvalue   prvalue
```

- **lvalue**（左值）：有地址、可取地址的表达式。`int x = 42;` 中的 `x` 是 lvalue。
- **prvalue**（纯右值）：纯的值，没有地址。字面量 `42`、`std::string("hello")` 是 prvalue。
- **xvalue**（将亡值）：一个"即将被移动"的值。`std::move(x)` 的结果是 xvalue。
- **glvalue**（泛左值）= lvalue + xvalue：任何"有身份"的表达式。
- **rvalue**（右值）= prvalue + xvalue：任何可以绑定到右值引用的表达式。

## 实际影响

```cpp
void f(int&);       // 只接受 lvalue
void f(int&&);      // 只接受 rvalue (prvalue 或 xvalue)

int x = 42;
f(x);               // 调用 f(int&)——x 是 lvalue
f(42);              // 调用 f(int&&)——42 是 prvalue
f(std::move(x));    // 调用 f(int&&)——std::move(x) 是 xvalue
```

## Materialization（C++17）

C++17 引入了一个关键概念：**temporary materialization**。当 prvalue 需要被绑定到引用或访问其成员时，它被"实体化"为一个临时对象（xvalue）。

```cpp
struct S { int x; };
S foo() { return S{42}; }  // foo() 是 prvalue

int&& r = foo().x;  // foo() 被实体化为临时 S 对象，然后取其成员
```

C++17 之前，`S s = foo();` 可能涉及两次拷贝（RVO 是可选的）。C++17 起，prvalue **不会**创建临时对象——它直接"在目标位置构造"。这是 **guaranteed copy elision**。

## 陷阱：`auto` 推导丢失引用

```cpp
std::string s = "hello";
auto&& r1 = s;           // r1 是 std::string&（lvalue 引用）
auto&& r2 = std::move(s); // r2 是 std::string&&（rvalue 引用）
auto&& r3 = "hello";     // r3 是 const char(&)[6]（数组引用）

auto&& r4 = std::string("hi"); // r4 是 std::string&&（C++17：prvalue 实体化）
```
