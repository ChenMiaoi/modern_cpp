# 模板元编程

## TMP 的起源

1994 年，Erwin Unruh 在 C++ 标准委员会上展示了一段不编译成功的程序——编译器错误信息中包含素数序列。这证明了 C++ 模板系统是图灵完备的——它实际上是一门在编译器中执行的函数式语言。

```cpp
template<int p, int i>
struct is_prime {
    enum { prim = (p == 2) || ((p % i) && is_prime<p, i - 1>::prim) };
};
template<int p>
struct is_prime<p, 1> { enum { prim = 1 }; };
```

## type_traits 基础

```cpp
static_assert(std::is_integral_v<int>);
static_assert(std::is_pointer_v<int*>);
using no_ref = std::remove_reference_t<int&>;    // int
static_assert(std::is_base_of_v<std::exception, std::runtime_error>);
// C++20: static_assert(std::movable<std::string>);
//        static_assert(std::three_way_comparable<double>);
```

## SFINAE

SFINAE（Substitution Failure Is Not An Error）：模板参数替换失败时，该重载被静默忽略。

```cpp
// 检测类型是否有 size() 成员
template <typename T, typename = void>
struct has_size : std::false_type {};
template <typename T>
struct has_size<T, std::void_t<decltype(std::declval<T>().size())>>
    : std::true_type {};

static_assert(has_size<std::vector<int>>::value);
static_assert(!has_size<int>::value);
```

## enable_if 与 void_t

```cpp
// enable_if: 条件为 true 时启用重载
template <typename T>
std::enable_if_t<std::is_integral_v<T>, T>
safe_add(T a, T b) {
    if (a > 0 && b > std::numeric_limits<T>::max() - a)
        throw std::overflow_error("overflow");
    return a + b;
}

// void_t: 将表达式合法性映射为类型约束
template <typename T, typename = void>
struct is_iterable : std::false_type {};
template <typename T>
struct is_iterable<T, std::void_t<
    decltype(std::declval<T>().begin()),
    decltype(std::declval<T>().end())>> : std::true_type {};
```

## constexpr if（C++17）

`if constexpr` 编译期求值条件，丢弃不满足的分支：

```cpp
template <typename T>
auto process(T value) {
    if constexpr (std::is_integral_v<T>) return value * 2;
    else if constexpr (std::is_floating_point_v<T>) return value + 0.5;
    else static_assert(always_false<T>::value, "unsupported type");
}
template <typename> struct always_false : std::false_type {};

// 关键优势：两个分支不需要同时合法
template <typename T>
void serialize(const T& obj) {
    if constexpr (requires { obj.serialize(); }) obj.serialize();
    else { /* 默认序列化 */ }
}
```

## Concepts（C++20）

将约束提升为一等公民，彻底改善模板编程的可读性和错误信息质量：

```cpp
template <typename T>
concept Hashable = requires(T a) {
    { std::hash<T>{}(a) } -> std::convertible_to<std::size_t>;
};

template <typename T>
concept Sortable = std::ranges::range<T>
    && requires(std::ranges::range_value_t<T>& a,
                std::ranges::range_value_t<T>& b) {
        { a < b } -> std::convertible_to<bool>;
    };

// 约束函数
template <Sortable Range>
void sort_and_print(Range& r) { std::ranges::sort(r); }

// 简写语法
void process(std::integral auto val) { /* ... */ }

// requires 子句
template <typename T>
concept Printable = requires(std::ostream& os, T val) {
    { os << val } -> std::same_as<std::ostream&>;
};
template <typename T> requires Printable<T> && std::movable<T>
void log(T&& value) { std::cout << value << '\n'; }
```

## 编译期分发模式

```cpp
// if constexpr（首选）
template <typename T> std::string type_name() {
    if constexpr (std::is_same_v<T, int>) return "int";
    else if constexpr (std::is_same_v<T, double>) return "double";
    else return "unknown";
}

// 模板特化
template <typename T> struct Serializer;
template <> struct Serializer<int> {
    static void write(std::ostream& os, int v) {
        os.write(reinterpret_cast<const char*>(&v), 4);
    }
};

// 概念 + 重载（C++20）
std::string describe(std::integral auto v) { return "integer"; }
std::string describe(std::floating_point auto v) { return "float"; }
std::string describe(const std::ranges::range auto&) { return "range"; }
```

## Policy-Based Design（Alexandrescu）

策略设计模式通过模板参数组合行为，在编译期零开销地构建灵活组件：

```cpp
struct SingleThreaded { struct Lock { explicit Lock(...) {} }; };
template <typename T>
struct CreateNew { static T* Create() { return new T; } };

template <typename T, typename ThreadingModel = SingleThreaded,
          template<typename> class CreationPolicy = CreateNew>
class SmartPtr {
    T* pointee_;
public:
    SmartPtr() : pointee_(CreationPolicy<T>::Create()) {}
    T* operator->() {
        typename ThreadingModel::Lock guard(/* ... */);
        return pointee_;
    }
};
```

## C++26 反射

C++26 引入编译期反射（P2996），允许在编译期检查和操控程序结构：
```cpp
// 编译期生成序列化代码
template <typename T>
std::string to_json(const T& obj) {
    std::string result = "{";
    bool first = true;
    template for (constexpr auto mem : std::meta::members_of(^T)) {
        if (!first) result += ", "; first = false;
        result += "\"" + std::string(std::meta::name_of(mem)) + "\"";
        result += ": " + serialize(obj.[:mem:]);
    }
    return result + "}";
}
```

