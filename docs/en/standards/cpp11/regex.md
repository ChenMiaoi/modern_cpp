---
title: "std::regex Regular Expressions"
topic: unknown
feature: regex
standard: N/A
status_checked_at: 2026-06-02
---
# std::regex Regular Expressions

## Overview

C++11 introduced the `<regex>` header, providing standardized regular expression support. `std::regex` uses ECMAScript syntax by default (highly compatible with JavaScript regex), while also supporting POSIX, awk, grep, and other dialects. The library includes pattern matching (`regex_match`), searching (`regex_search`), replacement (`regex_replace`), and iterator interfaces.

> **Performance warning:** C++ standard library `std::regex` implementations are typically much slower than specialized regex libraries (like RE2, PCRE2). In performance-sensitive scenarios, consider using a third-party library.

## API Overview

| Component | Description |
|-----------|-------------|
| `std::regex` | Compiled regular expression object |
| `std::smatch` / `std::cmatch` | Match result container (string / C string) |
| `std::regex_match` | Full match (entire string must match) |
| `std::regex_search` | Substring search (finds first match) |
| `std::regex_replace` | Replace matched content |
| `std::regex_iterator` | Iterate over all matches |
| `std::regex_token_iterator` | Iterate over specific sub-expressions or delimited segments |

## Basic Usage

```cpp
#include <iostream>
#include <string>
#include <regex>

int main() {
    std::regex pattern(R"(\d{3}-\d{4})");  // three digits-four digits
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

// regex_match: the entire string must match
std::regex_match("user@example.com", email_re);              // true
std::regex_match("Send to user@example.com", email_re);      // false

// regex_search: finds a substring match within the string
std::regex_search("Send to user@example.com", email_re);     // true
```

**Core difference:** `regex_match` requires the entire input to match the pattern; `regex_search` only needs to find a matching substring in the input.

## Capture Groups

```cpp
std::regex date_re(R"((\d{4})-(\d{2})-(\d{2}))");
std::string text = "Date: 2024-01-15, another: 2023-12-25";

std::smatch m;
if (std::regex_search(text, m, date_re)) {
    // m[0] is the full match, m[1], m[2], m[3] are capture groups
    std::string year  = m[1].str();  // "2024"
    std::string month = m[2].str();  // "01"
    std::string day   = m[3].str();  // "15"
    // m.prefix() is the part before the match, m.suffix() is the part after
}
```

## regex_replace

```cpp
std::string input = "Price: $100, Discount: $20";
std::regex dollar_re(R"(\$(\d+))");

std::string all = std::regex_replace(input, dollar_re, "¥$1");
// "Price: ¥100, Discount: ¥20" (replaces all matches)

std::string first = std::regex_replace(input, dollar_re, "[$1]",
    std::regex_constants::format_first_only);
// "Price: [100], Discount: $20" (replaces only the first)
```

## regex_iterator: Iterating Over All Matches

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

## regex_token_iterator: Splitting by Delimiter

```cpp
std::string csv = "apple,banana,,cherry";
std::regex comma_re(",");

// submatch index -1: takes the parts between matches
for (auto it = std::sregex_token_iterator(csv.begin(), csv.end(), comma_re, -1);
     it != std::sregex_token_iterator(); ++it) {
    std::cout << "[" << *it << "] ";
}
// [apple] [banana] [] [cherry]
```

Extracting specific capture groups:

```cpp
std::string html = "<b>bold</b> and <i>italic</i>";
std::regex tag_re(R"(<(\w+)>(.+?)</\1>)");

for (auto it = std::sregex_token_iterator(html.begin(), html.end(), tag_re, 2);
     it != std::sregex_token_iterator(); ++it) {
    std::cout << *it << '\n';  // "bold", "italic"
}
```

## Common Regex Patterns

```cpp
std::regex email_re(R"([\w.+-]+@[\w-]+\.[\w.]+)");
std::regex ipv4_re(R"((\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3}))");
std::regex url_re(R"(https?://[\w.-]+(?:/[\w./?#&=%-]*)?)");

// Trim leading/trailing whitespace
std::string trimmed = std::regex_replace(
    std::string("  hello  "), std::regex(R"(^\s+|\s+$)"), "");
```

## Regex Flags

```cpp
std::regex re1(R"(\d+)", std::regex::ECMAScript);  // default dialect
std::regex re2(R"(hello)", std::regex::icase);      // case-insensitive
std::regex_search("HELLO", re2);  // true

// Multiline mode (standardized in C++17)
std::regex re3(R"(^\d+$)", std::regex::ECMAScript | std::regex::multiline);
```

## Performance Notes

```cpp
// Wrong: repeated compilation in a loop — enormous overhead
for (const auto& s : vec) {
    std::regex re(R"(\d+)");
    std::regex_search(s, re);
}

// Correct: compile once, reuse many times
static const std::regex re(R"(\d+)");
for (const auto& s : vec) { std::regex_search(s, re); }
```

> **C++26 compile-time regex:** Future standards are expected to introduce `constexpr` regex, eliminating runtime construction overhead.

## Best Practices

1. **Compile regex only once**: `std::regex` construction has significant overhead; avoid repeated construction in loops.
2. **Prefer `regex_search` over `regex_match`**: Unless you truly need a full match.
3. **Use raw strings `R"()"`**: Avoids backslash escaping hell.
4. **Use capture groups judiciously**: Use `(?:...)` non-capturing groups to reduce overhead.
5. **Prefer simple string operations**: For fixed substring lookups, use `std::string::find`.

## Common Pitfalls

- **Default dialect is ECMAScript**: Not POSIX, not PCRE.
- **Backtracking causes performance disasters**: Nested quantifiers like `(a+)+` can cause exponential backtracking.
- **Invalid regex throws exception**: If the pattern is invalid at construction time, `std::regex_error` is thrown.
- **`<regex` compiles slowly**: The header is large, significantly increasing compilation time.
- **Implementation quality varies**: GCC's libstdc++ before 4.8 had an incomplete implementation. Consider RE2 for production.
- **Limited Unicode support**: Typically not as comprehensive as Python or PCRE.
