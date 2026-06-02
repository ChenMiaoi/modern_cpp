// 模块接口文件 (.cppm 是 GCC/Clang 约定)
// MSVC 使用 .ixx
export module math;

export namespace math {

int add(int a, int b) {
    return a + b;
}

int multiply(int a, int b) {
    return a * b;
}

} // namespace math
