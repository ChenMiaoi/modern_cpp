// 自动向量化演示
// 编译器将标量操作转为 SIMD 指令
//
// 编译查看:
//   g++ -O2 -mavx2 -S vectorize.cpp -o vectorize.s
//   # 搜索 vmovdqu, vpaddd 等 AVX2 指令
//   g++ -O2 -mavx2 -fopt-info-vec-optimized vectorize.cpp
//   # 输出向量化报告

#include <cstddef>

// 简单数组加法 — 编译器容易向量化
void add_arrays(int* __restrict__ a, const int* __restrict__ b, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        a[i] += b[i];  // 无依赖，可向量化
    }
}

// 有依赖的循环 — 不可向量化
void prefix_sum(int* a, size_t n) {
    for (size_t i = 1; i < n; ++i) {
        a[i] += a[i - 1];  // 循环依赖，阻止向量化
    }
}

// 条件向量化: 编译器可用 mask 指令
void abs_array(int* a, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if (a[i] < 0) a[i] = -a[i];  // 可条件向量化
    }
}

int main() {
    int a[8] = {1,2,3,4,5,6,7,8};
    int b[8] = {8,7,6,5,4,3,2,1};
    add_arrays(a, b, 8);
    return a[0];
}
