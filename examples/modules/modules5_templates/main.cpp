import concepts_math;
import <iostream>;

int main() {
    std::cout << "safe_add(3, 4) = " << safe_add(3, 4) << "\n";
    std::cout << "safe_add(1.5, 2.5) = " << safe_add(1.5, 2.5) << "\n";
    // safe_add("hello", "world");  // 编译失败: string 不满足 Numeric
    return 0;
}
