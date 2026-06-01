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
