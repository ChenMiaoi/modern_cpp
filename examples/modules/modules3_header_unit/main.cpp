// Header Unit 示例
// 编译方式 (GCC):
//   g++ -std=c++20 -fmodules-ts -xc++-system-header iostream
//   g++ -std=c++20 -fmodules-ts main.cpp
//
// Header unit 是传统头文件的模块化包装
// 与 named module 的区别:
//   - header unit 导出宏 (有限制)
//   - named module 不导出宏
//   - header unit 兼容传统头文件
//   - named module 需要重写接口

import <iostream>;
import <string>;
import <vector>;

int main() {
    std::vector<std::string> names = {"Alice", "Bob", "Charlie"};
    for (const auto& name : names) {
        std::cout << "Hello, " << name << "!\n";
    }
    return 0;
}
