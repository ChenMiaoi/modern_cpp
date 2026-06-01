// Exercise: inlinevars1 — 内联变量
// 使用 C++17 的 inline 变量解决 ODR 问题，实现 header-only 常量和 static 成员。
//
// 任务:
//   1. 理解 inline 变量在头文件中的声明和定义统一
//   2. 使用 inline const 变量作为命名空间级常量
//   3. 使用 inline static 成员作为类级常量
//   4. 理解 inline 变量保证所有翻译单元看到同一地址
// 提示: C++17 前，头文件中的全局 const 需要 extern + 定义分离
//       inline 变量允许在头文件中同时声明和定义

#include "cpplings.h"
#include <string>

// TODO: 定义一个 inline 命名空间级常量
// 提示: inline const int kGlobalVersion = 17;
// C++17 前这在头文件中会导致多重定义，现在 inline 保证单一实例

TEST("inline — 命名空间级常量") {
    // TODO: 使用你定义的 inline 常量
    int _todo_ = "请删除此行，实现上面的 TODO";
    // ASSERT_EQ(kGlobalVersion, 17);
}

// TODO: 定义一个有 inline static 成员的类
// 提示:
//   struct Config {
//       inline static const std::string appName = "cpplings";
//       inline static const int maxRetries = 3;
//   };

TEST("inline — static 类成员") {
    // TODO: 使用 Config 类的 inline static 成员
    int _todo_ = "请删除此行，实现上面的 TODO";
    // ASSERT_EQ(Config::appName, "cpplings");
    // ASSERT_EQ(Config::maxRetries, 3);
}

TEST("inline — static 成员无需实例") {
    // TODO: 直接通过类名访问，无需创建对象
    int _todo_ = "请删除此行，实现上面的 TODO";
    // ASSERT_EQ(Config::maxRetries, 3);
    // Config c;
    // ASSERT_EQ(c.maxRetries, 3);  // 也可以通过实例访问
}

TEST("inline — 可变 inline 变量") {
    // TODO: 定义 inline 变量（非 const，演示可变 inline 变量）
    // 提示: inline int counter = 0;
    // 修改它并验证
    int _todo_ = "请删除此行，实现上面的 TODO";
    // ASSERT_EQ(counter, 0);
    // counter++;
    // ASSERT_EQ(counter, 1);
    // counter = 42;
    // ASSERT_EQ(counter, 42);
}

CPPLINGS_MAIN
