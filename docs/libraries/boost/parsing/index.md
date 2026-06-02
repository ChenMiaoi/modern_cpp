---
title: "Boost 解析与文本"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# Boost 解析与文本

## Spirit.X3：PEG 解析器

Spirit.X3 基于 PEG（Parsing Expression Grammar），通过 C++ 运算符重载和表达式模板在编译期构造解析器对象。

### 核心类型与组合子

```cpp
namespace x3 = boost::spirit::x3;

auto const digit   = x3::char_('0', '9');
auto const integer = x3::int_;
auto const op      = x3::char_("+-*/");
auto const expr    = integer >> op >> integer;  // sequence<A, B>

// 运算符重载的类型推导：
// a >> b  →  sequence<A, B>     （顺序组合）
// a | b   →  alternative<A, B>  （有序选择）
// *a      →  kleene<A>          （零次或多次）
// +a      →  plus<A>            （一次或多次）
```

### 递归规则与语义动作

```cpp
x3::rule<class expression, ast::expression> const expression = "expression";
x3::rule<class term, ast::term>             const term       = "term";
x3::rule<class factor, ast::factor>         const factor     = "factor";

auto const expression_def =
    term >> *('+' >> term | '-' >> term);

auto const factor_def =
    x3::double_
    | '(' >> expression >> ')'        // 递归引用
    | '-' >> factor
;

BOOST_SPIRIT_DEFINE(expression, term, factor)
```

语义动作在解析过程中直接执行代码：

```cpp
auto const expression_def =
    term >> *(
        ('+' >> term)[[](auto& ctx) {
            auto& left  = x3::_val(ctx);      // 当前 rule 的属性
            auto& right = x3::_attr(ctx);      // 子解析器的属性
            left = ast::add{left, right};
        }]
    );
```

### 编译时间代价

X3 的主要缺点是编译时间。表达式模板的深层嵌套导致数千个模板实例化。建议将大文法拆分为多个 `rule`。

---

## Format：格式化（已被 fmt 取代）

Boost.Format 是 printf 风格的类型安全格式化库。已被 fmt 库（C++20 `std::format` 的参考实现）全面超越：

```cpp
// Boost.Format（旧）
std::string s = boost::format("Hello, %1%! You have %2% messages.") % name % count;

// fmt/std::format（新，推荐）
std::string s = fmt::format("Hello, {}! You have {} messages.", name, count);
```

---

## Tokenizer：分词器

将字符串按分隔符拆分为 token 序列：

```cpp
std::string s = "Hello,World,Foo,Bar";
boost::tokenizer<boost::char_separator<char>> tok(s, boost::char_separator<char>(","));
for (const auto& t : tok) {
    std::cout << t << "\n";  // Hello, World, Foo, Bar
}
```

---

## Locale：本地化

Boost.Locale 提供 ICU 后端的 Unicode 感知本地化，比 `std::locale` 更完整：大小写转换、排序、数字/日期格式化、字符编码转换。
