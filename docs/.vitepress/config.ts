import { defineConfig } from 'vitepress'

export default defineConfig({
  title: 'Modern C++',
  description: 'C++ 知识库：从 C++98 到 C++29',
  lang: 'zh-CN',
  base: '/',
  themeConfig: {
    logo: '/logo.svg',
    siteTitle: 'Modern C++',

    nav: [
      { text: '首页', link: '/' },
      {
        text: '标准版本',
        items: [
          { text: 'C++98', link: '/standards/cpp98/' },
          { text: 'C++03', link: '/standards/cpp03/' },
          { text: 'C++11', link: '/standards/cpp11/' },
          { text: 'C++14', link: '/standards/cpp14/' },
          { text: 'C++17', link: '/standards/cpp17/' },
          { text: 'C++20', link: '/standards/cpp20/' },
          { text: 'C++23', link: '/standards/cpp23/' },
          { text: 'C++26', link: '/standards/cpp26/' },
          { text: 'C++29', link: '/standards/cpp29/' },
        ],
      },
      { text: '专题', link: '/topics/' },
      { text: '知名库', link: '/libraries/' },
      { text: '参考资料', link: '/references/' },
      { text: '博客', link: '/blog/' },
    ],
    sidebar: {
      '/standards/cpp98/': [
        {
          text: 'C++98 (ISO/IEC 14882:1998)',
          items: [
            { text: '概述', link: '/standards/cpp98/' },
            { text: '语言特性', link: '/standards/cpp98/features' },
            { text: '标准库', link: '/standards/cpp98/standard-library' },
          ],
        },
      ],
      '/standards/cpp03/': [
        {
          text: 'C++03 (ISO/IEC 14882:2003)',
          items: [
            { text: '概述', link: '/standards/cpp03/' },
            { text: '变更与修正', link: '/standards/cpp03/changes' },
          ],
        },
      ],
      '/standards/cpp11/': [
        {
          text: 'C++11 (ISO/IEC 14882:2011)',
          items: [
            { text: '概述', link: '/standards/cpp11/' },
            {
              text: '语言特性',
              collapsed: false,
              items: [
                { text: 'auto 类型推导', link: '/standards/cpp11/auto-type-deduction' },
                { text: '右值引用与移动语义', link: '/standards/cpp11/move-semantics' },
                { text: 'Lambda 表达式', link: '/standards/cpp11/lambda-expressions' },
                { text: '可变参数模板', link: '/standards/cpp11/variadic-templates' },
                { text: 'constexpr', link: '/standards/cpp11/constexpr' },
                { text: 'nullptr', link: '/standards/cpp11/nullptr' },
                { text: '枚举类 (enum class)', link: '/standards/cpp11/enum-class' },
                { text: '委托构造函数', link: '/standards/cpp11/delegating-constructors' },
                { text: '用户定义字面量', link: '/standards/cpp11/user-defined-literals' },
                { text: 'static_assert', link: '/standards/cpp11/static-assert' },
                { text: '属性 (Attributes)', link: '/standards/cpp11/attributes' },
              ],
            },
            {
              text: '标准库',
              collapsed: false,
              items: [
                { text: '智能指针', link: '/standards/cpp11/smart-pointers' },
                { text: 'std::thread', link: '/standards/cpp11/thread' },
                { text: 'std::array', link: '/standards/cpp11/array' },
                { text: '正则表达式', link: '/standards/cpp11/regex' },
                { text: '随机数库', link: '/standards/cpp11/random' },
                { text: 'chrono 时间库', link: '/standards/cpp11/chrono' },
                { text: '无序容器', link: '/standards/cpp11/unordered-containers' },
                { text: 'std::tuple', link: '/standards/cpp11/tuple' },
              ],
            },
          ],
        },
      ],
      '/standards/cpp14/': [
        {
          text: 'C++14 (ISO/IEC 14882:2014)',
          items: [
            { text: '概述', link: '/standards/cpp14/' },
            { text: '泛型 Lambda', link: '/standards/cpp14/generic-lambda' },
            { text: '返回类型推导', link: '/standards/cpp14/return-type-deduction' },
            { text: '变量模板', link: '/standards/cpp14/variable-templates' },
            { text: '二进制字面量', link: '/standards/cpp14/binary-literals' },
            { text: '数字分隔符', link: '/standards/cpp14/digit-separators' },
            { text: 'std::make_unique', link: '/standards/cpp14/make-unique' },
            { text: 'std::exchange', link: '/standards/cpp14/exchange' },
            { text: 'shared_timed_mutex', link: '/standards/cpp14/shared-timed-mutex' },
          ],
        },
      ],
      '/standards/cpp17/': [
        {
          text: 'C++17 (ISO/IEC 14882:2017)',
          items: [
            { text: '概述', link: '/standards/cpp17/' },
            {
              text: '语言特性',
              collapsed: false,
              items: [
                { text: '结构化绑定', link: '/standards/cpp17/structured-bindings' },
                { text: 'std::optional', link: '/standards/cpp17/optional' },
                { text: 'std::variant', link: '/standards/cpp17/variant' },
                { text: 'std::any', link: '/standards/cpp17/any' },
                { text: 'if constexpr', link: '/standards/cpp17/if-constexpr' },
                { text: '折叠表达式', link: '/standards/cpp17/fold-expressions' },
                { text: '内联变量', link: '/standards/cpp17/inline-variables' },
                { text: '类模板参数推导 (CTAD)', link: '/standards/cpp17/class-template-argument-deduction' },
                { text: '嵌套命名空间', link: '/standards/cpp17/nested-namespaces' },
                { text: '[[nodiscard]] / [[maybe_unused]] / [[fallthrough]]', link: '/standards/cpp17/attributes' },
              ],
            },
            {
              text: '标准库',
              collapsed: false,
              items: [
                { text: 'std::string_view', link: '/standards/cpp17/string-view' },
                { text: '文件系统 (filesystem)', link: '/standards/cpp17/filesystem' },
                { text: '并行算法', link: '/standards/cpp17/parallel-algorithms' },
              ],
            },
          ],
        },
      ],
      '/standards/cpp20/': [
        {
          text: 'C++20 (ISO/IEC 14882:2020)',
          items: [
            { text: '概述', link: '/standards/cpp20/' },
            {
              text: '语言特性',
              collapsed: false,
              items: [
                { text: 'Concepts (概念)', link: '/standards/cpp20/concepts' },
                { text: 'Ranges (范围库)', link: '/standards/cpp20/ranges' },
                { text: '协程 (Coroutines)', link: '/standards/cpp20/coroutines' },
                { text: 'Modules (模块)', link: '/standards/cpp20/modules' },
                { text: '三路比较运算符 (<=>)', link: '/standards/cpp20/spaceship-operator' },
                { text: 'consteval / constinit', link: '/standards/cpp20/consteval-constinit' },
                { text: 'requires 表达式', link: '/standards/cpp20/requires-expression' },
                { text: '聚合初始化增强', link: '/standards/cpp20/aggregate-init-enhancements' },
              ],
            },
            {
              text: '标准库',
              collapsed: false,
              items: [
                { text: 'std::format', link: '/standards/cpp20/format' },
                { text: 'std::span', link: '/standards/cpp20/span' },
                { text: 'std::jthread', link: '/standards/cpp20/jthread' },
                { text: '日历与时间区 (chrono)', link: '/standards/cpp20/chrono-calendar-tz' },
                { text: 'std::expected (路径)', link: '/standards/cpp20/expected' },
              ],
            },
          ],
        },
      ],
      '/standards/cpp23/': [
        {
          text: 'C++23 (ISO/IEC 14882:2024)',
          items: [
            { text: '概述', link: '/standards/cpp23/' },
            { text: 'std::expected', link: '/standards/cpp23/expected' },
            { text: 'std::print / std::println', link: '/standards/cpp23/print' },
            { text: 'std::mdspan', link: '/standards/cpp23/mdspan' },
            { text: 'std::generator', link: '/standards/cpp23/generator' },
            { text: 'Deducing this', link: '/standards/cpp23/deducing-this' },
            { text: 'if consteval', link: '/standards/cpp23/if-consteval' },
            { text: '多维下标运算符', link: '/standards/cpp23/multidimensional-subscript' },
            { text: 'std::flat_map / flat_set', link: '/standards/cpp23/flat-map-set' },
            { text: 'std::stacktrace', link: '/standards/cpp23/stacktrace' },
            { text: 'std::move_only_function', link: '/standards/cpp23/move-only-function' },
            { text: 'import std', link: '/standards/cpp23/import-std' },
          ],
        },
      ],
      '/standards/cpp26/': [
        {
          text: 'C++26 (进行中)',
          items: [
            { text: '概述', link: '/standards/cpp26/' },
            { text: 'Contracts (契约)', link: '/standards/cpp26/contracts' },
            { text: '反射 (Reflection)', link: '/standards/cpp26/reflection' },
            { text: '模式匹配 (Pattern Matching)', link: '/standards/cpp26/pattern-matching' },
            { text: 'std::simd', link: '/standards/cpp26/simd' },
            { text: 'Senders/Receivers', link: '/standards/cpp26/senders-receivers' },
            { text: 'constexpr 扩展', link: '/standards/cpp26/constexpr-extensions' },
          ],
        },
      ],
      '/standards/cpp29/': [
        {
          text: 'C++29 (展望)',
          items: [
            { text: '概述与展望', link: '/standards/cpp29/' },
          ],
        },
      ],
      '/topics/': [
        {
          text: '专题',
          items: [
            { text: '总览', link: '/topics/' },
            { text: '内存模型与并发', link: '/topics/memory-model' },
            { text: '模板元编程', link: '/topics/template-metaprogramming' },
            { text: 'RAII 与资源管理', link: '/topics/raii' },
            { text: '编译期计算', link: '/topics/compile-time-computation' },
            { text: 'C++ 设计模式', link: '/topics/design-patterns' },
            { text: '编译器优化', link: '/topics/compiler-optimizations' },
            { text: '性能优化', link: '/topics/performance' },
            { text: '工具链与生态', link: '/topics/toolchain' },
          ],
        },
      ],
      '/references/': [
        {
          text: '参考资料',
          items: [
            { text: '总览', link: '/references/' },
          ],
        },
      ],
      '/blog/': [
        {
          text: '博客',
          items: [
            { text: '文章列表', link: '/blog/' },
          ],
        },
      ],
      '/libraries/': [
        {
          text: 'STL 与标准库实现',
          items: [
            { text: '总览', link: '/libraries/' },
            { text: 'libc++ (LLVM)', link: '/libraries/llvm' },
            { text: 'libstdc++ (GCC)', link: '/libraries/libstdcxx' },
            { text: 'Abseil (Google)', link: '/libraries/abseil' },
            { text: 'Folly (Meta)', link: '/libraries/folly' },
            { text: 'Boost', link: '/libraries/boost' },
            { text: 'EASTL (EA)', link: '/libraries/eastl' },
            { text: 'fmt / spdlog', link: '/libraries/fmt-spdlog' },
            { text: 'range-v3', link: '/libraries/range-v3' },
          ],
        },
      ],
    },
    socialLinks: [
      { icon: 'github', link: 'https://github.com/' },
    ],
    footer: {
      message: '基于 MIT 许可发布',
      copyright: '© 2026 Modern C++ Knowledge Base',
    },
    search: {
      provider: 'local',
      options: {
        translations: {
          button: { buttonText: '搜索文档', buttonAriaLabel: '搜索' },
          modal: {
            noResultsText: '没有找到相关结果',
            resetButtonTitle: '清除查询条件',
            footer: { selectText: '选择', navigateText: '切换', closeText: '关闭' },
          },
        },
      },
    },
    outline: { label: '页面导航', level: [2, 3] },
    docFooter: { prev: '上一篇', next: '下一篇' },
    lastUpdated: { text: '最后更新于' },
    editLink: { pattern: 'https://github.com/yourname/modern-cpp/edit/main/docs/:path', text: '在 GitHub 上编辑此页面' },
  },
})