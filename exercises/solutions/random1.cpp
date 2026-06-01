// Solution: 随机数库 (std::random)
// C++11 引入了基于引擎和分布的随机数库，
// 比 rand()/srand() 更灵活、更可控。

#include "cpplings.h"
#include <random>
#include <vector>
#include <set>

TEST("uniform_int_distribution 范围正确") {
    std::mt19937 gen(42);
    std::uniform_int_distribution<int> dist(1, 100);

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
    std::mt19937 gen1(99);
    std::mt19937 gen2(99);
    std::uniform_int_distribution<int> dist(0, 999);

    int a = dist(gen1);
    int b = dist(gen2);

    ASSERT_EQ(a, b);
}

TEST("uniform_real_distribution 浮点数") {
    std::mt19937 gen(7);
    std::uniform_real_distribution<double> dist(0.0, 1.0);

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
    std::mt19937 gen(123);
    std::uniform_int_distribution<int> dist(1, 10000);

    std::set<int> seen;
    for (int i = 0; i < 100; ++i) {
        seen.insert(dist(gen));
    }
    int unique_count = static_cast<int>(seen.size());

    ASSERT_TRUE(unique_count > 50);
}

CPPLINGS_MAIN
