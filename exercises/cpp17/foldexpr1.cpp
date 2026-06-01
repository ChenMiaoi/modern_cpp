// Exercise: foldexpr1 — 折叠表达式
// 使用 C++17 折叠表达式实现对可变参数包的操作。
//
// 任务:
//   1. 实现 sum() — 使用一元右折叠累加所有参数
//   2. 实现 product() — 使用一元右折叠求积
//   3. 实现 all_true() — 检查所有参数是否为 true
//   4. 实现 count_true() — 统计参数中 true 的个数
// 提示: 折叠表达式形式: (args op ...) 右折叠, (... op args) 左折叠

#include "cpplings.h"

// TODO: 实现一元右折叠的 sum 函数
// 提示: template<typename... Args> auto sum(Args... args) { return (args + ...); }
// 空参数包: 需要提供默认值，如 return (args + ... + 0);
// 注意: 一元右折叠 (args + ...) 要求参数包至少有一个元素
//       二元右折叠 (args + ... + init) 支持空参数包

TEST("fold — sum (二元右折叠)") {
    // TODO: 调用 sum() 并断言结果
    int _todo_ = "请删除此行，实现上面的 TODO";
    // ASSERT_EQ(sum(1, 2, 3), 6);
    // ASSERT_EQ(sum(10), 10);
    // ASSERT_EQ(sum(), 0);  // 空参数包
}

TEST("fold — product (二元右折叠)") {
    // TODO: 调用 product() 并断言结果
    int _todo_ = "请删除此行，实现上面的 TODO";
    // ASSERT_EQ(product(2, 3, 4), 24);
    // ASSERT_EQ(product(5), 5);
    // ASSERT_EQ(product(), 1);  // 空参数包
}

TEST("fold — all_true (一元左折叠)") {
    // TODO: 实现 all_true 并测试
    // 提示: template<typename... Args> bool all_true(Args... args) { return (... && args); }
    int _todo_ = "请删除此行，实现上面的 TODO";
    // ASSERT_TRUE(all_true(true, true, true));
    // ASSERT_FALSE(all_true(true, false, true));
    // ASSERT_TRUE(all_true(true));  // 单参数
}

TEST("fold — count_true (带 lambda 的折叠)") {
    // TODO: 实现 count_true，返回 true 值的个数
    // 提示: return (0 + ... + (args ? 1 : 0));
    int _todo_ = "请删除此行，实现上面的 TODO";
    // ASSERT_EQ(count_true(true, false, true, true), 3);
    // ASSERT_EQ(count_true(false, false), 0);
    // ASSERT_EQ(count_true(), 0);
}

CPPLINGS_MAIN
