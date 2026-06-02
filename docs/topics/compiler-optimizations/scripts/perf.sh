#!/bin/bash
# perf stat 分析脚本 (需要 Linux + perf)
# 用法: bash perf.sh <binary> [args...]

set -euo pipefail

BIN="${1:?用法: $0 <binary> [args...]}"
shift

echo "=== perf stat ==="
perf stat -d -d "$BIN" "$@" 2>&1

echo ""
echo "=== perf stat (详细缓存) ==="
perf stat -e cache-references,cache-misses,branch-instructions,branch-misses,instructions,cycles "$BIN" "$@" 2>&1

echo ""
echo "=== 生成火焰图数据 (需要 perf record) ==="
echo "运行: perf record -g "$BIN" "$@""
echo "然后: perf script | stackcollapse-perf.pl | flamegraph.pl > flame.svg"

echo ""
echo "=== 关键指标说明 ==="
echo "  IPC (Instructions Per Cycle): 越高越好，通常 1-4"
echo "  Cache Miss Rate: 越低越好，< 5% 为佳"
echo "  Branch Miss Rate: 越低越好，< 2% 为佳"
echo "  L1-dcache-load-misses: L1 数据缓存未命中"
echo "  LLC-load-misses: 最后一级缓存未命中"
