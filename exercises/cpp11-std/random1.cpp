// Exercise: 随机数库 (std::random)
// C++11 引入了基于引擎和分布的随机数库，
// 比 rand()/srand() 更灵活、更可控。
//
// 任务:
//   1. 用 mt19937 引擎和 uniform_int_distribution 生成 [1, 100] 的随机整数
//   2. 用固定种子保证可重复性
//   3. 用 uniform_real_distribution 生成 [0.0, 1.0) 的随机浮点数
//   4. 使用 min() 和 max() 验证分布范围
//
// 提示: #include <random>
//       std::mt19937 gen(seed);  // 以 seed 为种子初始化引擎
//       std::uniform_int_distribution<int> dist(a, b);
//       int val = dist(gen);  // 生成一个随机数

#include "cpplings.h"
#include <random>
#include <vector>

TEST("uniform_int_distribution 范围正确") {
    // TODO: 用种子 42 创建 mt19937 引擎 gen
    // TODO: 创建 uniform_int_distribution<int> dist(1, 100)
    // TODO: 生成 1000 个随机数，全部在 [1, 100] 范围内

    int _todo_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    bool all_in_range = true;
    for (int i = 0; i < 1000; ++i) {
        int val = dist(gen);
        if (val < 1 || val > 100) {
            all_in_range = false;
            break;
        }
    }
    ASSERT_TRUE(all_in_range);
    ASSERT_EQ(dist.min(), 1);
    ASSERT_EQ(dist.max(), 100);
}

TEST("固定种子可重复") {
    // TODO: 用种子 99 创建两个 mt19937 引擎 gen1, gen2
    // TODO: 创建 uniform_int_distribution<int> dist(0, 999)
    // TODO: 用 gen1 和 gen2 各生成一个数 a 和 b，它们应该相等

    int _todo2_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    ASSERT_EQ(a, b);
}

TEST("uniform_real_distribution 浮点数") {
    // TODO: 用种子 7 创建 mt19937 引擎 gen
    // TODO: 创建 uniform_real_distribution<double> dist(0.0, 1.0)
    // TODO: 生成 100 个浮点数，验证全部在 [0.0, 1.0) 范围内

    int _todo3_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    bool all_in_range = true;
    for (int i = 0; i < 100; ++i) {
        double val = dist(gen);
        if (val < 0.0 || val >= 1.0) {
            all_in_range = false;
            break;
        }
    }
    ASSERT_TRUE(all_in_range);
}

TEST("生成多个不同值") {
    // TODO: 用种子 123 创建 mt19937 引擎 gen
    // TODO: 创建 uniform_int_distribution<int> dist(1, 10000)
    // TODO: 生成 100 个数，检查至少有 50 个不同值（不全是同一个数）

    int _todo4_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    ASSERT_TRUE(unique_count > 50);
}

CPPLINGS_MAIN
