# std::regex 正则表达式

## 概述

C++11 引入了 `<regex>` 头文件，提供标准化的正则表达式支持。`std::regex` 默认使用 ECMAScript 语法（与 JavaScript 正则高度兼容），同时支持 POSIX、awk、grep 等方言。该库包含模式匹配（`regex_match`）、搜索（`regex_search`）、替换（`regex_replace`），以及迭代器接口。

> **性能警告：** C++ 标准库的 `std::regex` 实现通常比专用正则库（如 RE2、PCRE2）慢得多。在性能敏感场景下，考虑使用第三方库。

## API 概览

| 组件 | 说明 |
|------|------|
| `std::regex` | 编译后的正则表达式对象 |
| `std::smatch` / `std::cmatch` | 匹配结果容器（string / C 字符串） |
| `std::regex_match` | 完整匹配（整个字符串必须匹配） |
| `std::regex_search` | 子串搜索（找到第一个匹配即可） |
| `std::regex_replace` | 替换匹配内容 |
| `std::regex_iterator` | 遍历所有匹配 |
| `std::regex_token_iterator` | 遍历特定子表达式或分隔段 |

## 基本用法

```cpp
#include <iostream>
#include <string>
#include <regex>

int main() {
    std::regex pattern(R"(\d{3}-\d{4})");  // 三位数字-四位数字
    std::string text = "Call 555-1234 or 555-5678";

    std::smatch match;
    if (std::regex_search(text, match, pattern)) {
        std::cout << "Found: " << match[0] << '\n';  // 555-1234
    }
}
```

## regex_match vs regex_search

```cpp
std::regex email_re(R"((\w+)@(\w+)\.(\w+))");

// regex_match：必须完全匹配整个字符串
std::regex_match("user@example.com", email_re);              // true
std::regex_match("Send to user@example.com", email_re);      // false

// regex_search：在字符串中查找子串匹配
std::regex_search("Send to user@example.com", email_re);     // true
```

**核心区别：** `regex_match` 要求整个输入都匹配模式，`regex_search` 只需在输入中找到匹配子串。

## 捕获组

```cpp
std::regex date_re(R"((\d{4})-(\d{2})-(\d{2}))");
std::string text = "Date: 2024-01-15, another: 2023-12-25";

std::smatch m;
if (std::regex_search(text, m, date_re)) {
    // m[0] 是整个匹配，m[1], m[2], m[3] 是捕获组
    std::string year  = m[1].str();  // "2024"
    std::string month = m[2].str();  // "01"
    std::string day   = m[3].str();  // "15"
    // m.prefix() 匹配前部分，m.suffix() 匹配后部分
}
```

## regex_replace

```cpp
std::string input = "Price: $100, Discount: $20";
std::regex dollar_re(R"(\$(\d+))");

std::string all = std::regex_replace(input, dollar_re, "¥$1");
// "Price: ¥100, Discount: ¥20"（替换所有匹配）

std::string first = std::regex_replace(input, dollar_re, "[$1]",
    std::regex_constants::format_first_only);
// "Price: [100], Discount: $20"（只替换第一个）
```

## regex_iterator 遍历所有匹配

```cpp
std::string text = "2024-01-15 and 2023-12-25";
std::regex date_re(R"((\d{4})-(\d{2})-(\d{2}))");

for (auto it = std::sregex_iterator(text.begin(), text.end(), date_re);
     it != std::sregex_iterator(); ++it) {
    const std::smatch& m = *it;
    std::cout << m[0] << " -> Y:" << m[1] << " M:" << m[2] << '\n';
}
// 2024-01-15 -> Y:2024 M:01
// 2023-12-25 -> Y:2023 M:12
```

## regex_token_iterator 按分隔符分割

```cpp
std::string csv = "apple,banana,,cherry";
std::regex comma_re(",");

// submatch index -1：取匹配之间的部分
for (auto it = std::sregex_token_iterator(csv.begin(), csv.end(), comma_re, -1);
     it != std::sregex_token_iterator(); ++it) {
    std::cout << "[" << *it << "] ";
}
// [apple] [banana] [] [cherry]
```

提取特定捕获组：

```cpp
std::string html = "<b>bold</b> and <i>italic</i>";
std::regex tag_re(R"(<(\w+)>(.+?)</\1>)");

for (auto it = std::sregex_token_iterator(html.begin(), html.end(), tag_re, 2);
     it != std::sregex_token_iterator(); ++it) {
    std::cout << *it << '\n';  // "bold", "italic"
}
```

## 常用正则模式

```cpp
std::regex email_re(R"([\w.+-]+@[\w-]+\.[\w.]+)");
std::regex ipv4_re(R"((\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3}))");
std::regex url_re(R"(https?://[\w.-]+(?:/[\w./?#&=%-]*)?)");

// trim 前后空白
std::string trimmed = std::regex_replace(
    std::string("  hello  "), std::regex(R"(^\s+|\s+$)"), "");
```

## regex 标志

```cpp
std::regex re1(R"(\d+)", std::regex::ECMAScript);  // 默认方言
std::regex re2(R"(hello)", std::regex::icase);      // 忽略大小写
std::regex_search("HELLO", re2);  // true

// 多行模式（C++17 标准化）
std::regex re3(R"(^\d+$)", std::regex::ECMAScript | std::regex::multiline);
```

## 性能注意事项

```cpp
// 错误：循环中重复编译——开销巨大
for (const auto& s : vec) {
    std::regex re(R"(\d+)");
    std::regex_search(s, re);
}

// 正确：编译一次，复用多次
static const std::regex re(R"(\d+)");
for (const auto& s : vec) { std::regex_search(s, re); }
```

> **C++26 编译期正则：** 未来标准有望引入 `constexpr` 正则，消除运行时构造开销。

## 最佳实践

1. **正则只编译一次**：`std::regex` 构造有显著开销，避免循环中重复构造。
2. **优先 `regex_search` 而非 `regex_match`**：除非确实需要完整匹配。
3. **使用原始字符串 `R"()"`**：避免反斜杠转义地狱。
4. **合理使用捕获组**：用 `(?:...)` 非捕获组减少开销。
5. **简单字符串操作优先**：固定子串查找用 `std::string::find`。

## 常见陷阱

- **默认方言是 ECMAScript**：不是 POSIX，不是 PCRE。
- **回溯导致性能灾难**：嵌套量词如 `(a+)+` 可能导致指数级回溯。
- **无效正则抛异常**：构造时若模式无效，抛出 `std::regex_error`。
- **`<regex>` 编译慢**：头文件较大，显著增加编译时间。
- **实现质量参差不齐**：GCC 4.8 前 libstdc++ 未完成实现。生产环境考虑 RE2。
- **Unicode 支持有限**：通常不如 Python 或 PCRE 完善。
