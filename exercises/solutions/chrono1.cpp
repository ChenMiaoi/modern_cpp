// Solution: chrono 时间库
// C++11 的 <chrono> 提供了时间点、时长和时钟的类型安全抽象。

#include "cpplings.h"
#include <chrono>
#include <vector>
#include <numeric>

TEST("duration 基本算术") {
    using namespace std::chrono;

    seconds sec(3);
    milliseconds ms(500);
    auto total_ms = sec + ms;

    ASSERT_EQ(total_ms.count(), 3500);
}

TEST("duration_cast 单位转换") {
    using namespace std::chrono;

    milliseconds ms(2500);

    auto sec = duration_cast<seconds>(ms);
    auto us = duration_cast<microseconds>(ms);

    ASSERT_EQ(sec.count(), 2);
    ASSERT_EQ(us.count(), 2500000);
}

TEST("duration 乘除运算") {
    using namespace std::chrono;

    seconds s(10);

    auto half = s / 2;
    auto triple = s * 3;

    ASSERT_EQ(half.count(), 5);
    ASSERT_EQ(triple.count(), 30);
}

TEST("steady_clock 测量时间") {
    using namespace std::chrono;

    auto start = steady_clock::now();
    int sum = 0;
    for (int i = 1; i <= 100000; ++i) {
        sum += i;
    }
    auto end = steady_clock::now();
    auto elapsed = duration_cast<microseconds>(end - start);

    ASSERT_TRUE(elapsed.count() >= 0);
}

TEST("time_point 比较") {
    using namespace std::chrono;

    auto t1 = steady_clock::now();
    auto t2 = steady_clock::now();
    auto diff = t2 - t1;

    ASSERT_TRUE(t2 >= t1);
    ASSERT_TRUE(diff.count() >= 0);
}

CPPLINGS_MAIN
