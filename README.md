# Modern C++ Knowledge Base

[![cpplings](https://github.com/ChenMiaoi/modern_cpp/actions/workflows/cpplings.yml/badge.svg)](https://github.com/ChenMiaoi/modern_cpp/actions/workflows/cpplings.yml)
[![Deploy to GitHub Pages](https://github.com/ChenMiaoi/modern_cpp/actions/workflows/deploy.yml/badge.svg)](https://github.com/ChenMiaoi/modern_cpp/actions/workflows/deploy.yml)

Modern C++ Knowledge Base is a bilingual C++ learning and reference project covering language evolution from C++98 through C++29, implementation notes, library internals, and runnable exercises.

The documentation site is built with VitePress. Exercises are managed by `exercises/cpplings.mjs` and are verified in CI on Linux/GCC and macOS/Homebrew LLVM.

## Contents

- Versioned C++ standard notes from C++98 to C++29.
- Deep dives on object model, templates, overload resolution, memory model, optimization, ABI, and standard library behavior.
- Implementation references for major C++ libraries and toolchains.
- Compile-and-run exercises with checked solutions.
- GitHub Pages deployment configuration.

## Requirements

### Documentation site

- Node.js 22 LTS
- npm 10+

### Exercise verification

Linux CI uses GCC on Ubuntu 24.04.

macOS development and CI use Homebrew LLVM:

```bash
brew install llvm
```

On Apple Silicon, Homebrew installs LLVM under `/opt/homebrew/opt/llvm`. On Intel macOS, use `brew --prefix llvm` instead of hard-coding the prefix.

## Quick start

```bash
npm ci
npm run dev
```

Open the VitePress site at the URL printed by the dev server. For this repository, the configured base path is `/modern_cpp/`.

## Build

```bash
npm run build
```

The static site is emitted to:

```text
docs/.vitepress/dist
```

Preview the production build locally:

```bash
npm run preview -- --host 127.0.0.1
```

## Exercises

Verify all checked solutions with the default compiler available on `PATH`:

```bash
node exercises/cpplings.mjs verify --solutions --ci
```

Verify with Homebrew LLVM on macOS:

```bash
LLVM_PREFIX="$(brew --prefix llvm)"
env PATH="$LLVM_PREFIX/bin:$PATH" \
  node exercises/cpplings.mjs verify --solutions --ci \
  --compiler "$LLVM_PREFIX/bin/clang++"
```

`std::mdspan` has an implementation-facing compatibility difference across standard libraries: some expose element access through `operator()`, while libc++ uses the C++23 multidimensional `operator[]` form. The mdspan exercise uses `mdspan_at` so the same solution works on GCC/libstdc++ and Homebrew LLVM/libc++.

## Quality checks

Run the documentation build:

```bash
npm run build
```

Check Markdown frontmatter:

```bash
python3 scripts/check_frontmatter.py
```

Check knowledge map coverage:

```bash
python3 scripts/check_knowledge_map_coverage.py
```

The coverage check is intentionally stricter than the current corpus coverage. Treat failures as a content coverage signal, not as a site build failure.

## Continuous integration

The main CI workflow is `.github/workflows/cpplings.yml`.

It runs on pull requests and pushes to `main`:

- Documentation build on Ubuntu with Node.js 22.
- Exercise verification on Ubuntu 24.04 with GCC.
- Exercise verification on macOS with Homebrew LLVM/clang++.

## Deployment

GitHub Pages deployment is defined in `.github/workflows/deploy.yml`.

Deployment is triggered by:

- Pushes to `main`.
- Manual `workflow_dispatch` from GitHub Actions.

The deployment workflow runs:

```bash
npm ci
npm run build
```

and uploads:

```text
docs/.vitepress/dist
```

The production site base path is configured in `docs/.vitepress/config.ts` as:

```text
/modern_cpp/
```

## Repository layout

```text
docs/                 VitePress documentation source
exercises/            cpplings exercises, solutions, and runner
examples/             standalone C++ examples
benchmarks/           performance benchmark sources
references/           local reference material and implementation notes
scripts/              repository validation scripts
.github/workflows/    CI and GitHub Pages deployment
```

## Contributing

For content changes:

1. Update the relevant Markdown files under `docs/`.
2. Run `npm run build`.
3. Run `python3 scripts/check_frontmatter.py`.
4. If the change affects knowledge map coverage, update `knowledge-map.yml` and run `python3 scripts/check_knowledge_map_coverage.py`.

For exercise changes:

1. Update the exercise and matching solution.
2. Verify the solution with `node exercises/cpplings.mjs verify --solutions --ci`.
3. On macOS, also verify with Homebrew LLVM as shown above.

## License

No license file is currently present in this repository. Add one before distributing the project as open-source software.
