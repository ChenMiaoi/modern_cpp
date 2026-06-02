---
title: "Boost 解析与文本"
topic: unknown
feature: index
standard: N/A
status_checked_at: 2026-06-02
---
# Boost Parsing and Text

## Spirit.X3: PEG Parser

Spirit.X3 is based on PEG (Parsing Expression Grammar), constructing parser objects at compile time through C++ operator overloading and expression templates.

### Core Types and Combinators

```cpp
namespace x3 = boost::spirit::x3;

auto const digit   = x3::char_('0', '9');
auto const integer = x3::int_;
auto const op      = x3::char_("+-*/");
auto const expr    = integer >> op >> integer;  // sequence<A, B>

// Type deduction from operator overloading:
// a >> b  →  sequence<A, B>     (sequence)
// a | b   →  alternative<A, B>  (ordered choice)
// *a      →  kleene<A>          (zero or more)
// +a      →  plus<A>            (one or more)
```

### Recursive Rules and Semantic Actions

```cpp
x3::rule<class expression, ast::expression> const expression = "expression";
x3::rule<class term, ast::term>             const term       = "term";
x3::rule<class factor, ast::factor>         const factor     = "factor";

auto const expression_def =
    term >> *('+' >> term | '-' >> term);

auto const factor_def =
    x3::double_
    | '(' >> expression >> ')'        // recursive reference
    | '-' >> factor
;

BOOST_SPIRIT_DEFINE(expression, term, factor)
```

Semantic actions execute code directly during parsing:

```cpp
auto const expression_def =
    term >> *(
        ('+' >> term)[[](auto& ctx) {
            auto& left  = x3::_val(ctx);      // attribute of the current rule
            auto& right = x3::_attr(ctx);      // attribute of the sub-parser
            left = ast::add{left, right};
        }]
    );
```

### Compile-Time Cost

The main drawback of X3 is compile time. Deeply nested expression templates lead to thousands of template instantiations. It is recommended to split large grammars into multiple `rule`s.

---

## Format: Formatting (Superseded by fmt)

Boost.Format is a printf-style type-safe formatting library. It has been thoroughly surpassed by the fmt library (the reference implementation of C++20 `std::format`):

```cpp
// Boost.Format (old)
std::string s = boost::format("Hello, %1%! You have %2% messages.") % name % count;

// fmt/std::format (new, recommended)
std::string s = fmt::format("Hello, {}! You have {} messages.", name, count);
```

---

## Tokenizer

Splits a string into a sequence of tokens by delimiters:

```cpp
std::string s = "Hello,World,Foo,Bar";
boost::tokenizer<boost::char_separator<char>> tok(s, boost::char_separator<char>(","));
for (const auto& t : tok) {
    std::cout << t << "\n";  // Hello, World, Foo, Bar
}
```

---

## Locale

Boost.Locale provides Unicode-aware localization with an ICU backend, more complete than `std::locale`: case conversion, collation, number/date formatting, and character encoding conversion.
