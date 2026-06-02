// 模块基础使用示例
// 编译方式:
//   g++ -std=c++20 -fmodules-ts -c math.cppm -o math.o
//   g++ -std=c++20 -fmodules-ts main.cpp math.o -o main
//
// MSVC:
//   cl /std:c++20 /EHsc /c math.cppm
//   cl /std:c++20 /EHsc main.cpp math.obj

import math;
#include <iostream>

int main() {
    std::cout << "add(3, 4) = " << math::add(3, 4) << "\n";
    std::cout << "multiply(3, 4) = " << math::multiply(3, 4) << "\n";
    return 0;
}
