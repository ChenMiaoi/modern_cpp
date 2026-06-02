#!/bin/bash
# run_ir_examples.sh — Generate LLVM IR and assembly for C++ examples
#
# Usage:
#   ./scripts/run_ir_examples.sh [file.cpp]
#   ./scripts/run_ir_examples.sh --all
#
# Prerequisites: clang++ (for LLVM IR), g++ (for GIMPLE)
#
# Output: examples/ir_output/<basename>/
#   - O0.ll, O2.ll       (LLVM IR at different opt levels)
#   - O0.s, O2.s          (Assembly at different opt levels)
#   - O2_opt.txt          (Optimization remarks)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OUTPUT_DIR="$REPO_ROOT/examples/ir_output"
STD="c++20"

mkdir -p "$OUTPUT_DIR"

run_single() {
    local src="$1"
    local basename
    basename="$(basename "$src" .cpp)"
    local outdir="$OUTPUT_DIR/$basename"
    mkdir -p "$outdir"

    echo "=== Processing: $basename ==="

    # LLVM IR
    echo "  Generating LLVM IR (O0)..."
    clang++ -std=$STD -O0 -S -emit-llvm "$src" -o "$outdir/O0.ll" 2>/dev/null || echo "  [WARN] O0 IR failed"

    echo "  Generating LLVM IR (O2)..."
    clang++ -std=$STD -O2 -S -emit-llvm "$src" -o "$outdir/O2.ll" 2>/dev/null || echo "  [WARN] O2 IR failed"

    # Assembly
    echo "  Generating assembly (O0)..."
    clang++ -std=$STD -O0 -S "$src" -o "$outdir/O0.s" 2>/dev/null || echo "  [WARN] O0 asm failed"

    echo "  Generating assembly (O2)..."
    clang++ -std=$STD -O2 -S "$src" -o "$outdir/O2.s" 2>/dev/null || echo "  [WARN] O2 asm failed"

    # Optimization remarks
    echo "  Generating optimization remarks..."
    clang++ -std=$STD -O2 -Rpass=inline -Rpass=loop-vectorize \
        -Rpass-missed=inline -Rpass-missed=loop-vectorize \
        "$src" -o /dev/null 2>"$outdir/O2_remarks.txt" || true

    echo "  Output: $outdir/"
    echo ""
}

if [[ "${1:-}" == "--all" ]]; then
    echo "Running IR examples for all exercise files..."
    for src in "$REPO_ROOT"/exercises/**/*.cpp; do
        [[ "$src" == */solutions/* ]] && continue
        run_single "$src"
    done
elif [[ -n "${1:-}" ]]; then
    run_single "$1"
else
    echo "Usage: $0 <file.cpp>"
    echo "       $0 --all"
    echo ""
    echo "Generates LLVM IR and assembly for C++ source files."
    echo "Output: $OUTPUT_DIR/<basename>/"
    exit 1
fi
