---
title: fmt 格式化引擎
topic: libraries
feature: fmt-engine
standard: C++20
status_checked_at: 2026-06-02
implementation:
  fmt:
    paths:
      - references/impl/fmt/include/fmt/base.h
      - references/impl/fmt/include/fmt/format.h
    symbols:
      - basic_format_arg
      - basic_memory_buffer
      - fstring
      - format_string_checker
exercises: []
solutions: []
---
# fmt 格式化引擎

> 源码路径：`references/impl/fmt/include/fmt/base.h`, `format.h`

## 三阶段流水线

```
fmt::format("Hello, {}! Value: {:d}", name, count)
     |
     v
阶段 1: 编译期格式串解析 (consteval)
  fstring<Args...> 的 consteval 构造函数
  parse_format_string(str, checker)
  验证: 参数索引合法? 类型匹配? 格式规范合法?
  不合法 -> 编译期 report_error() -> 编译失败
     |
     v
阶段 2: 参数类型擦除 (tagged union)
  stored_type_constant<T>::value -> type 枚举
  value<Context> 联合体存储（16 字节，128 位值存储）
  运行时: basic_format_arg::visit(visitor) switch 分发
     |
     v
阶段 3: 输出生成
  memory_buffer: 500 字节栈数组，零堆分配
  formatter<T>::format(value, ctx)
  Dragonbox 浮点 / 整数直接写入
```

## 编译期格式串检查

```cpp
template <typename... T> struct fstring {
  using checker = detail::format_string_checker<char, int(sizeof...(T)), ...>;

  template <size_t N>
  FMT_CONSTEVAL fstring(const char (&s)[N]) : str(s, N - 1) {
    parse_format_string<char>(str, checker(str, arg_pack()));
  }
};
```

`FMT_CONSTEVAL` 展开为 C++20 `consteval`，编译器强制在编译期执行全部验证。

### 类型兼容性验证

```cpp
constexpr auto integral_set = sint_set | uint_set | bool_set | char_set;
case 'd': return parse_presentation_type(pres::dec, integral_set);
case 'f': return parse_presentation_type(pres::fixed, float_set);
```

`parse_presentation_type` 通过 `in(arg_type, set)` 位运算校验：若参数类型不在允许集合内，编译期报错。

## 类型擦除：tagged union + visit

```cpp
template <typename Context> class value {
public:
  union {
    int int_value;
    unsigned uint_value;
    double double_value;
    string_value<char_type> string;       // {const Char* data; size_t size;}
    custom_value<Context> custom;         // {void* value; void (*format)(...);}
    // ... 15 种类型
  };
};

template <typename Visitor>
FMT_CONSTEXPR auto visit(Visitor&& vis) const {
  switch (type_) {
  case type::int_type:     return vis(value_.int_value);
  case type::double_type:  return vis(value_.double_value);
  case type::string_type:  return vis(value_.string.str());
  case type::custom_type:  return vis(handle(value_.custom));
  // ... 15 个分支
  }
}
```

union 占 16 字节——最大成员是 `long double`（16 字节）。

## 输出缓冲区

```cpp
// 500 字节栈缓冲区，短字符串（<50 字符）全程栈内
template <typename T, size_t SIZE = inline_buffer_size,
          typename Allocator = std::allocator<T>>
class basic_memory_buffer : public detail::buffer<T> {
  T data_[SIZE];  // 栈上缓冲区
  Allocator alloc_;
  // 超过 SIZE 时自动切换到堆分配
};
```

## 用户 API

用户入口主要是 `fmt::format`、`fmt::print` 这一类高层接口；本文现有正文已经向下展开到格式串检查、参数擦除与输出缓冲。

## 标准语义

fmt 覆盖了 C++20 `std::format` / C++23 `std::print` 的完整格式串语法：位置参数 `{0}`、自动索引 `{}`、命名参数 `{name}`、嵌套宽度/精度 `{:{}}`、格式规范 `fill align sign # 0 width precision type`。核心语义差异如下：

| 维度 | `std::format` | `fmt::format` |
|------|---------------|---------------|
| 格式串类型 | `std::format_string<Args...>`（C++20 起 consteval） | `fmt::fstring<Args...>`（`FMT_CONSTEVAL` 构造函数） |
| 检查时机 | 编译期（consteval 构造） | 编译期（consteval 构造）+ 运行时回退（`vformat` 路径不检查） |
| 错误报告 | `static_assert` 或编译器诊断 | `report_error()` → 编译期 `static_assert`；运行时 `format_error` 异常 |
| 自定义类型 | `std::formatter<T>` 特化 | `fmt::formatter<T>` 特化 + ADL `format_as()` 自由函数 + `formatter<T>::format_as()` 成员函数 |
| 浮点格式 | 实现定义（通常 Grisu3 或 `to_chars`） | Dragonbox（最短表示，~2-5x 快于 `to_chars`） |
| 编译期格式串 | `std::format_string`（非类型模板参数，C++26） | `fmt::fstring`（类模板，C++20 consteval 构造函数） |
| 动态格式串 | `std::vformat(fmt, args)`（无编译期检查） | `fmt::vformat(fmt, args)`（无编译期检查，运行时解析） |

**fmt 对编译期检查的额外强化**：

- `format_string_checker` 在编译期逐字符解析格式串，对每个 `{id}` 调用 `parse_funcs_[id](context_)`——这会调用对应 `formatter<T>::parse()`，将格式规范的编译期验证下推到每个类型的 `parse` 方法。
- `compile_parse_context` 继承 `parse_context` 并持有 `types_` 数组和 `num_args_`，在 `check_arg_id(id)` 中验证参数索引不越界，在 `check_dynamic_spec(int)` 中验证动态宽度/精度参数必须是整数类型。
- `mapped_type_constant<T, Char>` 通过 `type_mapper` 将用户类型映射为内置类型或 `custom_type`，再通过 `stored_type_constant` 决定存储路径——编译期即可判断 `formatter<T>` 是否存在（`has_formatter<T, Char>()`），不存在则触发 `type_is_unformattable_for` 编译错误。
- `encode_types<Context, T...>()` 将每个参数的 `type` 枚举值打包进一个 `ullong`（每参数 4 bit），编译期计算 `desc_`，运行时零开销读取类型。
::::

## 对象布局

上文已经给出 `value<Context>` tagged union 与 `basic_memory_buffer` 的关键结构；后续补参数存储、handle 与缓冲区的统一布局图。

## 核心源码路径

本文开头已给出 `base.h` 与 `format.h`；后续补 `parse_format_string`、`format_string_checker`、`visit`、`memory_buffer` 的调用链。

## 核心类 / 函数

**`fstring<Args...>`**（`base.h`）：格式串包装器，持有 `basic_string_view<Char> str`。`FMT_CONSTEVAL` 构造函数接受 `const char (&)[N]` 或 `std::string`，构造时调用 `parse_format_string<char>(str, checker(str, arg_pack()))`——`checker` 是 `format_string_checker<char, int(sizeof...(T)), ...>` 的实例。编译期逐字符扫描格式串，每个 `{` 进入 replacement field 解析，`}` 结束；非法格式串在编译期通过 `report_error()` 触发 `static_assert`。

**`format_string_checker`**（`base.h:1679`）：编译期格式串验证器。构造时将每个参数的 `type` 枚举存入 `types_[]`，将每个 `formatter<T>::parse` 的函数指针存入 `parse_funcs_[]`。`on_arg_id()` 处理自动索引，`on_arg_id(int)` 处理显式索引（调用 `context_.check_arg_id(id)` 验证越界），`on_arg_id(basic_string_view<Char>)` 处理命名参数（在 `named_args_[]` 中线性查找）。`on_format_specs(id, begin, end)` 调用 `parse_funcs_[id](context_)` 将格式规范下推给对应 `formatter<T>::parse()`。

**`value<Context>`**（`base.h:2135`）：16 字节 tagged union，成员包括 `int`、`unsigned`、`long long`、`ullong`、`native_int128`、`bool`、`char_type`、`float`、`double`、`long double`（最大成员，决定 union 大小）、`const void*`、`string_value<char_type>`（`{const Char* data; size_t size}`）、`custom_value<Context>`（`{void* value; void (*format)(...)}`）。构造函数通过 `stored_type_constant<T>::value` 决定存储分支：内置类型直接写入对应 union 成员；用户类型走 `custom_tag` 路径，存储对象指针 + `format_custom<T>` 函数指针。

**`basic_format_arg<Context>`**（`base.h:2451`）：持有 `value<Context> value_` 和 `type type_`。`visit(Visitor&&)` 方法对 `type_` 做 15 路 switch 分发，将 union 成员传给 visitor。`format_custom()` 方法对 `custom_type` 调用 `value_.custom.format()`，将 `formatter<T>::format()` 的调用延迟到运行时。

**`basic_memory_buffer<T, SIZE, Allocator>`**（`format.h:778`）：继承 `detail::buffer<T>`（`{T* ptr_; size_t size_; size_t capacity_; grow_fun grow_}`），内嵌 `T store_[SIZE]`（默认 `SIZE = 500`）和 `Allocator alloc_`。构造时将 `ptr_` 指向 `store_`，`capacity_ = SIZE`。`grow` 静态方法在容量不足时按 1.5 倍增长（`new_capacity = old + old/2`），分配堆内存后 `memcpy` 搬移数据，释放旧缓冲（若不是 `store_`）。析构时仅释放堆分配（`if (data != store_)`）。

**`basic_format_args<Context>`**（`base.h:2532`）：参数视图，持有 `ullong desc_` 和一个 union（`values_` 或 `args_`）。当参数数 ≤ `max_packed_args`（15）时，类型信息编码在 `desc_` 的低 60 bit（每参数 4 bit），值存在连续的 `value<Context>` 数组中；超过 15 个参数时 `desc_` 高位设 `is_unpacked_bit`，改为存储 `basic_format_arg<Context>` 数组（每个 24 字节：16 字节 union + 4 字节 type + padding）。`type(index)` 方法从 `desc_` 中按偏移读取 4 bit 类型码。
::::

## 关键算法

正文已经覆盖三阶段流水线；后续补“解析 → 类型检查 → visit 分发 → 输出写入”的关键分支摘要。

## ABI 约束

fmt 以头文件模板和内联函数为主体，不提供标准库式的稳定 ABI 承诺。具体约束：

- **命名空间版本化**：`FMT_BEGIN_NAMESPACE` 展开为 `namespace fmt { inline namespace v12 { ... }}`。`v12` 是 ABI 版本标记——跨主版本（如 v11 → v12）链接同一二进制的不同版本 fmt 会导致符号冲突或 ODR 违反，因为 `inline namespace` 使 `fmt::format` 实际解析为 `fmt::v12::format`。
- **模板实例化**：`formatter<T>`、`value<Context>`、`basic_format_arg<Context>` 等核心类型均为模板，每个翻译单元独立实例化。ABI 稳定性取决于编译器是否对相同模板参数产生相同布局——通常成立，但不同编译器版本的空基类优化（EBO）、`[[no_unique_address]]` 语义差异可能导致 `value<Context>` 的实际大小不同。
- **`FMT_API` 标记**：少量非模板符号（如 `dragonbox::to_decimal`、`dragonbox::get_cached_power`、`assert_fail`）标记 `FMT_API`。在 Windows 上展开为 `__declspec(dllexport/dllimport)`，在 ELF/Mach-O 上展开为 `__attribute__((visibility("default")))`。这些符号在主版本内保持稳定，但跨主版本可能改变签名。
- **`FMT_BUILTIN_TYPES` 开关**：默认为 1（内置类型走 union 直存）。设为 0 时所有类型走 `custom_value` 路径（函数指针间接调用），改变 `stored_type_constant` 的映射结果，导致同一 `value<Context>` 的 `type_` 枚举值不同——跨翻译单元混用不同设置是 ODR 违反。
- **无导出符号表承诺**：fmt 不维护类似 `libc.so` 的版本脚本。动态库场景下（`FMT_SHARED`），导出的符号集合随版本变化。升级 fmt 版本需重新编译所有依赖方。
::::

## 异常安全

fmt 的异常安全模型分为三层：

**格式串错误（编译期）**：
- `format_string_checker::on_error()` 调用 `report_error(message)`，在 consteval 上下文中触发编译失败。不产生运行时异常。
- `compile_parse_context::check_arg_id()` 验证参数索引越界，同样编译期报错。

**格式串错误（运行时，`vformat` 路径）**：
- `vformat_to` 解析格式串时遇到非法格式（如未闭合的 `{`、无效的格式规范字符），抛出 `fmt::format_error`（继承 `std::runtime_error`）。
- `basic_format_args::get()` 在索引越界时返回空 `basic_format_arg`（`type_ == none_type`），visit 走到 `monostate` 分支，不抛异常但输出空字符串。

**内存分配失败**：
- `basic_memory_buffer::grow()` 调用 `Allocator::allocate(new_capacity)`。使用 `std::allocator` 时分配失败抛 `std::bad_alloc`。
- 关键保护：`grow()` 先分配新内存、`memcpy` 搬移数据、再释放旧内存。若 `allocate` 抛异常，旧数据不受影响（`old_data` 仍在原处），`basic_memory_buffer` 析构时正常释放 `store_` 或之前的堆分配。若 `deallocate` 抛异常（标准要求不抛，但 fmt 注释明确说"即使抛了也无害"，因为新存储已接管）。

**用户自定义 formatter 抛异常**：
- `formatter<T>::format()` 通过 `custom_value<Context>::format` 函数指针调用。若用户 formatter 抛异常，异常沿 `vformat_to` 的调用栈传播——`basic_memory_buffer` 在栈展开时析构，释放已分配的堆内存（RAII）。
- `format_to` 的输出迭代器可能已经部分写入——已写入的字符不可回滚。对于 `std::string` 输出（`back_insert_iterator`），已 `push_back` 的字符保留在 string 中。

**异常禁用模式**：
- `FMT_USE_EXCEPTIONS=0` 时，`FMT_THROW(x)` 展开为 `::fmt::assert_fail(__FILE__, __LINE__, (x).what())`——直接 `abort()`，不产生异常。`FMT_TRY`/`FMT_CATCH` 展开为 `if(true)`/`if(false)`，所有 catch 块被优化掉。
::::

## iterator / reference invalidation

**`basic_format_args` 的借用生命周期**：
- `basic_format_args` 是一个**非拥有视图**（view），不持有参数值的内存。它通过指针引用 `format_arg_store` 中的 `value<Context>[]` 或 `basic_format_arg<Context>[]`。
- `format_arg_store<Context, NUM_ARGS, ...>` 由 `make_format_args(args...)` 或 `fmt::format` 内部构造，其生命周期绑定到调用表达式。典型用法 `vformat_to(out, fmt, make_format_args(args...))` 中，`format_arg_store` 是临时对象，在 `vformat_to` 返回前有效——`basic_format_args` 不能逃逸到外部存储。
- **危险模式**：`auto args = fmt::make_format_args(42, "hello"); fmt::vformat("{}", args);`——`args` 内部持有指向栈上临时 `format_arg_store` 的指针，但 `format_arg_store` 已在第一条语句结束后析构，`args` 悬空。fmt 文档明确警告此模式。

**`basic_memory_buffer` 扩容后的指针失效**：
- `basic_memory_buffer` 的 `grow()` 方法在容量不足时分配新堆内存、`memcpy` 搬移、释放旧内存。搬移后 `ptr_` 指向新地址，`store_`（栈内数组）地址不变。
- `detail::buffer<T>` 提供的 `data()` 返回 `ptr_`，`size()` 返回已写入量。用户通过 `data()` 或迭代器获得的指针/引用在 `grow()` 后失效——与 `std::vector` 的 `push_back` 失效语义相同。
- `basic_appender<T>`（fmt 的 output iterator）内部持有 `buffer<T>*`，每次 `operator++` 调用 `buf.push_back()`，后者可能触发 `grow()`。由于 appender 不缓存 `data()` 指针，它始终安全。但用户若在格式化过程中缓存了 `buf.data()` 指针并后续解引用，行为未定义。
- **栈 → 堆切换点**：首次 `grow()` 发生在写入超过 500 字节时。`store_` 仍在栈上但不再使用，`ptr_` 切换到堆地址。此后所有指针失效仅涉及堆内存。
::::

## 性能模型

正文已经点出 consteval 检查、16-byte union 与 500-byte 栈缓冲；后续补分支预测、堆分配阈值与 visit 分发成本。

## libstdc++ vs libc++ vs MSVC

三家标准库的 `std::format` 引擎与 fmt（独立库）在关键维度上的对照：

| 维度 | fmt (v12) | libstdc++ (GCC 14+) | libc++ (LLVM 18+) | MSVC STL (VS 2022 17.10+) |
|------|-----------|---------------------|-------------------|---------------------------|
| 格式串检查 | `consteval` 构造（`fstring`） | `consteval` 构造（`__format_string_view`） | `consteval` 构造 | `consteval` 构造 |
| 参数类型擦除 | 16-byte union + 4-bit packed type（`value<Context>`） | `__format_arg` 类型擦除，内部 `_Arg` tagged union | `_FormatArg` 类型擦除 | `_Basic_format_arg` 类型擦除 |
| packed 参数阈值 | ≤15 参数 packed（4 bit × 15 = 60 bit in `desc_`） | 类似 packed 策略 | 实现定义 | 实现定义 |
| 缓冲策略 | 500 字节栈 SBO + 堆 fallback（1.5x 增长） | `__output_buffer`，栈 SBO + 堆 fallback | `_OutputBuffer`，栈 SBO + 堆 fallback | `_Fmt_buffer`，栈 SBO + 堆 fallback |
| 浮点格式化 | Dragonbox（最短表示） | `std::to_chars`（Grisu3 + fallback） | `std::to_chars`（Dragonbox 或 Ryu） | `std::to_chars`（Grisu3 + fallback，VS 2022 17.10+ 改用 Dragonbox） |
| 整数格式化 | `format_decimal` 直接写入缓冲 + `write_int` 处理符号/prefix/padding | `__to_chars_integral` | `_Int_to_chars` | `_Integral_to_chars` |
| 输出迭代器模型 | `basic_appender<T>`（back_insert_iterator）+ `FILE*` + `iterator_buffer` | 迭代器 + `__output_buffer` | 迭代器 + `_OutputBuffer` | 迭代器 + `_Fmt_buffer` |
| `format_error` | `fmt::format_error`（继承 `std::runtime_error`） | `std::format_error`（C++26 前为 `std::runtime_error`） | `std::format_error` | `std::format_error` |
| `format_as` 支持 | ADL `format_as()` + `formatter<T>::format_as()` 成员 | 不支持（C++26 提案 P2836） | 不支持 | 不支持 |
| 编译期 `formatter<T>::parse` | `constexpr`（`FMT_CONSTEVAL`），编译期完整验证格式规范 | `constexpr`，编译期验证 | `constexpr`，编译期验证 | `constexpr`，编译期验证 |

**关键差异**：
- **浮点格式化**是性能差异最大的点。fmt 的 Dragonbox 实现直接从 IEEE 754 位模式计算最短十进制表示，不依赖 `to_chars`。libstdc++ 和 MSVC STL 的 `std::format` 通过 `std::to_chars` 间接调用，性能取决于标准库的 `to_chars` 实现质量。
- **`format_as` 扩展点**是 fmt 独有的——允许非侵入式地将用户类型映射为内置类型，标准库无对应机制。
- **缓冲区大小**：三家标准库的栈 SBO 大小各不相同（通常 256-512 字节），fmt 固定 500 字节。实际命中率取决于格式化输出长度——典型日志行（<100 字符）在所有实现中均命中栈缓冲。
::::

## 最小复现代码

```cpp
#include <fmt/format.h>

int main() {
  auto s = fmt::format("Hello, {}! {}", "world", 42);
  return static_cast<int>(s.size());
}
```

## 编译 / 反汇编 / benchmark 证据

**consteval 检查路径**：
- `fstring` 构造函数标记 `FMT_CONSTEVAL`（展开为 C++20 `consteval`），编译器必须在编译期完成格式串解析。GCC/Clang 下 `-std=c++20` 编译 `fmt::format("{}", 42)` 时，格式串检查不产生任何运行时指令——可在反汇编中验证 `format_string_checker` 不出现在 `.text` 段。
- 若格式串非法（如 `fmt::format("{:d}", "hello")`），编译器报告 `format_string_checker::on_error()` → `report_error()` 触发的错误信息，而非链接或运行时错误。

**参数 visit 分发**：
- `basic_format_arg::visit()` 是 `FMT_INLINE` 标记的 15 路 switch。GCC/Clang 在 `-O2` 下将其编译为跳转表或二分查找——对 15 个 case，跳转表更常见（`jmp [table + rax*8]`）。
- `visit` 的 `case type::int_type` 分支直接读取 `value_.int_value`（偏移 0），`case type::double_type` 读取 `value_.double_value`（偏移 0，union 共享起始地址）——无额外间接层。
- `custom_type` 分支调用 `handle(value_.custom)` 后经 `custom_.format(custom_.custom.value, parse_ctx, ctx)` 间接调用用户 formatter——此处有一次函数指针间接跳转，branch predictor 需要历史记录来预测目标地址。

**`memory_buffer` 栈内命中率**：
- `inline_buffer_size = 500` 字节。典型格式化输出（日志行、错误消息、用户提示）通常 <100 字符，500 字节栈缓冲可覆盖绝大多数场景，**全程零堆分配**。
- `basic_memory_buffer` 的构造（`set(store_, SIZE)`）和析构（`if (data != store_) deallocate()`）在栈命中时仅涉及栈指针调整，无 `malloc`/`free` 系统调用。
- benchmark（fmt 官方）显示：格式化 `"Hello, {}! {}"` + `string_view` + `int` 在栈缓冲命中路径下耗时 ~30ns（GCC 12, `-O2`, Zen 3），主要成本是 `memcpy` 写入输出字符串 + `format_decimal` 整数转字符串。

**`format_decimal` 整数格式化**：
- fmt 使用查找表 `digits_`（200 字节，预计算 "00"-"99"）加速两位数转换——每次循环写 2 个字符而非 1 个。反汇编中可见 `movzx eax, WORD PTR digits[rax*2]` 的查表指令。
- 对 64 位整数，`count_digits` 使用 `FMT_BUILTIN_CLZLL`（`__builtin_clzll` 或 MSVC `_BitScanReverse64`）计算位数，O(1) 复杂度。

**Dragonbox 浮点格式化**：
- `dragonbox::to_decimal` 对 IEEE 754 double 的典型路径约 15-20 条算术指令（乘法 + 移位 + 查表），无循环。benchmark 显示比 `std::to_chars`（Grisu3）快 2-3 倍，比 `printf("%.17g")` 快 5-10 倍。
::::

## cpplings 练习入口

- [`format1` — std::format 格式化](../../../exercises/cpp20/format1.cpp)
- [`print23` — std::print / std::println 格式化输出](../../../exercises/cpp23/print23.cpp)
