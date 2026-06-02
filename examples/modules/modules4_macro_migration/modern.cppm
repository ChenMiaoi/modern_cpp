// 模块化版本: 不导出宏，用 constexpr/inline 替代
export module modern;

export {
    constexpr const char* version = "2.0";

    template <typename T>
    constexpr T max_val(T a, T b) {
        return a > b ? a : b;
    }

    inline int modern_add(int a, int b) {
        return a + b;
    }
}
