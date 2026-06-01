// Solution: inlinevars1 — 内联变量
#include "cpplings.h"
#include <string>

inline const int kGlobalVersion = 17;

inline int counter = 0;

struct Config {
    inline static const std::string appName = "cpplings";
    inline static const int maxRetries = 3;
};

TEST("inline — 命名空间级常量") {
    ASSERT_EQ(kGlobalVersion, 17);
}

TEST("inline — static 类成员") {
    ASSERT_EQ(Config::appName, "cpplings");
    ASSERT_EQ(Config::maxRetries, 3);
}

TEST("inline — static 成员无需实例") {
    ASSERT_EQ(Config::maxRetries, 3);
    Config c;
    ASSERT_EQ(c.maxRetries, 3);
}

TEST("inline — 可变 inline 变量") {
    // 重置（测试顺序可能不确定）
    counter = 0;
    ASSERT_EQ(counter, 0);
    counter++;
    ASSERT_EQ(counter, 1);
    counter = 42;
    ASSERT_EQ(counter, 42);
}

CPPLINGS_MAIN
