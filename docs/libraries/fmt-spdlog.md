# fmt / spdlog 源码级深度剖析

## 概述

**fmt**（[github.com/fmtlib/fmt](https://github.com/fmtlib/fmt)）由 Victor Zverovich 于 2012 年创建，是 C++ 格式化库的事实标准。其 API 设计直接被 C++20 标准采纳为 `std::format`（P0645R10），fmt 的源码即为该提案的参考实现。核心设计目标：类型安全、编译期格式检查、高性能，替代 `printf` / `iostream` 的两套缺陷。

**spdlog**（[github.com/gabime/spdlog](https://github.com/gabime/spdlog)）由 Gabi Melman 创建，是 C++ 生态中最流行的日志库。原生基于 fmt 构建，采用 Logger → Sink → Formatter 三层架构，支持同步/异步两种模式。设计哲学：零配置可用、常见场景零堆分配、日志级别在编译期裁剪。

两者的关系：fmt 负责"如何把值变成字符串"，spdlog 负责"何时、何地、以何种方式输出日志"。fmt 是 spdlog 的底层依赖，但 fmt 本身完全独立。

---

## fmt 核心设计

### 格式化引擎

fmt 的格式化引擎分三个阶段：**格式串解析** → **参数类型擦除** → **输出生成**。前两阶段在编译期完成，运行期只剩一次类型擦除的分发和内存写入。


### fmt 格式化引擎三阶段流水线

```
  fmt::format("Hello, {}! Value: {:d}", name, count)
       |
       |
       v
  +================================================================+
  | 阶段 1: 编译期格式串解析 (format_string_checker 状态机)          |
  |                                                                |
  |  fstring<Args...> 的 consteval 构造函数                         |
  |       |                                                        |
  |       v                                                        |
  |  parse_format_string(str, checker)                              |
  |       |                                                        |
  |       v                                                        |
  |  +--------+    +--------+    +---------+    +---------+        |
  |  | start  |--->|  { 遇到 |--->| arg_id  |--->| parse_  |        |
  |  | 逐字符 |    | 替换字段|    | 解析    |    | format_ |        |
  |  | 扫描   |    |        |    |         |    | specs   |        |
  |  +--------+    +--------+    +---------+    +---------+        |
  |                                                                |
  |  验证: 参数索引合法?  类型匹配?  格式规范合法?                   |
  |  不合法 -> 编译期 report_error() -> 编译失败                    |
  +================================================================+
       |
       v
  +================================================================+
  | 阶段 2: 参数类型擦除 (tagged union + visit 分发)                 |
  |                                                                |
  |  编译期:                                                       |
  |    stored_type_constant<T>::value -> type 枚举                  |
  |    (int_type, double_type, string_type, custom_type ...)        |
  |                                                                |
  |    value<Context> 联合体存储:                                    |
  |    +--------------------------------------------------+        |
  |    | int_value | double_value | string {data,size}    |        |
  |    | custom {void*, format_func_ptr} | ...            |        |
  |    +--------------------------------------------------+        |
  |    (整个 union 16 字节, 即 128 位值存储)                        |
  |                                                                |
  |  运行时:                                                       |
  |    basic_format_arg::visit(visitor)                             |
  |       |                                                        |
  |       v                                                        |
  |    switch (type_) {                                            |
  |      case int_type:    return vis(value_.int_value);           |
  |      case double_type: return vis(value_.double_value);        |
  |      case string_type: return vis(value_.string.str());        |
  |      case custom_type: return vis(handle(value_.custom));      |
  |      ... (15 种类型分支)                                        |
  |    }                                                           |
  +================================================================+
       |
       v
  +================================================================+
  | 阶段 3: 输出生成                                                |
  |                                                                |
  |  +-----------------------+                                     |
  |  | memory_buffer         |   500 字节栈数组, 零堆分配            |
  |  | (basic_memory_buffer) |                                     |
  |  +-----------+-----------+                                     |
  |              |                                                 |
  |              v                                                 |
  |  formatter<T>::format(value, ctx)                              |
  |       |                                                        |
  |       v                                                        |
  |  Dragonbox 浮点 / 整数直接写入 / 自定义类型回调                  |
  |       |                                                        |
  |       v                                                        |
  |  写入 output iterator (buffer / FILE* / 用户自定义)             |
  +================================================================+
       |
       v
  "Hello, Alice! Value: 42"

  性能特征:
  - 阶段 1 完全在编译期 (零运行时开销)
  - 阶段 2: <=15 参数时 packed 模式, 类型标签编码进 ullong 低位
  - 阶段 3: 短字符串 (<50 字符) 全程栈内, 无 malloc
```
### 编译期格式串解析：format\_string\_checker 状态机

`fmt::format` 的格式串必须是编译期常量（除非显式使用 `fmt::runtime()`）。核心机制：`fstring&lt;T...&gt;` 结构体持有一个 `consteval` 构造函数，在编译期完成全部验证。

#### 源码级实现

`fstring` 是 `format_string&lt;Args...&gt;` 的实际类型。其构造函数标记为 `FMT_CONSTEVAL`（展开为 C++20 `consteval`），编译器强制在编译期执行：

```cpp
// include/fmt/base.h — fstring 的 consteval 构造函数
template <typename... T> struct fstring {
  using checker = detail::format_string_checker<
      char, int(sizeof...(T)),    // NUM_ARGS
      num_static_named_args,      // NUM_NAMED_ARGS
      num_static_named_args != detail::count_named_args<T...>()>;

  template <size_t N>
  FMT_CONSTEVAL FMT_ALWAYS_INLINE fstring(const char (&s)[N]) : str(s, N - 1) {
    if (FMT_USE_CONSTEVAL)
      parse_format_string<char>(str, checker(str, arg_pack()));
  }
};
```

构造函数接收格式串 `s`，实例化 `checker` 对象，将 checker 作为 handler 传入 `parse_format_string`——这是逐字符扫描格式串的编译期驱动函数。

#### format\_string\_checker 状态机内部

```cpp
// include/fmt/base.h — 编译期格式串检查器
template <typename Char, int NUM_ARGS, int NUM_NAMED_ARGS, bool DYNAMIC_NAMES>
class format_string_checker {
private:
  type types_[max_of<size_t>(1, NUM_ARGS)];              // 每个参数的类型枚举
  named_arg_info<Char> named_args_[max_of<size_t>(1, NUM_NAMED_ARGS)];
  compile_parse_context<Char> context_;                   // 携带参数数量和类型数组
  using parse_func = auto (*)(parse_context<Char>&) -> const Char*;
  parse_func parse_funcs_[max_of<size_t>(1, NUM_ARGS)];  // 每个参数的 parse 函数指针
```

**构造阶段**：通过参数包展开，将每个 `Args` 的类型映射为 `detail::type` 枚举（`int_type`、`double_type`、`string_type` 等），存入 `types_[]` 数组。同时为每个参数注册一个 `invoke_parse&lt;T, Char&gt;` 函数指针，该函数在编译期调用对应 `formatter&lt;T&gt;::parse()` 验证格式规范合法性。

**扫描阶段**：`parse_format_string` 逐字符扫描，遇到 `{` 则进入替换字段解析。checker 提供三个 `on_arg_id` 钩子：

- `on_arg_id()` — 自动索引模式（`{}`），递增 `next_arg_id_` 计数器，返回当前参数索引
- `on_arg_id(int id)` — 显式索引模式（`{0}`），验证 `id &lt; NUM_ARGS`
- `on_arg_id(basic_string_view id)` — 命名参数模式（`{name}`），在 `named_args_[]` 数组中查找

`compile_parse_context` 继承自 `parse_context`，额外持有 `num_args_` 和 `types_[]` 指针。其 `next_arg_id()` 和 `check_arg_id(int)` 方法在索引越界时调用 `report_error()`——在 `consteval` 上下文中这触发编译错误。

**类型兼容性验证**：`on_format_specs(int id, begin, end)` 方法调用 `parse_funcs_[id](context_)`，即对应参数的 `formatter::parse()`。`parse_format_specs` 函数内部维护一个 `state` 枚举状态机（`start → align → sign → hash → zero → width → precision → locale`），对每个格式说明符字符验证：

```cpp
// include/fmt/base.h — parse_format_specs 内的类型集合校验
constexpr auto integral_set = sint_set | uint_set | bool_set | char_set;
// ...
case 'd': return parse_presentation_type(pres::dec, integral_set);
case 'f': return parse_presentation_type(pres::fixed, float_set);
case 's': return parse_presentation_type(pres::string,
                                         bool_set | string_set | cstring_set);
```

`parse_presentation_type` 通过 `in(arg_type, set)` 位运算校验：若参数类型不在允许集合内，调用 `report_error("invalid format specifier")`，在编译期产生硬错误。

```cpp
// 类型位集合定义
constexpr auto set(type rhs) -> int { return 1 << int(rhs); }
constexpr auto in(type t, int set) -> bool {
  return ((set >> int(t)) & 1) != 0;
}
```

**混合索引检测**：自动索引和显式索引不能混用。`next_arg_id_` 初始为 0，自动索引时递增；一旦调用 `check_arg_id`（显式索引），设为 -1。后续任何 `next_arg_id()` 调用发现 `next_arg_id_ &lt; 0` 即报错"cannot switch from manual to automatic argument indexing"。

**C++11 降级模式**：当 `FMT_USE_CONSTEVAL == 0` 时（GCC &lt; 10、Clang &lt; 11），`FMT_CONSTEVAL` 展开为空，`consteval` 检查退化为运行期断言。用户可通过 `FMT_STRING` 宏强制 `constexpr` 字符串（已废弃）。

#### 实际编译期检查效果

```cpp
auto s1 = fmt::format("Hello, {}! You have {} messages.", name, count);
auto s2 = fmt::format("Value: {:d}", "hello");
//  ❌ 编译失败——在 format_string_checker 中，
//    string_type 不在 integral_set 内，触发 report_error
auto s3 = fmt::format(fmt::runtime(tpl), value);
//  ✅ 运行期格式串，绕过 consteval 检查
```

### format\_arg 类型擦除：tagged union + visit 分发

fmt 不能为每种参数组合生成独立的模板特化——会导致代码膨胀。取而代之的是**类型擦除**：所有参数通过 `value&lt;Context&gt;` union 捕获，运行时通过 `basic_format_arg::visit()` 分发。

#### value 联合体定义

```cpp
// include/fmt/base.h — 值存储联合体
template <typename Context> class value {
public:
  union {
    monostate no_value;
    int int_value;
    unsigned uint_value;
    long long long_long_value;
    ullong ulong_long_value;
    native_int128 int128_value;
    native_uint128 uint128_value;
    bool bool_value;
    char_type char_value;
    float float_value;
    double double_value;
    long double long_double_value;
    const void* pointer;
    string_value<char_type> string;       // {const Char* data; size_t size;}
    custom_value<Context> custom;         // {void* value; void (*format)(...);}
    named_arg_value<char_type> named_args;
  };
```

联合体中最大的成员是 `long double`（通常 16 字节对齐后 16 字节）和 `int128_value`（16 字节），所以整个 union 占 16 字节——即 128 位值存储。

#### type 枚举

```cpp
// include/fmt/base.h — 类型标签枚举
enum class type {
  none_type,
  int_type, uint_type, long_long_type, ulong_long_type,
  int128_type, uint128_type, bool_type, char_type,
  last_integer_type = char_type,
  float_type, double_type, long_double_type,
  last_numeric_type = long_double_type,
  cstring_type, string_type, pointer_type,
  custom_type
};
```

枚举值编码了类型层级关系：`is_integral_type(t)` 检查 `t &gt; none_type &amp;&amp; t &lt;= last_integer_type`；`is_arithmetic_type(t)` 检查 `t &lt;= last_numeric_type`。`parse_format_specs` 中的格式说明符校验（`{:d}` 只接受整数类型，`{:.2f}` 只接受浮点类型）就依赖这些位集合运算。

#### basic\_format\_arg：包装 type + value

```cpp
// include/fmt/base.h — 类型擦除的参数句柄
template <typename Context> class basic_format_arg {
private:
  detail::value<Context> value_;
  detail::type type_;
```

构造时通过 `stored_type_constant&lt;T, Context&gt;::value` 在编译期计算类型标签，存入 `type_`。

#### visit() switch 分发

```cpp
// include/fmt/base.h — 运行时类型分发
template <typename Visitor>
FMT_CONSTEXPR FMT_INLINE auto visit(Visitor&& vis) const {
  switch (type_) {
  case detail::type::none_type:        break;
  case detail::type::int_type:         return vis(value_.int_value);
  case detail::type::uint_type:        return vis(value_.uint_value);
  case detail::type::long_long_type:   return vis(value_.long_long_value);
  case detail::type::ulong_long_type:  return vis(value_.ulong_long_value);
  case detail::type::int128_type:      return vis(map(value_.int128_value));
  case detail::type::uint128_type:     return vis(map(value_.uint128_value));
  case detail::type::bool_type:        return vis(value_.bool_value);
  case detail::type::char_type:        return vis(value_.char_value);
  case detail::type::float_type:       return vis(value_.float_value);
  case detail::type::double_type:      return vis(value_.double_value);
  case detail::type::long_double_type: return vis(value_.long_double_value);
  case detail::type::cstring_type:     return vis(value_.string.data);
  case detail::type::string_type:      return vis(value_.string.str());
  case detail::type::pointer_type:     return vis(value_.pointer);
  case detail::type::custom_type:      return vis(handle(value_.custom));
  }
  return vis(monostate());
}
```

`visit()` 是一个 `constexpr` 内联函数，覆盖所有 15 种类型分支。对内建类型直接传递联合体成员；对 `custom_type` 则包装为 `basic_format_arg::handle` 对象。

#### custom\_formatter 闭包机制

当用户特化 `fmt::formatter&lt;T&gt;` 时，`value` 的构造函数检测 `use_formatter&lt;T&gt;` 为 true，进入 `custom_tag` 路径：

```cpp
// include/fmt/base.h — 自定义类型的类型擦除入口
template <typename T, FMT_ENABLE_IF(has_formatter<T, char_type>())>
FMT_CONSTEXPR value(T& x, custom_tag) {
  custom.value = const_cast<char*>(
      &reinterpret_cast<const volatile char&>(x));
  custom.format = format_custom<value_type>;  // 静态函数指针
}

template <typename T>
static void format_custom(void* arg, parse_context<char_type>& parse_ctx,
                          Context& ctx) {
  auto f = formatter<T, char_type>();
  parse_ctx.advance_to(f.parse(parse_ctx));
  const auto& cf = f;
  ctx.advance_to(cf.format(*static_cast<T*>(arg), ctx));
}
```

`custom_value` 是一个 `{void* value; void (*format)(...);}` 对——函数指针 `format` 指向 `format_custom&lt;T&gt;`，该模板函数在编译期为每个类型 `T` 生成一个实例。运行时，`visit()` 的 `custom_type` 分支构造 `handle` 对象，其 `format()` 方法通过函数指针回调 `formatter&lt;T&gt;::parse()` + `format()`。

#### packed 参数优化

当参数数量 ≤ `max_packed_args`（15 个）时，类型标签被编码进一个 `ullong desc_` 描述符的低位（每参数 4 位），参数值直接存储在 `value&lt;Context&gt;` 数组中，避免了 `basic_format_arg` 对象的间接寻址。超过 15 个参数则使用 unpacked 模式——desc\_ 高位设 `is_unpacked_bit`，存储指向 `basic_format_arg` 数组的指针。

### 动态宽度/精度与自定义格式化器

宽度和精度可以是字面量（编译期提取）或参数索引（运行时获取）：

```cpp
fmt::format("{:10}", 42);              // "        42"  字面量宽度
fmt::format("{:{}}", 42, 10);          // "        42"  动态宽度，从第 2 个参数取
fmt::format("{:.{}f}", 3.14159, 2);    // "3.14"       动态精度
```

解析时，`parse_dynamic_spec` 检测到 `{` 后递归进入参数 ID 解析，将宽度/精度绑定为 `arg_ref&lt;Char&gt;`（一个 union，存储 `int index` 或 `basic_string_view name`），同时标记 `arg_id_kind::index`。运行时格式化阶段再通过 `basic_format_args::get()` 获取实际整数值。

`compile_parse_context::check_dynamic_spec(int arg_id)` 在编译期验证：被引用作宽度/精度的参数必须是整数类型，否则报错"width/precision is not integer"。

用户通过特化 `fmt::formatter&lt;T&gt;` 为自定义类型添加格式化支持。`parse()` 在编译期解析格式规范，`format()` 在运行期输出：

```cpp
struct Point { double x, y; };

template <>
struct fmt::formatter&lt;Point&gt; : fmt::formatter&lt;std::string_view&gt; {
    auto parse(format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}')
            throw format_error("invalid format spec for Point");
        return it;
    }
    auto format(const Point& p, format_context& ctx) const {
        return fmt::format_to(ctx.out(), "({}, {})", p.x, p.y);
    }
};
// fmt::format("point = {}", Point{3.14, 2.72})  →  "point = (3.14, 2.72)"
```

`parse()` 在首次调用时缓存结果，不会为每条日志消息重复解析格式规范。

### 浮点数格式化：Dragonbox 算法

fmt 使用 **Dragonbox**（[fmt.dev/papers/Dragonbox.pdf](https://fmt.dev/papers/Dragonbox.pdf)）进行浮点数→字符串转换，由 Junekey Jeon 设计，是当前已知最快的正确舍入算法：

| 算法 | 正确舍入 | 最短表示 | 速度 | 备注 |
|------|---------|---------|------|------|
| Grisu2 | 否 | ~99.5% | 快 | 约 0.5% 的值需 fallback 到 sprintf |
| Ryu | 是 | 100% | 较快 | 需要更大的查找表 |
| **Dragonbox** | **是** | **100%** | **最快** | 表最小，指令最少 |

#### 核心数学原理

IEEE 754 双精度浮点数表示为 $x = m \times 2^e$，其中 $m$ 是尾数、$e$ 是指数。Dragonbox 的目标：找到最短十进制表示 $(M, k)$ 使得 $M \times 10^k$ 回舍入后等于原始浮点值。

关键洞察：传统算法（如 Grisu 系列）需要大整数运算来精确判断区间边界。Dragonbox 通过精心选择缩放因子，将问题转化为**有界宽度的整数乘法 + 移位**，完全避免任意精度算术。

具体步骤：

1. **IEEE 754 解包**：将浮点位模式拆分为符号、指数、尾数
2. **区间计算**：计算浮点值对应的十进制候选区间 $[c^-, c^+]$，即相邻浮点值的中点
3. **缩放与乘法**：用预计算的 $5^q$ 缩放因子（来自查找表）将二进制区间映射到十进制。核心操作是**两次 128 位乘法**（`uint128 × uint64 → upper bits`）加上一次移位
4. **商与余数**：乘法结果的高位给出十进制有效数字 $M$，低位余数用于判断是否需要调整
5. **最短化**：逐次除以 10 去掉尾部零，同时检查舍入不变性

#### 查找表结构

Dragonbox 预计算了一组与 $5^q$ 相关的缩放常数。在 fmt 的实现中，这些表以 `cache_entry` 类型存储，双精度版本需要覆盖指数范围 $[-343, +347]$。每个条目是一个 128 位值（两个 `uint64_t`），共约 **128 个条目 × 16 字节 ≈ 2 KB**（双精度主表）；加上单精度条目和其他辅助常数表，总表大小约 **6 KB**。

```cpp
// fmt 内部 Dragonbox 表结构（概念简化）
struct cache_entry {
  uint64_t high;   // 高 64 位
  uint64_t low;    // 低 64 位
};
static constexpr cache_entry cache[] = { /* ~128 entries */ };
```

**运行时路径**：`dragonbox::to_chars_n` 函数对一个 `double` 的完整格式化仅需：
- 1 次整数除法（确定十进制指数）
- 2 次乘法（缩放映射）
- 1 次表查找（获取缩放因子）
- 若干移位和比较

**零分配**：输出直接写入预分配的字符缓冲区，不需要临时 `std::string`。

### memory\_buffer：栈分配零堆格式化

`memory_buffer`（全名 `basic_memory_buffer&lt;char&gt;`）是 fmt 零堆分配策略的核心：

```cpp
// include/fmt/format.h
enum { inline_buffer_size = 500 };  // 栈内缓冲区大小

template &lt;typename T, size_t SIZE = inline_buffer_size,
          typename Allocator = detail::allocator&lt;T&gt;&gt;
class basic_memory_buffer : public detail::buffer&lt;T&gt; {
private:
  T store_[SIZE];  // 500 字节栈数组，零堆分配
```

**设计要点**：

1. **栈优先**：`store_[500]` 是对象内嵌的 `char[500]` 数组，构造时 `set(store_, SIZE)` 直接指向自身存储，**不调用 `malloc`**
2. **溢出回退**：当格式化结果超过 500 字节时，`grow()` 静态方法以 1.5 倍速率扩容，通过 allocator 分配堆内存。新旧数据通过 `memcpy` 迁移，旧内存（如果不是栈内存储）立即释放
3. **零初始化优化**：`buffer` 基类的 `ptr_` 成员**故意不初始化**以节省周期（MSVC 版本除外，因有 `-W26495` 警告）
4. **模板参数化**：`SIZE` 可自定义，spdlog 内部使用 `memory_buf_t = fmt::basic_memory_buffer&lt;char, 250&gt;`——缩减为 250 字节以适配 spdlog 的栈帧

**buffer 基类**的设计同样精巧：

```cpp
template &lt;typename T&gt; class buffer {
private:
  T* ptr_;
  size_t size_;
  size_t capacity_;
  using grow_fun = void (*)(buffer& buf, size_t capacity);
  grow_fun grow_;  // 函数指针，避免虚函数开销
```

`grow_` 是一个存储增长策略的函数指针——不同子类（`basic_memory_buffer`、`iterator_buffer`、`container_buffer`）注入不同的增长逻辑，既实现了多态又避免了虚表指针。

**热路径影响**：在 `fmt::format` 的典型调用中，`basic_memory_buffer` 的构造、写入、析构全程在栈上完成，**不触发任何堆分配**。实测对 &lt; 50 字符的短字符串（覆盖绝大多数日志行），比 `std::format`（依赖 `std::string` 堆分配）快 10-30%。

### 与 std::format 的对比

`std::format`（C++20）的规范直接基于 fmt 的设计，两者 API 表面几乎相同：

| 维度 | fmt | std::format |
|------|-----|-------------|
| 编译期检查 | `format_string&lt;Args...&gt;` 的 `consteval` 构造函数强制检查 | 标准仅要求"不匹配导致 UB"，实现各异 |
| 诊断质量 | 精确指向第几个参数、具体类型不匹配 | MSVC STL 较好；libstdc++ 较差 |
| 浮点格式化 | Dragonbox（整数算术 + 6KB 查找表） | 依赖实现，通常 Grisu2 或 Ryu |
| 栈分配 | `memory_buffer` 默认 500 字节栈缓冲 | 实现各异，通常依赖 `std::string` |
| 异常类型 | `fmt::format_error` | `std::format_error` |
| 最低要求 | C++11（降低模式） | 完整 C++20 支持 |

**fmt 仍不可替代**：（1）GCC 13+ / Clang 17+ 才完整实现 `&lt;format&gt;`；（2）fmt 的编译期诊断远优于标准库实现；（3）新特性（如 `chrono` 格式化）在标准之前就可用。

### 字符串编码处理

fmt 以**字节序列**处理字符串，不假设特定编码。对于 UTF-8，宽度计算按 Unicode 显示宽度（非字节数），内置基于 Unicode 表的宽度函数，正确处理 CJK 全角字符：

```cpp
fmt::format("{:<10}", "Hi");     // "Hi        "（8 个空格）
fmt::format("{:<10}", "你好");    // "你好      "（6 个空格，每个中文宽度 2）
```

---

## spdlog 核心设计


### spdlog 三层架构: Logger → Sink → Formatter

```
  用户代码
  spdlog::info("Processed: {}", request)
       |
       v
  +---------------------------------------------------+
  |                   Logger 层                        |
  |                                                   |
  |  logger->log(source_loc, level, fmt, args...)     |
  |                                                   |
  |  1. should_log(level) 运行时级别检查               |
  |  2. memory_buf_t buf (250 字节栈缓冲)             |
  |  3. fmt::vformat_to(buf, fmt, args...)            |
  |  4. 构造 log_msg {loc, name, level, msg_view}    |
  +---------------------------+                       |
                              |                       |
                              v                       |
  +---------------------------------------------------+
  |                   Sink 层                          |
  |                                                   |
  |  一个 Logger 可绑定多个 Sink (多路输出)             |
  |                                                   |
  |  +------------------+  +---------------------+    |
  |  | stdout_color     |  | rotating_file       |    |
  |  | _sink_mt         |  | _sink_mt            |    |
  |  | ANSI 彩色终端     |  | 按大小轮转           |    |
  |  +------------------+  | (10MB x 3 文件)     |    |
  |                        +---------------------+    |
  |  +------------------+  +---------------------+    |
  |  | daily_file       |  | basic_file          |    |
  |  | _sink_mt         |  | _sink_mt            |    |
  |  | 按日期轮转        |  | 简单追加写入         |    |
  |  +------------------+  +---------------------+    |
  |                                                   |
  |  sink_it_(log_msg):                               |
  |  1. formatter_->format(log_msg, formatted)        |
  |  2. sink->write(formatted)                        |
  +---------------------------+                       |
                              |                       |
                              v                       |
  +---------------------------------------------------+
  |                 Formatter 层                       |
  |                                                   |
  |  默认 pattern: "[%Y-%m-%d %H:%M:%S.%e] [%n]     |
  |                 [%^%l%$] %v"                      |
  |                                                   |
  |  +-----------+  +-----------+  +-----------+      |
  |  | flag      |  | aggregate |  | formatter |      |
  |  | formatter |  | formatter |  | (组合器)   |      |
  |  +-----------+  +-----------+  +-----------+      |
  |  | %Y 年     |  | %^ 颜色起 |  | 按 pattern|      |
  |  | %m 月     |  | %$ 颜色止 |  | 字符串    |      |
  |  | %H 时     |  | %+ 完整   |  | 顺序输出  |      |
  |  | %l 级别   |  |           |  |           |      |
  |  | %v 正文   |  |           |  |           |      |
  |  +-----------+  +-----------+  +-----------+      |
  |                                                   |
  |  输出: [2024-01-15 10:30:45.123] [app] [info]     |
  |        Processed: GET /api/users -> 200           |
  +---------------------------------------------------+

  异步模式额外层:

  Logger -> mpmc_blocking_queue (mutex + condvar)
         -> 后台消费者线程 -> Sink -> Formatter

  队列溢出策略:
  +------------------+-------------------------------+
  | block            | 阻塞生产者直到队列有空间       |
  | overrun_oldest   | 覆盖最旧的消息                |
  | discard_new      | 丢弃当前消息, 递增丢弃计数器   |
  +------------------+-------------------------------+
```
### 架构：Logger → Sink → Formatter

**Logger** 是用户交互的唯一入口，持有名称和日志级别阈值。**Sink** 是输出目标抽象，一个 logger 可绑定多个 sink。**Formatter** 定义输出格式。

spdlog 内置 sink：

| Sink | 用途 |
|------|------|
| `stdout_color_sink_mt` | 彩色终端（ANSI 转义码） |
| `rotating_file_sink_mt` | 按大小轮转（默认 10 MB / 3 文件） |
| `daily_file_sink_mt` | 按日期轮转 |
| `basic_file_sink_mt` | 简单追加 |
| `syslog_sink_mt` | POSIX syslog |
| `null_sink_mt` | 丢弃（用于测试或条件日志） |

默认输出格式：`[2024-01-15 10:30:45.123] [logger_name] [info] message`

自定义格式通过模式字符串：`spdlog::set_pattern("[%H:%M:%S] [%n] [%^%l%$] %v")`，其中 `%^`/`%$` 为颜色起止，`%l` 级别，`%v` 消息正文。

### 异步 Logger 与 mpmc\_blocking\_queue

异步模式下消息入队到 `mpmc_blocking_queue`，后台线程消费写入 sink。队列实现位于 `include/spdlog/details/mpmc_blocking_q.h`。

#### 源码级实现

`mpmc_blocking_queue` 是一个 **基于 mutex + condition\_variable 的多生产者-多消费者阻塞队列**——严格来说**不是** lock-free 的：

```cpp
// include/spdlog/details/mpmc_blocking_q.h
template &lt;typename T&gt;
class mpmc_blocking_queue {
public:
    explicit mpmc_blocking_queue(size_t max_items)
        : q_(max_items) {}   // circular_q 预分配固定槽位

private:
    std::mutex queue_mutex_;
    std::condition_variable push_cv_;   // 消费者等待：队列非空
    std::condition_variable pop_cv_;    // 生产者等待：队列非满
    spdlog::details::circular_q&lt;T&gt; q_; // 固定大小环形缓冲区
    std::atomic&lt;size_t&gt; discard_counter_{0};
};
```

底层存储是 `circular_q&lt;T&gt;`——一个预分配的固定大小环形缓冲区。当队列满时 `push_back` 覆盖最旧元素并递增 overrun 计数器。

#### 三种入队策略对应三种溢出策略

```cpp
// include/spdlog/details/mpmc_blocking_q.h

// 策略 1：block — 阻塞生产者直到有空间
void enqueue(T&& item) {
    {
        std::unique_lock&lt;std::mutex&gt; lock(queue_mutex_);
        pop_cv_.wait(lock, [this] { return !this-&gt;q_.full(); });
        q_.push_back(std::move(item));
    }
    push_cv_.notify_one();   // 唤醒一个消费者
}

// 策略 2：overrun_oldest — 立即入队，覆盖最旧消息
void enqueue_nowait(T&& item) {
    {
        std::unique_lock&lt;std::mutex&gt; lock(queue_mutex_);
        q_.push_back(std::move(item));   // circular_q 满时自动覆盖最旧
    }
    push_cv_.notify_one();
}

// 策略 3：discard_new — 丢弃当前消息
void enqueue_if_have_room(T&& item) {
    bool pushed = false;
    {
        std::unique_lock&lt;std::mutex&gt; lock(queue_mutex_);
        if (!q_.full()) {
            q_.push_back(std::move(item));
            pushed = true;
        }
    }
    if (pushed) {
        push_cv_.notify_one();
    } else {
        ++discard_counter_;   // 原子计数，外部可查询丢弃数
    }
}
```

#### 消费者侧

```cpp
// 带超时的出队——后台消费者线程使用
bool dequeue_for(T& popped_item, std::chrono::milliseconds wait_duration) {
    {
        std::unique_lock&lt;std::mutex&gt; lock(queue_mutex_);
        if (!push_cv_.wait_for(lock, wait_duration,
                               [this] { return !this-&gt;q_.empty(); })) {
            return false;   // 超时
        }
        popped_item = std::move(q_.front());
        q_.pop_front();
    }
    pop_cv_.notify_one();   // 唤醒一个阻塞的生产者
    return true;
}
```

**线程池映射**（`details/thread_pool-inl.h`）：

```cpp
// 溢出策略 → 队列方法的映射
switch (overflow_policy) {
    case block:         q_.enqueue(std::move(item));           break;
    case overrun_oldest: q_.enqueue_nowait(std::move(item));  break;
    case discard_new:   q_.enqueue_if_have_room(std::move(item)); break;
}
```

**性能特征**：mutex + CV 的方案在低竞争时开销极低（一次 lock/unlock + 一次 notify），但在极高并发（> 32 线程）下锁竞争会成为瓶颈。spdlog 选择此方案而非 lock-free 队列，是因为 lock-free 的 MPMC 需要更复杂的 ABA 防护和内存序管理，而 mutex + CV 的正确性和可移植性更有保障。

#### 构造方式

```cpp
spdlog::init_thread_pool(8192, 1);  // 队列容量 8192，1 个消费者线程
auto async_logger = spdlog::async_logger_mt(
    "async_file",
    std::make_shared&lt;spdlog::sinks::basic_file_sink_mt&gt;("log.txt"),
    spdlog::async_overflow_policy::block);
```

### 编译期级别过滤：SPDLOG\_ACTIVE\_LEVEL 宏

`SPDLOG_ACTIVE_LEVEL` 宏在编译期**完全裁剪**低级别日志——被裁剪的代码在二进制中不存在。

#### 源码级机制

在 `include/spdlog/common.h` 中定义级别常量和默认激活级别：

```cpp
// include/spdlog/common.h
#define SPDLOG_LEVEL_TRACE    0
#define SPDLOG_LEVEL_DEBUG    1
#define SPDLOG_LEVEL_INFO     2
#define SPDLOG_LEVEL_WARN     3
#define SPDLOG_LEVEL_ERROR    4
#define SPDLOG_LEVEL_CRITICAL 5
#define SPDLOG_LEVEL_OFF      6

#if !defined(SPDLOG_ACTIVE_LEVEL)
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO   // 默认：INFO 及以上
#endif
```

在 `include/spdlog/spdlog.h` 中，每个级别的宏通过 `#if` 预处理条件分支定义：

```cpp
// include/spdlog/spdlog.h — 编译期级别过滤的核心宏

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
#define SPDLOG_LOGGER_TRACE(logger, ...) \
    SPDLOG_LOGGER_CALL(logger, spdlog::level::trace, __VA_ARGS__)
#define SPDLOG_TRACE(...) SPDLOG_LOGGER_TRACE(spdlog::default_logger_raw(), __VA_ARGS__)
#else
#define SPDLOG_LOGGER_TRACE(logger, ...) (void)0   // 编译期替换为空操作
#define SPDLOG_TRACE(...) (void)0
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG
#define SPDLOG_LOGGER_DEBUG(logger, ...) \
    SPDLOG_LOGGER_CALL(logger, spdlog::level::debug, __VA_ARGS__)
#define SPDLOG_DEBUG(...) SPDLOG_LOGGER_DEBUG(spdlog::default_logger_raw(), __VA_ARGS__)
#else
#define SPDLOG_LOGGER_DEBUG(logger, ...) (void)0
#define SPDLOG_DEBUG(...) (void)0
#endif
```

**关键区分**：

- `SPDLOG_DEBUG(...)` / `SPDLOG_LOGGER_DEBUG(logger, ...)` — **宏调用**，在 `#else` 分支中被替换为 `(void)0`，编译器优化器直接消除。二进制中不存在格式化代码、参数求值代码、函数调用代码
- `logger-&gt;debug(...)` — **虚函数调用**，虽然运行时 `should_log()` 检查级别后会早返回，但格式串模板实例化、参数包展开、`fmt::format_string` 的 `consteval` 检查**仍然发生**，生成对应的机器码

**SPDLOG\_LOGGER\_CALL** 传递源码位置信息（文件名、行号、函数名）：

```cpp
#ifndef SPDLOG_NO_SOURCE_LOC
#define SPDLOG_LOGGER_CALL(logger, level, ...) \
    (logger)->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, \
                  level, __VA_ARGS__)
#else
#define SPDLOG_LOGGER_CALL(logger, level, ...) \
    (logger)->log(spdlog::source_loc{}, level, __VA_ARGS__)
#endif
```

**使用示例**：

```cpp
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_WARN
#include &lt;spdlog/spdlog.h&gt;

void process() {
    SPDLOG_DEBUG("removed at compile time");  // (void)0 — 不存在于二进制
    SPDLOG_INFO("also removed");              // (void)0
    SPDLOG_WARN("this stays");                // SPDLOG_LOGGER_CALL(...)
}
```

**注意**：`SPDLOG_ACTIVE_LEVEL` 宏必须在 `#include &lt;spdlog/spdlog.h&gt;` 之前定义，因为条件编译在头文件包含时就已经确定。

### 性能设计

#### 零分配模式

spdlog 的性能核心是确保常见路径零堆分配：

1. **栈缓冲区预分配**：每条日志使用 `fmt::basic_memory_buffer&lt;char, 250&gt;`（250 字节栈数组），仅超长消息才 fallback 堆分配。注意 spdlog 将 `inline_buffer_size` 从 fmt 默认的 500 缩减为 250——日志行通常比通用格式化短得多
2. **sink 内部**同样使用 `memory_buffer` 而非 `std::string`
3. **异步队列**使用 `circular_q` 预分配固定大小槽位
4. **时间戳缓存**：同一秒内的多条日志共享格式化后的时间字符串

```cpp
// spdlog logger::log_() 内部简化
template &lt;typename... Args&gt;
void log_(source_loc loc, level::level_enum lvl, string_view_t fmt, Args&&... args) {
    bool log_enabled = should_log(lvl);
    bool traceback_enabled = tracer_.enabled();
    if (!log_enabled && !traceback_enabled) return;  // 运行时级别检查

    memory_buf_t buf;   // 250 字节栈缓冲区，零堆分配
    fmt::vformat_to(fmt::appender(buf), fmt, fmt::make_format_args(args...));
    details::log_msg log_msg(loc, name_, lvl,
                             string_view_t(buf.data(), buf.size()));
    log_it_(log_msg, log_enabled, traceback_enabled);
}
```

#### Benchmark 参考

10 线程并发，每线程 100 万条日志，旋转文件 sink：

| 库 | 吞吐量 |
|----|--------|
| spdlog async | ~18M msg/s |
| spdlog sync | ~8M msg/s |
| glog | ~2M msg/s |
| Boost.Log | ~1M msg/s |

### 与标准库及其他日志库的对比

| 维度 | spdlog | glog (Google) | Boost.Log | C++26 提案 |
|------|--------|---------------|-----------|-----------|
| 格式化 | fmt（编译期类型安全） | `printf` 风格 | Boost.Format | `std::format` |
| 异步 | 原生线程池 + mpmc\_blocking\_queue | 无 | async sink | 未明确 |
| 编译期裁剪 | `SPDLOG_ACTIVE_LEVEL` 宏消除代码 | `GOOGLE_LOG` 宏 | 运行时控制 | 未明确 |
| 依赖 | header-only，仅 fmt | 独立库，依赖 gflags | Boost 子库 | 标准库 |
| 性能 | 最优 | 中等 | 中等偏低 | 取决于实现 |

P3394（C++26 提案）正在推进标准化日志接口，截至 N5008 草案尚未进入标准。spdlog 的设计（类型安全格式化 + sink 抽象 + 编译期过滤）很可能影响最终标准。

---

## 实际使用示例

### fmt 基础格式化

```cpp
#include <fmt/format.h>
#include <fmt/chrono.h>
#include <fmt/ranges.h>

int main() {
    fmt::print("{1} has {0} apples\n", 5, "Alice");           // 位置参数
    fmt::print("{:>10.4f}\n", 3.14159);                       // "    3.1416"
    fmt::print("{:#08x}\n", 255);                              // "0x0000ff"
    fmt::print("{:b}\n", 42);                                  // "101010"

    auto now = std::chrono::system_clock::now();
    fmt::print("{:%Y-%m-%d %H:%M:%S}\n", now);                // 类型安全的 chrono 格式化

    std::vector&lt;int&gt; v{1, 2, 3, 4, 5};
    fmt::print("{}\n", v);                                    // "[1, 2, 3, 4, 5]"
}
```

### spdlog 多 Sink + 自定义类型

```cpp
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

struct Request { std::string method, path; int status; };

template <>
struct fmt::formatter&lt;Request&gt; : fmt::formatter&lt;std::string_view&gt; {
    auto format(const Request& r, format_context& ctx) const {
        return fmt::format_to(ctx.out(), "{} {} -> {}", r.method, r.path, r.status);
    }
};

int main() {
    auto console = std::make_shared&lt;spdlog::sinks::stdout_color_sink_mt&gt;();
    auto file = std::make_shared&lt;spdlog::sinks::rotating_file_sink_mt&gt;(
        "logs/app.log", 1024 * 1024 * 10, 3);  // 10 MB × 3 文件

    auto logger = std::make_shared&lt;spdlog::logger&gt;("app",
        spdlog::sinks_init_list{console, file});
    spdlog::set_default_logger(logger);

    spdlog::info("Processed: {}", Request{"GET", "/api/users", 200});
    spdlog::error("Failed: {}", Request{"POST", "/upload", 500});
}
```

### 异步日志 + 编译期裁剪

```cpp
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>

int main() {
    spdlog::init_thread_pool(8192, 1);
    auto sink = std::make_shared&lt;spdlog::sinks::basic_file_sink_mt&gt;("async.log");
    auto logger = std::make_shared&lt;spdlog::async_logger&gt;(
        "async", sink, spdlog::thread_pool(),
        spdlog::async_overflow_policy::block);

    for (int i = 0; i < 1000000; ++i)
        SPDLOG_LOGGER_INFO(logger, "iteration {}", i);  // 编译期可裁剪

    spdlog::shutdown();  // 等待队列排空
}
```
