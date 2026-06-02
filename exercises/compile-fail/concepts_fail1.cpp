// compile-fail: concepts_fail1
// 预期编译失败: constraints not satisfied
// 测试: concept 约束不满足时应无法编译

#include <concepts>
#include <string>

template <typename T>
concept Sortable = requires(T a, T b) {
    { a < b } -> std::convertible_to<bool>;
};

void sort_it(Sortable auto& container) {
    (void)container;
}

struct NotSortable {
    int x;
    // 没有 operator<
};

int main() {
    NotSortable ns{42};
    sort_it(ns); // 应该编译失败: constraints not satisfied
    return 0;
}
