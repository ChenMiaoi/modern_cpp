# C++14 泛型 Lambda (Generic Lambda)

## 概述

C++11 引入了 lambda 表达式，但参数类型必须显式指定。C++14 允许 lambda 参数使用 `auto` 关键字，使 lambda 自动成为泛型的——等价于一个带模板参数的函数对象。这消除了为不同类型的相似操作编写多个 lambda 或显式 Functor 类的需要。

## 语法

```cpp
// C++11 lambda — 参数类型必须显式
auto add = [](int a, int b) { return a + b; };

// C++14 generic lambda — 使用 auto 参数
auto add = [](auto a, auto b) { return a + b; };

// 等价的显式模板 Functor
struct Add {
    template <typename T, typename U>
    auto operator()(T a, U b) const { return a + b; }
};
```

每个 `auto` 参数对应一个独立的模板参数，编译器为每种参数组合生成一个 `operator()` 重载。

## 代码示例

### 基本用法

```cpp
#include <iostream>
#include <string>

int main() {
    // 同一个 lambda 处理不同类型
    auto print = [](auto const& val) {
        std::cout << val << '\n';
    };

    print(42);          // int
    print(3.14);        // double
    print("hello");     // const char*
    print(std::string("world")); // std::string
}
```

### 与 STL 算法配合

```cpp
#include <algorithm>
#include <vector>
#include <string>

// 通用查找：任意容器、任意值类型
auto contains = [](auto const& container, auto const& value) {
    return std::find(container.begin(), container.end(), value)
         != container.end();
};

void demo() {
    std::vector<int> vi = {1, 2, 3, 4, 5};
    std::vector<std::string> vs = {"alpha", "beta", "gamma"};

    contains(vi, 3);        // true
    contains(vs, std::string("beta")); // true
}
```

### 泛型捕获与泛型参数组合

```cpp
#include <functional>

auto make_adder = [](auto x) {
    // 返回一个闭包，捕获 x 的值
    return [x](auto y) { return x + y; };
};

void demo() {
    auto add5 = make_adder(5);
    add5(3);       // 8 — int
    add5(2.5);     // 7.5 — double
}
```

### 多参数泛型 lambda 与完美转发

```cpp
#include <utility>
#include <iostream>

auto perfect_call = [](auto&& func, auto&&... args) {
    return std::forward<decltype(func)>(func)(
        std::forward<decltype(args)>(args)...
    );
};

void greet(const char* name, int times) {
    for (int i = 0; i < times; ++i)
        std::cout << "Hello, " << name << "!\n";
}

void demo() {
    perfect_call(greet, "World", 3);
}
```

## 编译器如何处理泛型 Lambda

编译器将泛型 lambda 转换为一个闭包类型，其中 `operator()` 是一个成员模板：

```cpp
// 你写的：
auto lam = [](auto a, auto b) { return a + b; };

// 编译器生成的（简化）：
struct __closure_type {
    template <typename T, typename U>
    auto operator()(T a, U b) const { return a + b; }
};
```

因此同一个 lambda 对不同类型参数会实例化不同的函数体。

## 最佳实践

1. **优先泛型 lambda 代替冗余的 Functor 类**：当行为简单且需要跨类型复用时，泛型 lambda 比手写 Functor 类更简洁。
2. **注意 `auto&&` 与 `auto` 的区别**：值传递会拷贝，引用传递用 `auto const&` 或 `auto&&`。对通用代码推荐 `auto const&` 或 `auto&&`。
3. **避免过度泛型化**：如果 lambda 只用于一种类型，显式类型更清晰，也能给出更好的编译错误信息。
4. **泛型 lambda 不能虚函数化**：闭包类型是唯一的匿名类型，其 `operator()` 模板不能声明为 `virtual`。
5. **与 `std::invoke` / `std::function` 配合时注意**：`std::function` 需要固定签名，泛型 lambda 不能直接存入 `std::function`，除非指定具体模板参数。
6. **C++20 简化**：C++20 允许 `auto` 作为普通函数参数（abbreviated function template），泛型 lambda 的语法优势相对缩小，但在 C++14/17 中仍是唯一途径。
