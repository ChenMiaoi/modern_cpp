// SROA (Scalar Replacement of Aggregates) 演示
// 编译器将小结构体拆解为独立标量变量
//
// 编译查看优化效果:
//   g++ -O0 -S sroa.cpp -o sroa_O0.s
//   g++ -O2 -S sroa.cpp -o sroa_O2.s
//   diff sroa_O0.s sroa_O2.s

struct Point {
    int x, y;
};

// SROA 可以将 Point 拆为两个独立 int 变量
int sum_points(Point* arr, int n) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        Point p = arr[i];  // SROA: p.x, p.y 成为独立变量
        total += p.x + p.y;
    }
    return total;
}

// 不可 SROA 的情况: 取地址
int sum_points_addr(Point* arr, int n) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        Point p = arr[i];
        int* px = &p.x;  // 取地址阻止 SROA
        total += *px + p.y;
    }
    return total;
}

int main() {
    Point arr[] = {{1,2}, {3,4}, {5,6}};
    return sum_points(arr, 3);
}
