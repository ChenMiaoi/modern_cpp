# C++98

C++98（ISO/IEC 14882:1998）是 C++ 语言的第一个国际标准。它在 C with Classes 和早期 C++ 实践的基础上，正式确立了语言的核心框架。

## 历史背景

- **标准化日期**：1998 年
- **前身**：Bjarne Stroustrup 在 1979 年开始设计，经历了 Cfront 等早期实现
- **意义**：将泛型编程、面向对象编程和系统级编程能力统一在一门语言中

## 核心特性

| 特性 | 说明 |
|------|------|
| 类与继承 | 单继承、多继承、虚函数、抽象基类 |
| 模板 | 函数模板、类模板（但不支持偏特化的全部场景） |
| 异常处理 | `try`/`catch`/`throw` 机制 |
| 命名空间 | `namespace` 用于避免名称冲突 |
| RTTI | `typeid`、`dynamic_cast` 运行时类型识别 |
| `const` 与引用 | `const` 正确性、左值引用 |
| 运算符重载 | 支持自定义类型的运算符语义 |
| `new` / `delete` | 动态内存管理（替代 C 的 `malloc`/`free`） |

## 标准库组件

- **STL 容器**：`vector`、`list`、`deque`、`map`、`set`、`stack`、`queue`
- **迭代器**：五类迭代器体系
- **算法**：`sort`、`find`、`transform`、`accumulate` 等
- **函数对象**：`less`、`greater`、`bind1st`/`bind2nd`
- **字符串**：`std::string`
- **I/O 流**：`iostream`、`fstream`、`sstream`
- **智能指针**：`auto_ptr`（已废弃）

## 局限性

- 模板能力有限，不支持外部模板显式实例化控制
- 缺乏统一的内存模型和多线程支持
- `auto_ptr` 的拷贝语义存在陷阱
- 没有 `nullptr`，用 `0` 或 `NULL` 表示空指针
- 枚举类型隐式转换，容易导致命名冲突
- Lambda、类型推导等现代特性均不可用

## 延伸阅读

- [语言特性](/standards/cpp98/features)
- [标准库](/standards/cpp98/standard-library)
