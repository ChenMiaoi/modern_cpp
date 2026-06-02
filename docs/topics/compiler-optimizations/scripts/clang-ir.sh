#!/bin/bash
# Clang IR / LLVM IR 生成脚本
# 用法: bash clang-ir.sh <source.cpp>

set -euo pipefail

SRC="${1:?用法: $0 <source.cpp>}"
BASE="${SRC%.cpp}"

echo "=== 生成 LLVM IR ==="
clang++ -O0 -S -emit-llvm "${SRC}" -o "${BASE}_O0.ll"
clang++ -O2 -S -emit-llvm "${SRC}" -o "${BASE}_O2.ll"

echo "=== O0 IR (前 30 行) ==="
head -30 "${BASE}_O0.ll"

echo ""
echo "=== O2 IR (前 30 行) ==="
head -30 "${BASE}_O2.ll"

echo ""
echo "=== IR 差异 ==="
diff "${BASE}_O0.ll" "${BASE}_O2.ll" | head -50 || true

echo ""
echo "=== 生成 LLVM bitcode ==="
clang++ -O2 -c -emit-llvm "${SRC}" -o "${BASE}.bc"

echo "=== 查看优化 pass ==="
opt -O2 -S "${BASE}_O0.ll" -o "${BASE}_opt.ll" 2>&1 | head -20 || echo "opt 不可用"

echo ""
echo "输出文件:"
echo "  ${BASE}_O0.ll  (未优化 IR)"
echo "  ${BASE}_O2.ll  (优化后 IR)"
echo "  ${BASE}.bc     (LLVM bitcode)"
