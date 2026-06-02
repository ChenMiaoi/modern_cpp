// LICM (Loop-Invariant Code Motion) 演示
// 编译器将循环不变量移出循环
//
// 编译查看:
//   g++ -O2 -S licm.cpp -o licm.s
//   # 搜索循环体，确认不变量计算在循环外

#include <cmath>

// 循环不变量: len 在循环中不变，应被移出
float normalize(float* vec, int n) {
    float len = 0.0f;
    for (int i = 0; i < n; ++i) {
        len += vec[i] * vec[i];  // 累加
    }
    len = std::sqrt(len);  // 循环后计算

    float sum = 0.0f;
    for (int i = 0; i < n; ++i) {
        float normalized = vec[i] / len;  // len 是循环不变量
        sum += normalized * normalized;
    }
    return sum;
}

// 手动 LICM: 将不变量提前
float normalize_manual_licm(float* vec, int n) {
    float len = 0.0f;
    for (int i = 0; i < n; ++i) {
        len += vec[i] * vec[i];
    }
    len = std::sqrt(len);
    float inv_len = 1.0f / len;  // 移出循环

    float sum = 0.0f;
    for (int i = 0; i < n; ++i) {
        float normalized = vec[i] * inv_len;  // 乘法替代除法
        sum += normalized * normalized;
    }
    return sum;
}

int main() {
    float v[] = {1.0f, 2.0f, 3.0f, 4.0f};
    return (int)normalize(v, 4);
}
