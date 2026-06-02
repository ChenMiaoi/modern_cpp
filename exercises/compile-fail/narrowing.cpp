// compile-fail: narrowing
// 预期编译失败: narrowing conversion
// 测试: 列表初始化中的窄化转换应无法编译

int main() {
    int x = 42;
    char c{x};  // 应该编译失败: narrowing conversion from int to char
    (void)c;
    return 0;
}
