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

待补：补上 fmt 引擎如何覆盖 `std::format` 的核心语义，以及对编译期格式串检查的额外强化。

## 对象布局

上文已经给出 `value<Context>` tagged union 与 `basic_memory_buffer` 的关键结构；后续补参数存储、handle 与缓冲区的统一布局图。

## 核心源码路径

本文开头已给出 `base.h` 与 `format.h`；后续补 `parse_format_string`、`format_string_checker`、`visit`、`memory_buffer` 的调用链。

## 核心类 / 函数

待补：统一整理 `fstring`、`format_string_checker`、`value<Context>`、`basic_format_arg::visit`、`basic_memory_buffer`。

## 关键算法

正文已经覆盖三阶段流水线；后续补“解析 → 类型检查 → visit 分发 → 输出写入”的关键分支摘要。

## ABI 约束

待补：说明 fmt 以头文件模板与内联实现为主，跨版本兼容更多依赖 API 约定而非稳定 ABI。

## 异常安全

待补：补充分配失败、格式串错误、用户自定义 formatter 抛异常时的传播与资源回收路径。

## iterator / reference invalidation

待补：明确 `basic_format_args` 的借用生命周期，以及 `memory_buffer` 扩容后此前获得的迭代器/指针为何不能继续使用。

## 性能模型

正文已经点出 consteval 检查、16-byte union 与 500-byte 栈缓冲；后续补分支预测、堆分配阈值与 visit 分发成本。

## libstdc++ vs libc++ vs MSVC

待补：这里主要对照三家 `std::format` 引擎在格式串检查、参数表示与缓冲策略上的差异。

## 最小复现代码

```cpp
#include <fmt/format.h>

int main() {
  auto s = fmt::format("Hello, {}! {}", "world", 42);
  return static_cast<int>(s.size());
}
```

## 编译 / 反汇编 / benchmark 证据

待补：补上 consteval 检查路径、参数 visit 分发与 `memory_buffer` 栈内命中率的 benchmark/反汇编证据。

## cpplings 练习入口

- [`format1` — std::format 格式化](../../../exercises/cpp20/format1.cpp)
- [`print23` — std::print / std::println 格式化输出](../../../exercises/cpp23/print23.cpp)
