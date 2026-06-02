import { defineConfig } from 'vitepress'

// ── Shared config ──────────────────────────────────────────────────
const base = '/modern_cpp/'
const githubEdit = 'https://github.com/ChenMiaoi/modern_cpp/edit/main/docs/:path'
const socialLinks = [{ icon: 'github' as const, link: 'https://github.com/' }]

// ── Sidebar (shared structure, labels differ per locale) ───────────
// prefix: '' for zh-CN root, '/en' for English locale
function sidebar(zh: boolean, prefix = '') {
  const L = (link: string) => prefix + link
  return {
    '/standards/cpp98/': [{ text: zh ? 'C++98 (ISO/IEC 14882:1998)' : 'C++98 (ISO/IEC 14882:1998)', items: [
      { text: zh ? '概述' : 'Overview', link: L('/standards/cpp98/') },
      { text: zh ? '语言特性' : 'Language Features', link: L('/standards/cpp98/features') },
      { text: zh ? '标准库' : 'Standard Library', link: L('/standards/cpp98/standard-library') },
    ]}],
    '/standards/cpp03/': [{ text: zh ? 'C++03 (ISO/IEC 14882:2003)' : 'C++03 (ISO/IEC 14882:2003)', items: [
      { text: zh ? '概述' : 'Overview', link: L('/standards/cpp03/') },
      { text: zh ? '变更与修正' : 'Changes & Fixes', link: L('/standards/cpp03/changes') },
    ]}],
    '/standards/cpp11/': [{ text: zh ? 'C++11 (ISO/IEC 14882:2011)' : 'C++11 (ISO/IEC 14882:2011)', items: [
      { text: zh ? '概述' : 'Overview', link: L('/standards/cpp11/') },
      { text: zh ? '语言特性' : 'Language Features', collapsed: false, items: [
        { text: zh ? 'auto 类型推导' : 'auto Type Deduction', link: L('/standards/cpp11/auto-type-deduction') },
        { text: zh ? '右值引用与移动语义' : 'Rvalue References & Move Semantics', link: L('/standards/cpp11/move-semantics') },
        { text: zh ? 'Lambda 表达式' : 'Lambda Expressions', link: L('/standards/cpp11/lambda-expressions') },
        { text: zh ? '可变参数模板' : 'Variadic Templates', link: L('/standards/cpp11/variadic-templates') },
        { text: 'constexpr', link: L('/standards/cpp11/constexpr') },
        { text: 'nullptr', link: L('/standards/cpp11/nullptr') },
        { text: zh ? '枚举类 (enum class)' : 'enum class', link: L('/standards/cpp11/enum-class') },
        { text: zh ? '委托构造函数' : 'Delegating Constructors', link: L('/standards/cpp11/delegating-constructors') },
        { text: zh ? '用户定义字面量' : 'User-Defined Literals', link: L('/standards/cpp11/user-defined-literals') },
        { text: 'static_assert', link: L('/standards/cpp11/static-assert') },
        { text: zh ? '属性 (Attributes)' : 'Attributes', link: L('/standards/cpp11/attributes') },
      ]},
      { text: zh ? '标准库' : 'Standard Library', collapsed: false, items: [
        { text: zh ? '智能指针' : 'Smart Pointers', link: L('/standards/cpp11/smart-pointers') },
        { text: 'std::thread', link: L('/standards/cpp11/thread') },
        { text: 'std::array', link: L('/standards/cpp11/array') },
        { text: zh ? '正则表达式' : 'Regular Expressions', link: L('/standards/cpp11/regex') },
        { text: zh ? '随机数库' : 'Random Number Library', link: L('/standards/cpp11/random') },
        { text: zh ? 'chrono 时间库' : 'chrono Library', link: L('/standards/cpp11/chrono') },
        { text: zh ? '无序容器' : 'Unordered Containers', link: L('/standards/cpp11/unordered-containers') },
        { text: 'std::tuple', link: L('/standards/cpp11/tuple') },
      ]},
    ]}],
    '/standards/cpp14/': [{ text: zh ? 'C++14 (ISO/IEC 14882:2014)' : 'C++14 (ISO/IEC 14882:2014)', items: [
      { text: zh ? '概述' : 'Overview', link: L('/standards/cpp14/') },
      { text: zh ? '泛型 Lambda' : 'Generic Lambdas', link: L('/standards/cpp14/generic-lambda') },
      { text: zh ? '返回类型推导' : 'Return Type Deduction', link: L('/standards/cpp14/return-type-deduction') },
      { text: zh ? '变量模板' : 'Variable Templates', link: L('/standards/cpp14/variable-templates') },
      { text: zh ? '二进制字面量' : 'Binary Literals', link: L('/standards/cpp14/binary-literals') },
      { text: zh ? '数字分隔符' : 'Digit Separators', link: L('/standards/cpp14/digit-separators') },
      { text: 'std::make_unique', link: L('/standards/cpp14/make-unique') },
      { text: 'std::exchange', link: L('/standards/cpp14/exchange') },
      { text: 'shared_timed_mutex', link: L('/standards/cpp14/shared-timed-mutex') },
    ]}],
    '/standards/cpp17/': [{ text: zh ? 'C++17 (ISO/IEC 14882:2017)' : 'C++17 (ISO/IEC 14882:2017)', items: [
      { text: zh ? '概述' : 'Overview', link: L('/standards/cpp17/') },
      { text: zh ? '语言特性' : 'Language Features', collapsed: false, items: [
        { text: zh ? '结构化绑定' : 'Structured Bindings', link: L('/standards/cpp17/structured-bindings') },
        { text: 'std::optional', link: L('/standards/cpp17/optional') },
        { text: 'std::variant', link: L('/standards/cpp17/variant') },
        { text: 'std::any', link: L('/standards/cpp17/any') },
        { text: 'if constexpr', link: L('/standards/cpp17/if-constexpr') },
        { text: zh ? '折叠表达式' : 'Fold Expressions', link: L('/standards/cpp17/fold-expressions') },
        { text: zh ? '内联变量' : 'Inline Variables', link: L('/standards/cpp17/inline-variables') },
        { text: zh ? '类模板参数推导 (CTAD)' : 'Class Template Argument Deduction (CTAD)', link: L('/standards/cpp17/class-template-argument-deduction') },
        { text: zh ? '嵌套命名空间' : 'Nested Namespaces', link: L('/standards/cpp17/nested-namespaces') },
        { text: '[[nodiscard]] / [[maybe_unused]] / [[fallthrough]]', link: L('/standards/cpp17/attributes') },
      ]},
      { text: zh ? '标准库' : 'Standard Library', collapsed: false, items: [
        { text: 'std::string_view', link: L('/standards/cpp17/string-view') },
        { text: zh ? '文件系统 (filesystem)' : 'Filesystem', link: L('/standards/cpp17/filesystem') },
        { text: zh ? '并行算法' : 'Parallel Algorithms', link: L('/standards/cpp17/parallel-algorithms') },
      ]},
    ]}],
    '/standards/cpp20/': [{ text: zh ? 'C++20 (ISO/IEC 14882:2020)' : 'C++20 (ISO/IEC 14882:2020)', items: [
      { text: zh ? '概述' : 'Overview', link: L('/standards/cpp20/') },
      { text: zh ? '语言特性' : 'Language Features', collapsed: false, items: [
        { text: zh ? 'Concepts (概念)' : 'Concepts', link: L('/standards/cpp20/concepts') },
        { text: zh ? 'Ranges (范围库)' : 'Ranges', link: L('/standards/cpp20/ranges') },
        { text: zh ? '协程 (Coroutines)' : 'Coroutines', link: L('/standards/cpp20/coroutines') },
        { text: zh ? 'Modules (模块)' : 'Modules', link: L('/standards/cpp20/modules') },
        { text: zh ? '三路比较运算符 (<=>)' : 'Three-Way Comparison (<=>)', link: L('/standards/cpp20/spaceship-operator') },
        { text: 'consteval / constinit', link: L('/standards/cpp20/consteval-constinit') },
        { text: zh ? 'requires 表达式' : 'requires Expressions', link: L('/standards/cpp20/requires-expression') },
        { text: zh ? '聚合初始化增强' : 'Aggregate Init Enhancements', link: L('/standards/cpp20/aggregate-init-enhancements') },
      ]},
      { text: zh ? '标准库' : 'Standard Library', collapsed: false, items: [
        { text: 'std::format', link: L('/standards/cpp20/format') },
        { text: 'std::span', link: L('/standards/cpp20/span') },
        { text: 'std::jthread', link: L('/standards/cpp20/jthread') },
        { text: zh ? '日历与时间区 (chrono)' : 'Calendar & Timezone (chrono)', link: L('/standards/cpp20/chrono-calendar-tz') },
        { text: zh ? 'std::expected (路径)' : 'std::expected (Path)', link: L('/standards/cpp20/expected') },
      ]},
    ]}],
    '/standards/cpp23/': [{ text: zh ? 'C++23 (ISO/IEC 14882:2024)' : 'C++23 (ISO/IEC 14882:2024)', items: [
      { text: zh ? '概述' : 'Overview', link: L('/standards/cpp23/') },
      { text: 'std::expected', link: L('/standards/cpp23/expected') },
      { text: 'std::print / std::println', link: L('/standards/cpp23/print') },
      { text: 'std::mdspan', link: L('/standards/cpp23/mdspan') },
      { text: 'std::generator', link: L('/standards/cpp23/generator') },
      { text: 'Deducing this', link: L('/standards/cpp23/deducing-this') },
      { text: 'if consteval', link: L('/standards/cpp23/if-consteval') },
      { text: zh ? '多维下标运算符' : 'Multidimensional Subscript', link: L('/standards/cpp23/multidimensional-subscript') },
      { text: 'std::flat_map / flat_set', link: L('/standards/cpp23/flat-map-set') },
      { text: 'std::stacktrace', link: L('/standards/cpp23/stacktrace') },
      { text: 'std::move_only_function', link: L('/standards/cpp23/move-only-function') },
      { text: 'import std', link: L('/standards/cpp23/import-std') },
    ]}],
    '/standards/cpp26/': [{ text: zh ? 'C++26 (进行中)' : 'C++26 (In Progress)', items: [
      { text: zh ? '概述' : 'Overview', link: L('/standards/cpp26/') },
      { text: zh ? 'Contracts (契约)' : 'Contracts', link: L('/standards/cpp26/contracts') },
      { text: zh ? '反射 (Reflection)' : 'Reflection', link: L('/standards/cpp26/reflection') },
      { text: zh ? '模式匹配 (Pattern Matching)' : 'Pattern Matching', link: L('/standards/cpp26/pattern-matching') },
      { text: 'std::simd', link: L('/standards/cpp26/simd') },
      { text: 'Senders/Receivers', link: L('/standards/cpp26/senders-receivers') },
      { text: zh ? 'constexpr 扩展' : 'constexpr Extensions', link: L('/standards/cpp26/constexpr-extensions') },
    ]}],
    '/standards/cpp29/': [{ text: zh ? 'C++29 (展望)' : 'C++29 (Outlook)', items: [
      { text: zh ? '概述与展望' : 'Overview & Outlook', link: L('/standards/cpp29/') },
    ]}],
    '/topics/': [{ text: zh ? '专题' : 'Topics', items: [
      { text: zh ? '总览' : 'Overview', link: L('/topics/') },
      { text: zh ? 'C++ 术语黑话全书' : 'C++ Jargon Buster', link: L('/topics/cpp-jargon/') },
      { text: zh ? '内存模型与并发' : 'Memory Model & Concurrency', link: L('/topics/memory-model') },
      { text: zh ? '模板元编程' : 'Template Metaprogramming', link: L('/topics/template-metaprogramming') },
      { text: zh ? 'RAII 与资源管理' : 'RAII & Resource Management', link: L('/topics/raii') },
      { text: zh ? '编译期计算' : 'Compile-Time Computation', link: L('/topics/compile-time-computation') },
      { text: zh ? 'C++ 设计模式' : 'Design Patterns', link: L('/topics/design-patterns') },
      { text: zh ? '编译器优化' : 'Compiler Optimizations', link: L('/topics/compiler-optimizations') },
      { text: zh ? '性能优化' : 'Performance', link: L('/topics/performance') },
      { text: zh ? '工具链与生态' : 'Toolchain & Ecosystem', link: L('/topics/toolchain') },
    ]}],
    '/references/': [{ text: zh ? '参考资料' : 'References', items: [
      { text: zh ? '总览' : 'Overview', link: L('/references/') },
    ]}],
    '/blog/': [{ text: zh ? '博客' : 'Blog', items: [
      { text: zh ? '文章列表' : 'Posts', link: L('/blog/') },
    ]}],
    '/libraries/': [{ text: zh ? 'STL 与标准库实现' : 'STL & Standard Library Implementations', items: [
      { text: zh ? '总览' : 'Overview', link: L('/libraries/') },
      { text: 'libc++ (LLVM)', link: L('/libraries/llvm/') },
      { text: 'libstdc++ (GCC)', link: L('/libraries/libstdcxx/') },
      { text: 'Abseil (Google)', link: L('/libraries/abseil/') },
      { text: 'Folly (Meta)', link: L('/libraries/folly/') },
      { text: 'Boost (Top 50)', link: L('/libraries/boost/') },
      { text: 'EASTL (EA)', link: L('/libraries/eastl/') },
      { text: 'fmt / spdlog', link: L('/libraries/fmt-spdlog/') },
      { text: 'range-v3', link: L('/libraries/range-v3/') },
    ]}],
  }
}

// ── Export ─────────────────────────────────────────────────────────
export default defineConfig({
  title: 'Modern C++',
  base,
  ignoreDeadLinks: [
    /^(\.\/)?\.\.\/\.\.\/\.\.\/exercises\//,
  ],
  markdown: {
    languages: ['ini'],
    languageAlias: { meson: 'ini' },
    config: (md) => {
      const defaultRender = md.renderer.rules.fence!.bind(md.renderer.rules)
      md.renderer.rules.fence = (tokens, idx, options, env, self) => {
        const token = tokens[idx]
        const rendered = defaultRender(tokens, idx, options, env, self)
        if (!/^(cpp|c\+\+|cxx)\b/.test(token.info.trim())) return rendered
        const b64 = Buffer.from(token.content, 'utf-8').toString('base64')
        return `<div class="ce-wrapper" data-ce-code="${b64}">${rendered}</div>\n`
      }
    },
  },
  vite: {
    build: { chunkSizeWarningLimit: 1200 },
  },

  locales: {
    root: {
      label: '简体中文',
      lang: 'zh-CN',
      description: 'C++ 知识库：从 C++98 到 C++29',
      themeConfig: {
        nav: [
          { text: '首页', link: '/' },
          { text: '标准版本', items: [
            { text: 'C++98', link: '/standards/cpp98/' },
            { text: 'C++03', link: '/standards/cpp03/' },
            { text: 'C++11', link: '/standards/cpp11/' },
            { text: 'C++14', link: '/standards/cpp14/' },
            { text: 'C++17', link: '/standards/cpp17/' },
            { text: 'C++20', link: '/standards/cpp20/' },
            { text: 'C++23', link: '/standards/cpp23/' },
            { text: 'C++26', link: '/standards/cpp26/' },
            { text: 'C++29', link: '/standards/cpp29/' },
          ]},
          { text: '专题', link: '/topics/' },
          { text: '知名库', link: '/libraries/' },
          { text: '参考资料', link: '/references/' },
          { text: '博客', link: '/blog/' },
        ],
        sidebar: sidebar(true),
        outline: { label: '页面导航', level: [2, 3] as [number, number] },
        docFooter: { prev: '上一篇', next: '下一篇' },
        lastUpdated: { text: '最后更新于' },
        editLink: { pattern: githubEdit, text: '在 GitHub 上编辑此页面' },
        footer: {
          message: '基于 MIT 许可发布',
          copyright: '© 2026 Modern C++ Knowledge Base',
        },
        search: {
          provider: 'local' as const,
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
      },
    },
    en: {
      label: 'English',
      lang: 'en-US',
      description: 'C++ Knowledge Base: From C++98 to C++29',
      themeConfig: {
        nav: [
          { text: 'Home', link: '/en/' },
          { text: 'Standards', items: [
            { text: 'C++98', link: '/en/standards/cpp98/' },
            { text: 'C++03', link: '/en/standards/cpp03/' },
            { text: 'C++11', link: '/en/standards/cpp11/' },
            { text: 'C++14', link: '/en/standards/cpp14/' },
            { text: 'C++17', link: '/en/standards/cpp17/' },
            { text: 'C++20', link: '/en/standards/cpp20/' },
            { text: 'C++23', link: '/en/standards/cpp23/' },
            { text: 'C++26', link: '/en/standards/cpp26/' },
            { text: 'C++29', link: '/en/standards/cpp29/' },
          ]},
          { text: 'Topics', link: '/en/topics/' },
          { text: 'Libraries', link: '/en/libraries/' },
          { text: 'References', link: '/en/references/' },
          { text: 'Blog', link: '/en/blog/' },
        ],
        sidebar: sidebar(false, '/en'),
        outline: { label: 'On this page', level: [2, 3] as [number, number] },
        docFooter: { prev: 'Previous', next: 'Next' },
        lastUpdated: { text: 'Last updated at' },
        editLink: { pattern: githubEdit, text: 'Edit this page on GitHub' },
        footer: {
          message: 'Released under the MIT License',
          copyright: '© 2026 Modern C++ Knowledge Base',
        },
        search: {
          provider: 'local' as const,
          options: {
            translations: {
              button: { buttonText: 'Search docs', buttonAriaLabel: 'Search' },
              modal: {
                noResultsText: 'No results found',
                resetButtonTitle: 'Clear query',
                footer: { selectText: 'Select', navigateText: 'Navigate', closeText: 'Close' },
              },
            },
          },
        },
      },
    },
  },

  themeConfig: {
    logo: '/logo.svg',
    siteTitle: 'Modern C++',
    socialLinks,
  },
})
