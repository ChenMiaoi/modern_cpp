// 模块中使用 concepts 和 templates
export module concepts_math;

import <concepts>;

export {
    template <typename T>
    concept Numeric = std::integral<T> || std::floating_point<T>;

    template <Numeric T>
    T safe_add(T a, T b) {
        return a + b;
    }

    template <Numeric T>
    T safe_multiply(T a, T b) {
        return a * b;
    }
}
