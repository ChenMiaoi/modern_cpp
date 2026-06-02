#!/bin/bash
# GCC GIMPLE dump 生成脚本
# 用法: bash gcc-dump.sh <source.cpp>

set -euo pipefail

SRC="${1:?用法: $0 <source.cpp>}"
BASE="${SRC%.cpp}"

echo "=== 生成 GIMPLE dump ==="
g++ -O0 -fdump-tree-all -S "${SRC}" -o /dev/null
g++ -O2 -fdump-tree-all -S "${SRC}" -o /dev/null

echo "=== 优化前 GIMPLE (原始) ==="
DUMP_O0=$(ls ${BASE}.*.gimple 2>/dev/null | head -1)
if [ -n "$DUMP_O0" ]; then
    head -40 "$DUMP_O0"
fi

echo ""
echo "=== 优化后 GIMPLE ==="
DUMP_O2=$(ls ${BASE}.*.optimized 2>/dev/null | head -1 || ls ${BASE}.*.gimple 2>/dev/null | tail -1)
if [ -n "$DUMP_O2" ]; then
    head -40 "$DUMP_O2"
fi

echo ""
echo "=== RTL dump ==="
g++ -O2 -fdump-rtl-all -S "${SRC}" -o /dev/null 2>&1 | head -10 || true
RTL=$(ls ${BASE}.*.rtl 2>/dev/null | head -1)
if [ -n "$RTL" ]; then
    head -20 "$RTL"
fi

echo ""
echo "生成的 dump 文件:"
ls -la ${BASE}.* 2>/dev/null | head -20
