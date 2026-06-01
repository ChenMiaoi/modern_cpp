# 模板机制术语

## SFINAE（Substitution Failure Is Not An Error）

模板参数替换失败时，编译器不报错，而是把这个重载从候选集中移除：

```cpp
// enable_if: 条件为 false 时，type 不存在 → 替换失败 → 这个重载被忽略
template<typename T>
typename std::enable_if<std::is_integral<T>::value, T>::type
safe_divide(T a, T b) { return b != 0 ? a / b : 0; }

template<typename T>
typename std::enable_if<std::is_floating_point<T>::value, T>::type
safe_divide(T a, T b) { return b != 0.0 ? a / b : 0.0; }
```

SFINAE 是 C++20 之前的模板约束核心机制——丑陋但有效。

## CRTP（Curiously Recurring Template Pattern）

基类的模板参数是派生类本身——实现编译期多态：

```cpp
template<typename Derived>
class Base {
public:
  void interface() {
    static_cast<Derived*>(this)->implementation();  // 编译期分发
  }
};

class MyClass : public Base<MyClass> {
public:
  void implementation() { /* 具体实现 */ }
};
```

CRTP 的优势：零运行时开销（没有虚函数表），编译器可以内联 `implementation()`。

## CTAD（Class Template Argument Deduction，C++17）

编译器从构造函数推导类模板参数：

```cpp
// C++11/14：必须写模板参数
std::pair<int, double> p1(42, 3.14);
auto p2 = std::make_pair(42, 3.14);

// C++17：直接推导
std::pair p3(42, 3.14);  // CTAD: pair<int, double>
std::vector v{1, 2, 3};  // CTAD: vector<int>
```

### Deduction Guide（推导指引）

当自动推导不够时，可以显式定义推导规则：

```cpp
template<typename T>
struct MyContainer {
  MyContainer(std::initializer_list<T>);
};

// 推导指引：从 initializer_list 推导 T
template<typename T>
MyContainer(std::initializer_list<T>) -> MyContainer<T>;
```

## Concept（C++20）

对模板参数的命名约束，取代 SFINAE 黑魔法：

```cpp
template<typename T>
concept Hashable = requires(T a) {
  { std::hash<T>{}(a) } -> std::convertible_to<std::size_t>;
};

template<Hashable T>
void process(T value) { ... }  // 清晰、可读、报错信息友好
```

### Subsumption（概念包含）

C++20 Concepts 之间的包含关系——更严格的约束优先：

```cpp
template<typename T> concept C = requires { typename T::type; };
template<typename T> concept D = C<T> && requires { typename T::value_type; };

void f(C auto);  // 通用版本
void f(D auto);  // 更特化的版本——当 D 满足时优先选择
```

## Variadic Template 与 Parameter Pack

```cpp
template<typename... Args>
void print(Args&&... args) {
  (std::cout << ... << args) << "\n";  // 折叠表达式 (C++17)
}
```

### Fold Expression（折叠表达式，C++17）

```cpp
template<typename... Args>
auto sum(Args... args) {
  return (args + ...);  // 一元右折叠
  // 等价于: arg1 + (arg2 + (arg3 + ...))
}
```

## Expression Template（表达式模板）

延迟计算的模板技术，避免中间临时对象：

```cpp
// a = b * c + d * e
// 朴素实现：3 个临时对象
// 表达式模板：0 个临时对象——整个表达式树在赋值时一次性计算

// b * c 返回 mul_expr<B, C>——不执行乘法
// +   返回 add_expr<mul_expr1, mul_expr2>——不执行加法
// =   展开整个表达式，直接计算到 a 中
```

详见 [Boost.Multiprecision](/libraries/algorithms/multiprecision) 中的表达式模板实现。

## Template Template Parameter

模板的模板参数——让一个模板接受另一个模板作为参数：

```cpp
template<typename T, template<typename> class Container>
struct Holds {
  Container<T> data;
};

Holds<int, std::vector> h;  // Container = std::vector
Holds<int, std::list> h2;   // Container = std::list
```
