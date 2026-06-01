# import std

C++23 引入 `import std;`，允许在单条语句中导入整个 C++ 标准库。它是标准库模块化的重要里程碑，显著减少编译时间并简化项目配置。

## 基本用法

```cpp
import std;

int main() {
    std::vector<int> v = {3, 1, 4, 1, 5};
    std::ranges::sort(v);

    for (auto x : v | std::views::reverse) {
        std::println("{}", x);
    }
}
```

等价于不再需要 `#include <vector>`、`#include <algorithm>`、`#include <ranges>`、`#include <print>` 等数十个头文件。

## import std.compat

`import std.compat;` 在 `import std;` 基础上额外提供 C 标准库的全局命名空间名称：

```cpp
import std.compat;

int main() {
    printf("hello %d\n", 42);   // ::printf 直接可用
    auto p = malloc(100);       // ::malloc
    free(p);

    std::vector<int> v = {1, 2, 3};  // C++ 库在 std::
}
```

区别：
- `import std;` — C 函数在 `std::` 命名空间
- `import std.compat;` — C 函数同时在 `::` 和 `std::`

## 编译速度优势

```cpp
// 传统方式：每个翻译单元重复解析头文件
#include <iostream>     // ~30K 行预处理输出
#include <vector>       // ~25K 行
#include <algorithm>    // ~20K 行
// 100 个源文件 = 数百万行重复解析

// 模块方式：编译器加载预编译模块接口
import std;  // 一次编译，多次复用
```

实测编译时间改善（量级参考）：
- 小型项目（< 10 文件）：减少 20-40%
- 中型项目（50-200 文件）：减少 40-60%
- 大型项目（> 500 文件）：减少 50-70%+

## 编译器支持状态

| 编译器 | 版本 | 状态 | 备注 |
|--------|------|------|------|
| MSVC | 17.5+ | 基本支持 | `/std:c++latest` |
| GCC | 14+ | 实验性 | 需手动构建标准库模块 |
| Clang | 18+ | 实验性 | libc++ 支持进行中 |

### CMake 配置

```cmake
cmake_minimum_required(VERSION 3.30)
project(myproject LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
add_executable(app main.cpp)
target_sources(app PRIVATE FILE_SET CXX_MODULES FILES main.cpp)
```

## 与 #include 的对比

| 特性 | `#include` | `import std;` |
|------|-----------|---------------|
| 宏隔离 | 无（宏泄漏） | 是（不导出宏） |
| 编译速度 | 慢（重复解析） | 快（预编译模块） |
| 顺序依赖 | 有 | 无 |
| 符号可见性 | 全部导出 | 按模块规则 |

## 迁移策略

```cpp
// 渐进式迁移
// 新文件: 直接 import std;
import std;
#include "my_header.h"  // 用户头文件仍用 include

// 原有文件: 逐步替换
// 之前
#include <iostream>
#include <vector>
#include <algorithm>
#include <memory>
// 之后
import std;
```

## 宏的注意事项

`import std;` 不导出标准库宏，如需使用需单独 include：

```cpp
#include <cassert>  // assert 宏需要单独 include
#include <cerrno>   // errno 宏
import std;         // 标准库其余部分
```

## 模块接口单元

`import std;` 背后是编译器提供的预编译标准库模块接口，类似：

```cpp
// 编译器内部的模块接口（简化示意）
export module std;
export import std.core;
export import std.algorithm;
export import std.ranges;
export import std.io;
// ...
```

用户不需要关心这些文件，编译器和构建系统自动处理。

## 注意事项

- 不是所有编译器都已完全实现，生产环境需验证
- 宏（`assert`、`errno`、`NDEBUG`）不通过模块导出
- 构建系统需支持 C++23 模块才能正确编译
- 当前仍处于早期采用阶段，建议在新项目中小范围试用
