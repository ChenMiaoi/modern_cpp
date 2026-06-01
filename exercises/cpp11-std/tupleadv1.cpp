// Exercise: tuple 高级操作
// C++11 的 std::tuple 提供了丰富的编译期操作，
// 包括拼接、转发、遍历和类型查询。
//
// 任务:
//   1. 实现 tuple_cat_result — 用 tuple_cat 拼接两个 tuple
//   2. 实现 make_ref_tuple — 用 forward_as_tuple 创建引用 tuple
//   3. 实现 for_each_in_tuple — 遍历 tuple 中的每个元素并调用函数
//      （使用 C++11 手动 index_sequence 实现）
//   4. 实现 get_type_name — 利用 tuple_element 获取第 N 个元素的类型描述
//
// 提示: std::tuple_cat(t1, t2) 拼接两个 tuple
//       std::forward_as_tuple(std::forward<Args>(args)...) 保持值类别
//       C++11 没有 std::index_sequence，需要自己实现
//       std::tuple_element<N, Tuple>::type 获取第 N 个元素的类型

#include "cpplings.h"
#include <tuple>
#include <string>
#include <type_traits>
#include <sstream>

// C++11 手动实现 index_sequence（C++14 才有 std::index_sequence）
template <size_t... Is>
struct index_sequence {};

template <size_t N, size_t... Is>
struct make_index_sequence : make_index_sequence<N - 1, N - 1, Is...> {};

template <size_t... Is>
struct make_index_sequence<0, Is...> {
    using type = index_sequence<Is...>;
};

// TODO 1: 实现 tuple_cat_result
//   拼接 tuple<int, double> 和 tuple<char, std::string>，
//   返回类型应为 tuple<int, double, char, std::string>
//   auto tuple_cat_result() {
//       auto t1 = std::make_tuple(1, 3.14);
//       auto t2 = std::make_tuple('a', std::string("hello"));
//       return std::tuple_cat(t1, t2);
//   }

int _todo_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

// TODO 2: 实现 make_ref_tuple
//   template <typename... Args>
//   auto make_ref_tuple(Args&&... args)
//       -> decltype(std::forward_as_tuple(std::forward<Args>(args)...))
//   {
//       return std::forward_as_tuple(std::forward<Args>(args)...);
//   }
//   forward_as_tuple 创建一个保持值类别的 tuple：
//   - 传入左值 → tuple 中存的是左值引用
//   - 传入右值 → tuple 中存的是右值引用

int _todo2_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

// TODO 3: 实现 for_each_in_tuple
//   // 辅助函数：对 tuple 中的每个元素调用 func
//   template <typename Tuple, typename F, size_t... Is>
//   void for_each_impl(Tuple&& t, F&& func, index_sequence<Is...>) {
//       // 用逗号展开（C++11 fold 替代方案）
//       using swallow = int[];
//       (void)swallow{0, (func(std::get<Is>(std::forward<Tuple>(t))), 0)...};
//   }
//
//   template <typename Tuple, typename F>
//   void for_each_in_tuple(Tuple&& t, F&& func) {
//       constexpr size_t size = std::tuple_size<
//           typename std::decay<Tuple>::type
//       >::value;
//       for_each_impl(std::forward<Tuple>(t),
//                     std::forward<F>(func),
//                     typename make_index_sequence<size>::type{});
//   }

int _todo3_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

// TODO 4: 实现 get_type_name
//   获取 tuple 中第 N 个元素的类型名称字符串。
//   std::string get_type_name_at_0() {
//       // 对于 tuple<int, double, std::string>，第 0 个元素类型是 int
//       using T = std::tuple<int, double, std::string>;
//       using Elem = std::tuple_element<0, T>::type;
//       // 用 type traits 判断类型名
//       if (std::is_same<Elem, int>::value) return "int";
//       if (std::is_same<Elem, double>::value) return "double";
//       if (std::is_same<Elem, std::string>::value) return "string";
//       return "unknown";
//   }

int _todo4_ = "请删除此行，实现上面的 TODO";  // 编译错误：类型不匹配

// --- Tests ---

TEST("tuple_cat 拼接") {
    auto result = tuple_cat_result();

    ASSERT_EQ(std::get<0>(result), 1);
    ASSERT_EQ(std::get<1>(result), 3.14);
    ASSERT_EQ(std::get<2>(result), 'a');
    ASSERT_EQ(std::get<3>(result), std::string("hello"));

    // 检查结果类型大小
    ASSERT_EQ(std::tuple_size<decltype(result)>::value, 4u);
}

TEST("forward_as_tuple 保持左值引用") {
    int x = 42;
    std::string s = "hello";

    auto ref_t = make_ref_tuple(x, s);

    // 修改原始变量应该反映到 tuple 中
    x = 100;
    s = "world";

    ASSERT_EQ(std::get<0>(ref_t), 100);
    ASSERT_EQ(std::get<1>(ref_t), std::string("world"));

    // 验证类型：应该持有引用
    bool holds_ref = std::is_lvalue_reference<
        std::tuple_element<0, decltype(ref_t)>::type
    >::value;
    ASSERT_TRUE(holds_ref);
}

TEST("forward_as_tuple 保持右值引用") {
    auto ref_t = make_ref_tuple(42, std::string("temp"));

    // 右值引用的 tuple 元素类型应该是右值引用
    bool elem0_is_rref = std::is_rvalue_reference<
        std::tuple_element<0, decltype(ref_t)>::type
    >::value;
    ASSERT_TRUE(elem0_is_rref);
}

TEST("for_each_in_tuple 遍历求和") {
    auto t = std::make_tuple(1, 2, 3, 4, 5);
    int sum = 0;

    for_each_in_tuple(t, [&sum](int val) {
        sum += val;
    });

    ASSERT_EQ(sum, 15);
}

TEST("for_each_in_tuple 字符串拼接") {
    auto t = std::make_tuple(
        std::string("a"),
        std::string("b"),
        std::string("c")
    );
    std::string result;

    for_each_in_tuple(t, [&result](const std::string& s) {
        result += s;
    });

    ASSERT_EQ(result, std::string("abc"));
}

TEST("tuple_element 获取类型 — 静态断言") {
    using T = std::tuple<int, double, std::string>;

    // 第 0 个元素类型是 int
    static_assert(std::is_same<std::tuple_element<0, T>::type, int>::value,
                  "element 0 should be int");
    // 第 1 个元素类型是 double
    static_assert(std::is_same<std::tuple_element<1, T>::type, double>::value,
                  "element 1 should be double");
    // 第 2 个元素类型是 std::string
    static_assert(std::is_same<std::tuple_element<2, T>::type, std::string>::value,
                  "element 2 should be std::string");

    // 运行时验证
    std::string name = get_type_name_at_0();
    ASSERT_EQ(name, std::string("int"));
}

TEST("tie 与 ignore 部分解包") {
    auto t = std::make_tuple(1, std::string("hello"), 3.14, true);

    int first;
    double third;
    // 只解包第 0 和第 2 个元素
    std::tie(first, std::ignore, third, std::ignore) = t;

    ASSERT_EQ(first, 1);
    ASSERT_EQ(third, 3.14);
}

TEST("make_tuple decay 行为") {
    // make_tuple 对数组退化为指针
    int arr[] = {1, 2, 3};
    auto t1 = std::make_tuple(arr);
    bool is_pointer = std::is_pointer<
        std::tuple_element<0, decltype(t1)>::type
    >::value;
    ASSERT_TRUE(is_pointer);

    // make_tuple 对引用退化为值
    int x = 42;
    auto t2 = std::make_tuple(std::ref(x));
    // std::ref 创建 reference_wrapper，不是引用
    std::get<0>(t2) = 99;
    ASSERT_EQ(x, 99);  // 修改应该反映到原始变量
}

CPPLINGS_MAIN
