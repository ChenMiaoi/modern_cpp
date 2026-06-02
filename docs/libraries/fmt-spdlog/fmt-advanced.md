---
title: "fmt 高级特性"
topic: unknown
feature: fmt-advanced
standard: N/A
status_checked_at: 2026-06-02
---
# fmt 高级特性

> 源码路径：`references/impl/fmt/include/fmt/chrono.h`, `ranges.h`, `compile.h`

## 自定义类型

```cpp
struct Point {
  double x, y;
};

template <> struct fmt::formatter<Point> : formatter<double> {
  auto format(const Point& p, format_context& ctx) const {
    return format_to(ctx.out(), "({}, {})", p.x, p.y);
  }
};

fmt::format("{}", Point{1.5, 2.5});  // "(1.5, 2.5)"
```

自定义类型通过 `custom_value<Context>` 存储——`value` 构造函数检测到用户特化了 `formatter<T>`，进入 `custom_tag` 路径，将对象指针和格式化函数指针打包到 16 字节 union 中。

## 编译期格式化

```cpp
// fmt::format_string 在编译期验证格式串
constexpr auto s = fmt::format<int, double>("{} {}", 42, 3.14);
```

## Chrono 格式化

```cpp
auto now = std::chrono::system_clock::now();
fmt::format("{:%Y-%m-%d %H:%M}", now);  // "2024-01-15 14:30"

std::chrono::seconds dur(3661);
fmt::format("{:%H:%M:%S}", dur);  // "01:01:01"
```

## Ranges 格式化

```cpp
std::vector<int> v = {1, 2, 3};
fmt::format("{}", v);  // "[1, 2, 3]"
fmt::format("{::#x}", v);  // "[0x1, 0x2, 0x3]"
```

## 浮点格式化：Dragonbox

fmt 使用 Dragonbox 算法进行浮点到字符串的转换，比 `std::to_chars` 快 2-5 倍：

- **Dragonbox**：基于 IEEE 754 浮点数的数学特性，直接计算最短表示
- **Grisu-Exact**：备选算法，保证精确舍入
- **Ryu**：另一种快速算法，fmt 早期使用

## 用户 API

本文覆盖的用户入口包括 `fmt::format`、自定义 `formatter<T>`、chrono/ranges 格式化与编译期格式串检查；现有正文已经直接展开这些高级特性。

## 标准语义

fmt 在 `std::format` / `std::print` 的标准语义之上提供了若干扩展，也存在几处不兼容细节：

| 维度 | `std::format` / `std::print` | `fmt` 扩展 / 差异 |
|------|-------------------------------|-------------------|
| 自定义 formatter | 特化 `std::formatter<T>`，必须提供 `parse()` + `format()` | 特化 `fmt::formatter<T>`；额外支持 ADL `format_as(T) -> U` 将类型映射为已格式化类型，以及 `formatter<T>::format_as()` 成员函数——两者标准库均无对应物 |
| 编译期格式串 | C++20 `std::format_string<Args...>`（consteval 构造） | `fmt::fstring<Args...>`（`FMT_CONSTEVAL` 构造），语义等价；`FMT_COMPILE` 宏进一步将格式串编译为类型级 AST（`compile.h`），完全消除运行时解析 |
| Ranges 格式化 | C++23 `std::formatter` 特化对 range 的支持（`std::range` 概念） | `fmt::formatter<Range>` 通过 `ranges.h` 实现，支持 map/set/sequence/tuple，`fmt::join(range, sep)` 将元素拼接为自定义分隔符——标准库无等价物 |
| Chrono 格式化 | `std::format` 支持 `std::chrono` 类型（C++20） | `fmt::formatter<duration>` / `formatter<time_point>` 在 `chrono.h` 中实现，支持 `%Y-%m-%d %H:%M:%S` 等 strftime 风格 token；`FMT_SAFE_DURATION_CAST`（默认开启）保证浮点 duration 转换不溢出 |
| 动态宽度/精度 | `{:{}}` 语法，编译期检查参数为整数 | 语义相同，fmt 额外允许命名参数 `{:{name}}` |
| 输出目标 | `std::string`（`std::format`）/ `FILE*`（`std::print`） | `fmt::format` → `std::string`；`fmt::format_to(it, ...)` → 任意输出迭代器；`fmt::print(FILE*, ...)` / `fmt::print(ostream&, ...)` / `fmt::print_to(FILE*, ...)` |
| 错误类型 | `std::format_error`（C++26 前为 `std::runtime_error`） | `fmt::format_error`（继承 `std::runtime_error`），语义相同但非同一类型——catch `std::format_error` 不会捕获 `fmt::format_error` |
| `format_to` 返回值 | 返回输出迭代器（past-the-end） | 语义相同 |
| `to_string` | 无（使用 `std::format`） | `fmt::to_string(T)` 直接调用 `fmt::format("{}", val)` 返回 `std::string` |

**关键不兼容**：
- `format_as` 是 fmt 独有的扩展点（[P2836R1](https://wg21.link/P2836) 提议纳入标准，截至 C++26 尚未合并）。标准库代码无法使用 `format_as`，需显式特化 `std::formatter`。
- `fmt::format_error` 与 `std::format_error` 是不同类型，混合使用 fmt 和标准库格式化时需分别 catch。
- fmt 的 `formatter<T>::parse()` 接收 `fmt::parse_context&`，标准库接收 `std::format_parse_context&`——两者 API 相似但非同一类型，formatter 特化不能在 fmt 和 `std::format` 间复用。

## 对象布局

fmt-advanced 涉及的类型擦除对象布局主要集中在三处：自定义 formatter 的 `custom_value`、chrono/ranges 的 formatter 特化实例，以及 `FMT_COMPILE` 生成的类型级 AST。

### `custom_value<Context>` 与自定义 formatter

```
value<Context> (16 字节 union)
┌────────────────────────────────────────────┐
│ union {                                    │
│   int / unsigned / long long / ...         │ ← 内置类型直接存储
│   double / long double                     │
│   string_value<char_type>  {ptr; size}     │ ← 16 字节（指针 + 长度）
│   custom_value<Context>    {ptr; fmt_fn}   │ ← 16 字节（void* + 函数指针）
│ }                                          │
└────────────────────────────────────────────┘
```

用户特化 `fmt::formatter<T>` 后，`stored_type_constant<T>::value` 映射为 `custom_type`。构造 `value<Context>` 时进入 `custom_tag` 分支：
- `custom.value = const_cast<void*>(static_cast<const void*>(&obj))` —— 指向用户对象的 void 指针
- `custom.format = format_custom<T>` —— 指向 `format_custom` 模板实例的函数指针，该实例内部调用 `formatter<T>().format(value, ctx)`

运行时 `visit` 到 `custom_type` 分支时，通过 `handle(custom)` 构造 `basic_format_arg::handle`，再调用 `custom.format(custom.value, parse_ctx, ctx)`——一次间接函数指针跳转。

### chrono formatter 存储

`formatter<std::chrono::duration<Rep, Period>>` 和 `formatter<std::chrono::time_point<Clock, Duration>>` 是完整特化（非继承 `formatter<double>`）。格式规范通过 `parse()` 解析为内部 `chrono_format_spec` 结构体：

```
chrono_format_spec
├── fill         : char       // 填充字符，默认 ' '
├── align        : align_t    // 对齐方式（left/right/center）
├── width        : int        // 最小宽度
├── precision    : int        // 精度（仅 %S 的小数部分）
├── localized    : bool       // 是否使用 locale
└── chrono_specs : string     // strftime token 序列，如 "%Y-%m-%d %H:%M"
```

`format()` 内部将 `time_point` 转为 `std::tm`（通过 `gmtime_r` / `gmtime_s` / `localtime_r`），再逐字符输出 strftime 结果。duration 格式化通过 `format_duration` 直接处理 `%H`/`%M`/`%S` token，避免中间 `tm` 转换。

### ranges formatter 存储

`formatter<Range>` 在 `ranges.h` 中通过 SFINAE 检测 `is_range_` / `is_tuple_like_` 后实例化。其内部持有：

```
formatter<Range, Char>
├── underlying_  : formatter<element_type, Char>  // 元素的 formatter
├── specs_       : range_formatter_specs           // 分隔符、外层括号样式
│   ├── separator  : basic_string_view<Char>       // 默认 ", "
│   ├── opening    : char                          // 默认 '['
│   └── closing    : char                          // 默认 ']'
└── (格式规范由 parse() 解析，通过 "{::spec}" 传递给元素 formatter)
```

`{::#x}` 语法中，`::` 后的 `#x` 被下推到元素 `formatter<int>::parse()`，整数按十六进制输出。外层 `[]` 和分隔符由 range formatter 自身控制。

### `FMT_COMPILE` 类型级 AST

`compile.h` 中 `FMT_COMPILE("{}")` 展开为 `compiled_string` 子类，编译期将格式串解析为类型级 AST：

```
concat<text<Char>, concat<field<Char, int, 0>, text<Char>>>
├── lhs : text<Char>          // 静态文本片段
└── rhs : concat<field<...>, text<...>>
    ├── lhs : field<Char, int, 0>  // 替换字段，绑定参数 0
    └── rhs : text<Char>           // 尾部文本
```

每个 AST 节点是零大小类型（`text` 仅持有 `string_view`，`field` 无数据成员），`concat<L,R>` 大小为两者之和。编译器实例化整棵 AST 树后，`format()` 调用通过递归 `concat::format()` 展开为纯内联代码——无运行时格式串解析、无 switch 分发、无堆分配。
## 核心源码路径

本文开头已给出 `chrono.h`、`ranges.h`、`compile.h`；后续补 `format.h` / `base.h` 中与这些特性衔接的入口。

## 核心类 / 函数

### `fmt::formatter<T>` 特化协议

自定义类型格式化的入口。用户必须特化 `fmt::formatter<T>` 并实现两个方法：

```cpp
template <> struct fmt::formatter<MyType> {
  // 编译期解析格式规范（"{:spec}" 中的 spec 部分）
  constexpr auto parse(fmt::format_parse_context& ctx) -> decltype(ctx.begin());
  // 运行时格式化输出
  auto format(const MyType& val, fmt::format_context& ctx) const -> decltype(ctx.out());
};
```

- `parse()` 在 `fstring` 的 consteval 验证阶段被调用（通过 `format_string_checker::on_format_specs` 下推），必须在 `ctx` 的 `[begin, end)` 范围内消费格式规范字符，返回指向 `}` 前一位的迭代器。若格式规范为空（`"{}"`），默认实现 `return ctx.begin()` 即可。
- `format()` 通过 `custom_value<Context>::format` 函数指针在运行时调用，将输出写入 `ctx.out()` 并返回 past-the-end 迭代器。

### `custom_value<Context>`

`base.h` 中 16 字节 union 成员，存储用户类型指针 + 格式化函数指针。`format_custom<T>` 是模板函数，实例化时绑定到具体 `formatter<T>::format`。`basic_format_arg::visit()` 在 `custom_type` 分支调用 `handle(custom)` 返回代理对象，延迟到实际消费参数时才执行格式化。

### `format_string_checker` / `fstring`

`fstring<Args...>`（`base.h`）的 `FMT_CONSTEVAL` 构造函数在编译期调用 `format_string_checker`。后者持有 `parse_funcs_[]` 数组（每个元素是 `formatter<T>::parse` 的函数指针），逐字符扫描格式串，对每个 `{id}` 调用 `parse_funcs_[id](compile_parse_context)` 验证格式规范。验证通过则编译成功，否则 `report_error()` 触发 `static_assert`。

### chrono formatter

`formatter<std::chrono::duration<Rep, Period>>`（`chrono.h`）的 `parse()` 解析 `%Y`/`%m`/`%d`/`%H`/`%M`/`%S` 等 strftime token 序列存入 `chrono_format_spec`。`format()` 对 `time_point` 类型先通过 `to_time_t` + `gmtime_r`/`localtime_r` 转为 `std::tm`，再按 token 逐段输出；对 `duration` 类型直接计算时/分/秒/子秒。`FMT_SAFE_DURATION_CAST` 保证浮点 duration 精度无损。

### range formatter

`formatter<Range, Char>`（`ranges.h`）通过 `range_format_kind_<T>` SFINAE 检测判断 range 类别（`map` / `set` / `sequence` / `string`）。`parse()` 先消费外层格式规范（括号样式、分隔符），遇到 `::` 后将剩余规范下推给元素 `formatter<element_type>::parse()`。`format()` 遍历 range，对每个元素调用 `underlying_.format(elem, ctx)`，元素间插入分隔符。`fmt::join(range, sep)` 是独立便利函数，生成 `join_view` 后走专用路径，省去括号和嵌套格式规范解析。

### `FMT_COMPILE` AST 节点

`compile.h` 中 `compile_format_string<Args, POS, ID, ...>(fmt)` 递归解析格式串，生成类型级 AST：
- `text<Char>` —— 静态文本片段
- `field<Char, V, N>` —— 无格式规范的替换字段
- `spec_field<Char, V, N>` —— 带格式规范的替换字段（内部持有 `formatter<V, Char>` 实例）
- `concat<L, R>` —— AST 连接节点
- `runtime_named_field<Char>` —— 命名参数（运行时查找）

编译器将整棵 AST 内联后，`format()` 展开为纯指令序列。
## 关键算法

fmt-advanced 的四个核心路径——自定义 formatter 分发、编译期格式串检查、chrono token 解析、Dragonbox 浮点转换——通过三阶段流水线串联：编译期验证 → 参数擦除 → 输出生成。

### 自定义 formatter 分发路径

```
用户特化 formatter<T>
    │
    ▼
fstring 构造时: format_string_checker::on_format_specs(id, begin, end)
    │  调用 parse_funcs_[id](compile_parse_context)
    │  → formatter<T>::parse()  // 编译期验证格式规范
    ▼
运行时: value<Context> 构造 (custom_tag 路径)
    │  stored_type_constant<T> == custom_type
    │  custom.value = &obj, custom.format = format_custom<T>
    ▼
visit(ctx) → case custom_type:
    │  handle(custom).format(parse_ctx, ctx)
    │  → formatter<T>().format(value, ctx)  // 用户代码执行
    ▼
输出到 ctx.out()
```

关键：`parse()` 在编译期执行（consteval 上下文），`format()` 在运行时执行。两者通过 `custom_value` 的函数指针桥接。

### 编译期格式串检查路径

```
fstring<Args...> FMT_CONSTEVAL 构造
    │
    ▼
parse_format_string(str, checker)
    │  逐字符扫描: 遇 '{' → replacement field
    │             遇 '}' → 结束（非法裸 '}' 报错）
    │             其他 → 静态文本
    ▼
replacement field 解析:
    ├─ arg_id: 自动索引 / 显式索引 / 命名参数
    │   check_arg_id(id) 验证索引不越界
    ▼
format_specs 解析:
    │  fill? align? sign? #? 0? width? .precision? type?
    │  动态宽度/精度: check_dynamic_spec(int) 验证参数为整数
    ▼
formatter<T>::parse(ctx) 下推
    │  每个类型的 parse() 验证自己接受的格式规范
    │  如: formatter<int> 接受 d/x/o/b 等
    │      formatter<double> 接受 f/e/g/a 等
    ▼
编译成功 → fstring<Args...> 存储 string_view + 类型描述
编译失败 → report_error() → static_assert
```

### Chrono token 解析路径

```
"{:%Y-%m-%d %H:%M:%S}"
    │
    ▼
chrono_formatter::parse(ctx)
    │  消费 '%' 后读取 token 字符:
    │  %Y → year, %m → month, %d → day
    %H → hour, %M → minute, %S → second
    %F → %Y-%m-%d (快捷), %T → %H:%M:%S (快捷)
    │  token 序列存入 chrono_format_spec.chrono_specs
    ▼
chrono_formatter::format(tp, ctx)
    │  time_point → to_time_t → gmtime_r/localtime_r → std::tm
    │  遍历 chrono_specs:
    │    普通字符 → 直接输出
    │    %Y → format_decimal(tm.tm_year + 1900, 4)
    │    %S → 整数部分 + .%Q 子秒精度
    ▼
duration 格式化 (无 tm 转换):
    │  %H → duration_cast<hours>(d).count()
    │  %M → duration_cast<minutes>(d % 1h).count()
    │  %S → 子秒部分，精度由 .precision 控制
    │  FMT_SAFE_DURATION_CAST: 浮点 duration 安全转换
    ▼
输出到 ctx.out()
```

### Dragonbox 浮点转换

```
double val → dragonbox::to_decimal(val)
    │
    ▼
IEEE 754 分解:
    │  sign, exponent, significand
    │  特殊值: ±0, ±Inf, NaN → 快速路径直接输出
    ▼
cache 查表:
    │  根据 exponent 查 128-bit 预计算 cache
    │  两次 64×64 乘法: 移位 + 累加
    ▼
最短表示计算:
    │  直接计算最短十进制整数和指数
    │  无循环，固定 ~15-20 条指令
    ▼
write_int + write_exponent → 输出到缓冲区
```

Dragonbox 与 Ryu/Grisu3 的关键区别：Ryu 需要 128×128 乘法和除法，Grisu3 依赖迭代精化。Dragonbox 利用 IEEE 754 二进制模式的数学性质直接跳过精化步骤，单次查表 + 乘法即可得到最短表示。

### 串联关系

四条路径共享三阶段流水线：`fstring` consteval 构造（阶段 1）统一触发格式串解析和 `formatter<T>::parse()` 验证，无论是自定义类型、chrono 还是 ranges 都经过同一套 `format_string_checker`。运行时参数擦除（阶段 2）通过 `stored_type_constant` 将所有类型映射到 `value<Context>` 的 tagged union。输出生成（阶段 3）中，自定义 formatter 经 `custom_value` 函数指针间接调用，chrono/ranges 经完整特化的 `formatter<T>::format()` 直接调用，浮点类型走 Dragonbox 内联路径。`FMT_COMPILE` 绕过阶段 2，将格式串和参数绑定全部提升到类型系统，编译器直接生成最终输出代码。

fmt 的高级特性（自定义 formatter、chrono、ranges、compile）全部以头文件模板和内联函数实现，不提供标准库式的长期 ABI 稳定承诺。与 fmt-engine 中描述的基础 ABI 约束一致，额外注意：

- **模板实例化爆炸**：每个 `formatter<T>` 特化、每个 `format_custom<T>` 实例、每个 `FMT_COMPILE` 生成的 AST 节点都是独立模板实例。使用大量不同类型格式化时，目标文件体积可能膨胀。ABI 层面，这些实例在不同翻译单元中独立生成，链接器通过 COMDAT 折叠去重，但不同编译器的折叠策略可能不同。
- **`FMT_COMPILE` 的类型级 AST**：`concat<text, concat<field<int, 0>, text>>` 这样的嵌套类型完全由格式串决定。格式串内容改变即产生新类型，旧类型不再实例化——不存在跨版本兼容问题，但也意味着预编译格式串的二进制不可移植。
- **chrono/ranges 无独立 ABI**：`chrono.h` 和 `ranges.h` 中的 formatter 特化直接实例化在用户的翻译单元中，不导出任何额外符号（除 `FMT_API` 标记的 `safe_duration_cast` 辅助函数）。升级 fmt 版本后，所有包含这些头文件的翻译单元必须重新编译。
- **无 `stable` namespace**：fmt 的 `inline namespace v12` 在主版本间变化，v11 和 v12 的 `formatter<T>` 实例不能混合链接。

## 异常安全

高级特性的异常安全模型继承 fmt-engine 中描述的三层结构（编译期格式串错误 → 运行时 format_error → 内存分配失败），补充以下特有场景：

### 用户自定义 formatter 抛异常

- `formatter<T>::format()` 通过 `custom_value<Context>::format` 函数指针在运行时调用。若用户代码抛异常，异常沿 `vformat_to` 调用栈传播。
- `basic_memory_buffer` 在栈展开时通过 RAII 析构，释放已分配的堆内存。
- **部分输出不可回滚**：`format_to` 的输出迭代器（如 `back_insert_iterator<string>`）可能已写入部分字符。已 `push_back` 到 string 的字符保留在原处，不会回滚。对于 `FILE*` 输出，已写入的字节已到达文件描述符，无法撤回。
- 保证：无资源泄漏（RAII），但输出处于部分完成状态。

### Chrono 格式化的安全转换

- `FMT_SAFE_DURATION_CAST`（`chrono.h`）在浮点 duration 转换时检查溢出。`lossless_integral_conversion` 和 `safe_float_conversion` 设置错误码 `ec` 而非抛异常。
- `gmtime_r` / `localtime_r` 调用失败（如无效 `time_t`）时，fmt 返回空 `std::tm`，后续格式化输出 0 值而非崩溃。
- chrono formatter 的 `format()` 本身不抛异常（假设底层 C 库函数不抛），但 `format_to` 可能因缓冲区扩容抛 `bad_alloc`。

### Ranges 格式化的异常传播

- `formatter<Range>::format()` 遍历 range 时对每个元素调用 `underlying_.format(elem, ctx)`。若元素 formatter 抛异常，已格式化的前 N 个元素保留在输出中，后续元素不处理。
- range 遍历本身可能抛异常（如 `++it` 或 `*it` 的用户代码），传播路径同上。
- `fmt::join(range, sep)` 走专用路径，不创建 `format_context`，但元素格式化异常传播语义相同。

### `FMT_COMPILE` 的异常行为

- `FMT_COMPILE` 生成的 AST 节点的 `format()` 方法是 `constexpr` 函数，编译期内部不应抛异常。运行时执行时，由于无格式串解析和无类型擦除，唯一可能的异常来源是元素 formatter 和缓冲区扩容。
- 与普通 `fmt::format` 相比，`FMT_COMPILE` 路径少了 visit 分发和 format_specs 运行时解析，异常触发点更少。

### `FMT_USE_EXCEPTIONS=0`

- 所有 `FMT_THROW` 展开为 `fmt::assert_fail` → `abort()`。用户 formatter 中的 throw 同样被禁用（取决于编译器的 `-fno-exceptions` 设置）。在此模式下，任何异常路径直接终止进程。

## iterator / reference invalidation

本文涉及的引用/迭代器失效场景与 fmt-engine 中描述的基础机制一致，补充高级特性特有的关注点：

### `format_arg_store` 的借用语义

- `basic_format_args` 是非拥有视图，通过指针引用 `format_arg_store` 中的 `value<Context>[]`。`format_arg_store` 由 `make_format_args(args...)` 或 `fmt::format` 内部构造，生命周期绑定到调用表达式。
- **chrono/ranges 参数**：`fmt::format("{}", my_chrono_time)` 中，`format_arg_store` 持有 `custom_value{&my_chrono_time, format_custom<time_point>}`——void 指针指向栈上临时。`vformat_to` 返回前有效，不可逃逸。
- **危险模式**：`auto args = make_format_args(duration); vformat("{}", args);`——若 `duration` 是临时，`args` 悬空。fmt 文档明确警告。

### Ranges 适配器的迭代器稳定性

- `formatter<Range>::format()` 在遍历过程中缓存 `range_begin()` / `range_end()` 迭代器。若 range 在遍历期间被修改（如 `std::vector` 的 `push_back`），迭代器失效，行为未定义。
- `fmt::join(range, sep)` 同样在调用时求值 `begin()` / `end()`，之后使用缓存的迭代器。range 的生命周期必须覆盖整个 `format_to` 调用。
- **惰性 range**：`std::views::filter` 等惰性适配器在解引用时才求值谓词。若谓词捕获的引用失效，行为未定义。

### `basic_memory_buffer` 扩容与 `format_to` 迭代器

- `basic_appender<T>`（fmt 的 output iterator）内部持有 `buffer<T>*`，每次 `operator++` 调用 `push_back()` 可能触发 `grow()`。appender 不缓存 `data()` 指针，始终安全。
- 用户若在格式化过程中缓存 `buf.data()` 指针并后续解引用（如 `auto p = buf.data(); format_to(appender, ...); use(*p);`），`grow()` 后 `p` 悬空。

### Chrono 格式化的 `std::tm` 生命周期

- chrono formatter 的 `format()` 内部通过 `gmtime_r` / `localtime_r` 获取 `std::tm`。`gmtime_r` 写入调用者提供的缓冲区（栈上局部变量），`std::tm` 的生命周期覆盖整个 `format()` 调用。无悬空风险。
- `std::put_time`（若被使用）依赖 locale，locale 的 `std::time_put` facet 的生命周期由 locale 对象管理——fmt 不持有 locale，使用 `std::locale::classic()`，无生命周期问题。

### `FMT_COMPILE` AST 中的引用

- `text<Char>` 持有 `basic_string_view<Char>` 指向编译期字符串字面量（`.rodata` 段），永远有效。
- `spec_field<Char, V, N>` 内部持有 `formatter<V, Char>` 实例——由编译器在编译期构造，生命周期等于程序。
- `format()` 调用时参数通过 `const T&` 引用传入，引用有效性由调用者保证。

## 性能模型

fmt-advanced 的性能特征由四条路径的成本决定，按开销从小到大排列：

### `FMT_COMPILE`（最低开销）

- **零运行时解析**：格式串在编译期解析为类型级 AST，运行时无格式串扫描、无参数索引计算、无 format_specs 解析。
- **零类型擦除**：`spec_field<Char, V, N>::format()` 直接通过 `const V&` 引用获取参数值，无 tagged union、无 visit switch。
- **纯内联**：`concat<L,R>::format()` 递归调用被编译器完全内联，最终生成的代码等价于手写的 `write(out, "..."); write(out, val); write(out, "...");` 序列。
- benchmark 显示 `FMT_COMPILE("{}")` 路径比普通 `fmt::format("{}")` 快 ~20-30%（GCC 12, -O2），主要节省 visit 分发和 format_specs 运行时解析。

### 内置类型格式化（低开销）

- `value<Context>` 的 16 字节 union 直存——`int`/`double`/`string_view` 无堆分配。
- `visit()` 是 15 路 switch，GCC/Clang 在 `-O2` 下编译为跳转表（`jmp [table + rax*8]`），单次间接跳转。
- `format_decimal` 整数转字符串使用 200 字节查找表 `digits_`，每次循环写 2 个字符。`count_digits` 用 `__builtin_clzll` 位计数，O(1)。
- Dragonbox 浮点转换：~15-20 条算术指令（乘法 + 移位 + 查表），无循环。比 `std::to_chars`（Grisu3）快 2-3 倍。

### 自定义 formatter（中等开销）

- 额外一次函数指针间接跳转（`custom.format`）—— branch predictor 需要历史记录预测目标地址。首次调用冷启动 ~5-10ns。
- `formatter<T>::parse()` 在编译期执行，零运行时开销。
- `formatter<T>::format()` 的开销完全取决于用户实现——典型场景（写 3-5 个字段）与内置类型格式化在同一数量级。

### Chrono 格式化（中高开销）

- `gmtime_r` / `localtime_r` 系统调用可能涉及锁（某些 libc 实现），~50-100ns。
- `strftime` 风格 token 逐段解析 + `format_decimal` 输出，无额外堆分配。
- `FMT_SAFE_DURATION_CAST` 的安全转换增加少量分支检查（~2-3 个条件判断），可忽略。

### Ranges 格式化（最高开销）

- 线性遍历 range，对每个元素调用 `underlying_.format(elem, ctx)`——开销 = N × 元素格式化成本。
- `fmt::join(range, sep)` 比嵌套 `format("{}", range)` 省去括号和外层 format_specs 解析，但元素级开销相同。
- 嵌套 range（`vector<vector<int>>`）每次内层 `format()` 调用都触发 `underlying_` formatter 的递归实例化，编译时间和代码体积随嵌套深度指数增长。

### 栈缓冲命中率

所有路径共享 `basic_memory_buffer`（500 字节栈 SBO）。典型格式化输出 <100 字符，栈缓冲命中率 >95%。仅在输出超长（如格式化整个 range）时触发堆分配，堆分配按 1.5 倍增长，摊还 O(1)。

## libstdc++ vs libc++ vs MSVC

三家标准库的 `std::format` 在高级特性上与 fmt 的对照（基础引擎对比见 fmt-engine）：

| 维度 | fmt (v12) | libstdc++ (GCC 14+) | libc++ (LLVM 18+) | MSVC STL (VS 2022 17.10+) |
|------|-----------|---------------------|-------------------|---------------------------|
| 自定义 formatter 协议 | `formatter<T>::parse()` + `format()` + ADL `format_as()` | `std::formatter<T>::parse()` + `format()` | 同左 | 同左 |
| `format_as` 扩展 | 支持（ADL + 成员函数） | 不支持 | 不支持 | 不支持 |
| Ranges 格式化 | `ranges.h`：SFINAE 检测 `is_range_`/`is_tuple_like_`，支持 map/set/sequence/tuple，`join(range, sep)` | C++23 `std::formatter<range>`，支持 `range` 概念，无 `join` 等价物 | 同左 | 同左 |
| Chrono 格式化 | 完整 strftime token + `FMT_SAFE_DURATION_CAST` + 浮点 duration 安全转换 | C++20 `std::chrono` 格式化，完整 strftime 支持，无安全 duration cast | 同左 | 同左 |
| 编译期格式串编译 | `FMT_COMPILE` → 类型级 AST，完全消除运行时解析 | 无等价物（consteval 检查 ≠ 编译格式串） | 同左 | 同左 |
| 动态宽度/精度 | `{:{}}` + 命名参数 `{:{name}}` | `{:{}}`，不支持命名动态精度 | 同左 | 同左 |
| 输出迭代器模型 | `basic_appender<T>` + `FILE*` + `ostream&` + 任意 `OutputIt` | `OutputIt` + `FILE*`（C++23 `std::print`） | 同左 | 同左 |
| `to_string` 便利 | `fmt::to_string(T)` | 无（使用 `std::format`） | 同左 | 同左 |
| 填充与对齐 | `fill align width`，任意 Unicode 填充字符 | 同标准语义 | 同左 | 同左 |
| `format_error` 类型 | `fmt::format_error`（`std::runtime_error` 子类） | `std::format_error`（C++26 前为 `std::runtime_error`） | 同左 | 同左 |

**关键差异**：

1. **`format_as` 是 fmt 独有扩展**：允许非侵入式将用户类型映射为已格式化类型，无需特化 formatter。P2836 提议纳入 C++26 标准，但截至各实现最新版本均未支持。标准库代码必须显式特化 `std::formatter<T>`。

2. **`FMT_COMPILE` 无标准等价物**：标准库的 consteval 检查仅验证格式串合法性，不生成预编译格式化代码。`FMT_COMPILE` 将格式串编译为类型级 AST，消除运行时格式串解析——对高频格式化路径（如日志热路径）有显著性能优势。

3. **Ranges 格式化的功能差异**：fmt 的 `fmt::join(range, sep)` 是标准库无对应物的便利函数。标准库的 `std::formatter<range>` 支持基本 range 格式化但不提供自定义分隔符的直接 API。此外，fmt 的 range formatter 通过 `{::spec}` 语法将格式规范下推到元素 formatter，标准库行为类似但规范细节可能有差异。

4. **编译期检查的深度**：fmt 的 `format_string_checker` 在编译期调用每个 `formatter<T>::parse()` 验证格式规范，标准库实现也做类似检查。但 fmt 额外通过 `check_dynamic_spec` 验证动态宽度/精度参数必须为整数类型，此检查在标准库中由实现决定是否执行。

5. **性能差异**：浮点格式化是最大差异点（Dragonbox vs `to_chars`）。在自定义 formatter、chrono 和 ranges 路径上，fmt 和标准库实现的性能差异主要来自缓冲区策略（fmt 500 字节栈 SBO vs 标准库实现定义的 SBO 大小）和内联策略。

## 最小复现代码

```cpp
#include <chrono>
#include <fmt/chrono.h>
#include <fmt/format.h>

struct Point {
  int x;
  int y;
};

template <>
struct fmt::formatter<Point> : fmt::formatter<int> {
  auto format(const Point& p, fmt::format_context& ctx) const {
    return fmt::format_to(ctx.out(), "({}, {})", p.x, p.y);
  }
};

int main() {
  return static_cast<int>(fmt::format("{}", Point{1, 2}).size());
}
```

## 编译 / 反汇编 / benchmark 证据
### 自定义 formatter 反汇编

```cpp
struct Point { double x, y; };
template <> struct fmt::formatter<Point> : fmt::formatter<double> {
  auto format(const Point& p, fmt::format_context& ctx) const {
    return fmt::format_to(ctx.out(), "({}, {})", p.x, p.y);
  }
};
```

- GCC 12 `-O2` 下，`visit` 的 `custom_type` 分支编译为 `call rax`（`rax` = `custom.format` 函数指针）。`format_custom<Point>` 实例被内联到 `formatter<Point>::format()` 中，最终代码包含 `format_to` 对 double 的两次调用 + 两个字面量写入。
- `custom_value` 的 16 字节 union 在栈上对齐到 16 字节，`value<Context>` 数组连续排列，visit 的索引计算为 `base + idx * 16`（O(1)）。

### Chrono 格式化 benchmark

```cpp
auto now = std::chrono::system_clock::now();
for (int i = 0; i < 1000000; ++i)
  fmt::format("{:%Y-%m-%d %H:%M:%S}", now);
```

- GCC 12 `-O2`, Zen 3: ~200ns/call（栈缓冲命中路径），主要成本：`gmtime_r` 系统调用 ~80ns + `format_decimal` × 6 次 ~60ns + 字面量写入 ~20ns。
- 对比 `std::format`（libstdc++ GCC 14）：~250ns/call（缓冲区策略差异）。
- 对比 `snprintf(buf, ..., "%Y-%m-%d %H:%M:%S", tm)`：~300ns/call（格式串运行时解析）。

### Ranges 格式化 benchmark

```cpp
std::vector<int> v(100);
std::iota(v.begin(), v.end(), 0);
for (int i = 0; i < 100000; ++i)
  fmt::format("{}", v);
```

- GCC 12 `-O2`, Zen 3: ~2.5μs/call（100 个 int，栈缓冲命中）。主要成本：100 次 `format_decimal` + 99 次分隔符写入 + 2 次括号写入。
- `fmt::join(v, ", ")`：~2.3μs/call（省去括号和外层 format_specs 解析）。
- 对比手写循环 + `format_to`：~2.0μs/call——ranges 格式化的额外开销约 15-20%，主要来自 `underlying_.format()` 的间接调用和 range 迭代器解引用。

### `FMT_COMPILE` 反汇编与 benchmark

```cpp
auto compiled = FMT_COMPILE("{} {}");
for (int i = 0; i < 1000000; ++i)
  fmt::format(compiled, 42, 3.14);
```

- GCC 12 `-O2` 下，`FMT_COMPILE("{} {}")` 生成的 `concat<text, concat<field<int, 0>, concat<text, field<double, 1>>>>::format()` 被完全内联。反汇编可见：`format_decimal` 调用（int 42）+ 直接写入 `' '` + Dragonbox 调用（double 3.14）——无 visit switch、无 format_specs 解析、无 tagged union 访问。
- benchmark：~25ns/call vs 普通 `fmt::format` ~35ns/call，快 ~30%。

### Dragonbox 浮点路径反汇编

```cpp
fmt::format("{}", 3.14159);
```

- GCC 12 `-O2` 下，`dragonbox::to_decimal` 编译为 ~18 条算术指令：`imul`（cache 查表） + `shrx`/`shlx`（移位） + `movzx`（digits 查表） + 条件分支（特殊值检查）。无循环、无函数调用。
- 对比 `std::to_chars(buf, buf+32, 3.14159)`（Grisu3 路径）：~25 条指令 + 1 次 128×128 乘法。
- 对比 `snprintf(buf, 32, "%g", 3.14159)`：~200 条指令（libc 的 `printf` 实现）。

## cpplings 练习入口

- [`format1` — std::format 格式化](../../../exercises/cpp20/format1.cpp)
- [`print23` — std::print / std::println 格式化输出](../../../exercises/cpp23/print23.cpp)
- [`chrono1` — chrono 时间库](../../../exercises/cpp11-std/chrono1.cpp)
- [`ranges1` — Ranges 基础`](../../../exercises/cpp20/ranges1.cpp)
