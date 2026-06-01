// Exercise: chrono 时间库
// C++11 的 <chrono> 提供了时间点、时长和时钟的类型安全抽象。
//
// 任务:
//   1. 创建不同单位的 duration 并进行算术运算
//   2. 使用 duration_cast 在不同单位间转换
//   3. 使用 steady_clock 测量代码执行时间
//
// 提示: #include <chrono>
//       std::chrono::seconds s(5);
//       std::chrono::milliseconds ms(1000);
//       auto result = s + ms;  // 类型自动推导
//       std::chrono::duration_cast<std::chrono::milliseconds>(s).count()

#include "cpplings.h"
#include <chrono>
#include <vector>
#include <numeric>

TEST("duration 基本算术") {
    using namespace std::chrono;

    // TODO: 创建 seconds(3) 和 milliseconds(500)
    //       计算 total_ms = sec + ms，结果应为 3500 毫秒
    // 提示: 两个不同单位的 duration 相加，结果为精度更高的类型

    int _todo_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    ASSERT_EQ(total_ms.count(), 3500);
}

TEST("duration_cast 单位转换") {
    using namespace std::chrono;

    milliseconds ms(2500);

    // TODO: 用 duration_cast 将 ms 转换为 seconds，存入变量 sec
    //       用 duration_cast 将 ms 转换为 microseconds，存入变量 us

    int _todo2_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    ASSERT_EQ(sec.count(), 2);            // 2500ms -> 2s (截断)
    ASSERT_EQ(us.count(), 2500000);       // 2500ms -> 2500000us
}

TEST("duration 乘除运算") {
    using namespace std::chrono;

    seconds s(10);

    // TODO: 计算 half = s / 2，结果应为 5 秒
    // TODO: 计算 triple = s * 3，结果应为 30 秒

    int _todo3_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    ASSERT_EQ(half.count(), 5);
    ASSERT_EQ(triple.count(), 30);
}

TEST("steady_clock 测量时间") {
    using namespace std::chrono;

    // TODO: 记录 start = steady_clock::now()
    //       执行一个简单计算（例如累加 1 到 100000）
    //       记录 end = steady_clock::now()
    //       计算 elapsed = duration_cast<microseconds>(end - start)

    int _todo4_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    // elapsed 应该是正数（代码执行需要时间）
    ASSERT_TRUE(elapsed.count() >= 0);
}

TEST("time_point 比较") {
    using namespace std::chrono;

    // TODO: 记录 t1 = steady_clock::now()
    //       记录 t2 = steady_clock::now()
    //       用 diff = t2 - t1 计算差值

    int _todo5_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

    ASSERT_TRUE(t2 >= t1);
    ASSERT_TRUE(diff.count() >= 0);
}

CPPLINGS_MAIN
