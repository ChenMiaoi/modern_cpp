// compile-fail: deleted_copy
// 预期编译失败: use of deleted function
// 测试: 尝试拷贝不可拷贝的类型应无法编译

#include <memory>

struct MoveOnly {
    std::unique_ptr<int> ptr;
    MoveOnly() : ptr(new int(42)) {}
    // unique_ptr 使拷贝构造/赋值被 delete
};

int main() {
    MoveOnly a;
    MoveOnly b = a;  // 应该编译失败: use of deleted function
    return 0;
}
