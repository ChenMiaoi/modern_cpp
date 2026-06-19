# Modern C++ Knowledge Base

Modern C++ 知识库，覆盖 C++98 到 C++29 的标准演进、专题文章、库实现参考和可编译练习。站点使用 VitePress 构建，练习由 `exercises/cpplings.mjs` 管理。

## macOS 环境准备

推荐环境：

- macOS 14+
- Xcode Command Line Tools
- Homebrew
- Homebrew LLVM / Clang
- Node.js 22 LTS（CI 使用 Node 22；GitHub Pages 部署工作流使用 Node 20）
- npm 10+

安装基础工具：

```bash
xcode-select --install
```

安装 Homebrew LLVM 工具链：

```bash
brew install llvm
```

Homebrew LLVM 默认不会覆盖系统 `/usr/bin/clang`。在当前 shell 中优先使用 Homebrew 工具链：

```bash
export PATH="/opt/homebrew/opt/llvm/bin:$PATH"
export CC=/opt/homebrew/opt/llvm/bin/clang
export CXX=/opt/homebrew/opt/llvm/bin/clang++
```

安装 Node.js（二选一）：

```bash
# Homebrew
brew install node@22

# 或使用 nvm
nvm install 22
nvm use 22
```

确认工具链：

```bash
/opt/homebrew/opt/llvm/bin/clang --version
/opt/homebrew/opt/llvm/bin/clang++ --version
/opt/homebrew/opt/llvm/bin/llvm-config --version
node -v
npm -v
```

## 安装依赖

```bash
npm ci
```

## 本地开发

启动 VitePress 开发服务器：

```bash
npm run dev
```

默认访问地址通常为：

```text
http://localhost:5173/modern_cpp/
```

## 构建

生成静态站点：

```bash
npm run build
```

构建产物目录：

```text
docs/.vitepress/dist
```

本地预览构建产物：

```bash
npm run preview -- --host 127.0.0.1
```

访问：

```text
http://127.0.0.1:4173/modern_cpp/
```

## 练习验证

验证所有 solution 文件，并显式使用 Homebrew clang++：

```bash
env PATH="/opt/homebrew/opt/llvm/bin:$PATH" \
  node exercises/cpplings.mjs verify --solutions --ci \
  --compiler /opt/homebrew/opt/llvm/bin/clang++
```

说明：CI 在 Ubuntu + GCC 上验证练习。macOS 的 Homebrew LLVM 仍使用 libc++，对部分较新的 C++23 库特性支持可能不同，例如 `std::mdspan` 的访问 API；如果本地练习验证失败，优先用 CI 的 Ubuntu + GCC 结果作为发布门禁。

## 文档检查

检查 frontmatter：

```bash
python3 scripts/check_frontmatter.py
```

检查知识图谱覆盖率：

```bash
python3 scripts/check_knowledge_map_coverage.py
```

## 部署

本仓库通过 GitHub Pages 自动部署。

工作流文件：

```text
.github/workflows/deploy.yml
```

触发方式：

- push 到 `main`
- 在 GitHub Actions 页面手动触发 `Deploy to GitHub Pages`

部署流程：

```bash
npm ci
npm run build
```

GitHub Actions 上传的 Pages artifact 路径：

```text
docs/.vitepress/dist
```

站点配置中的 base path 为：

```text
/modern_cpp/
```

## 常用命令

```bash
npm ci
env PATH="/opt/homebrew/opt/llvm/bin:$PATH" CC=/opt/homebrew/opt/llvm/bin/clang CXX=/opt/homebrew/opt/llvm/bin/clang++ npm run dev
env PATH="/opt/homebrew/opt/llvm/bin:$PATH" CC=/opt/homebrew/opt/llvm/bin/clang CXX=/opt/homebrew/opt/llvm/bin/clang++ npm run build
npm run preview -- --host 127.0.0.1
env PATH="/opt/homebrew/opt/llvm/bin:$PATH" node exercises/cpplings.mjs verify --solutions --ci --compiler /opt/homebrew/opt/llvm/bin/clang++
python3 scripts/check_frontmatter.py
python3 scripts/check_knowledge_map_coverage.py
```
