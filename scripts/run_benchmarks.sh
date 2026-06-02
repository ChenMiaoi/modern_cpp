#!/bin/bash
# run_benchmarks.sh — Build and run benchmark examples
#
# Usage:
#   ./scripts/run_benchmarks.sh [benchmark_name]
#   ./scripts/run_benchmarks.sh --all
#
# Prerequisites: g++ or clang++ with C++20 support, Google Benchmark (optional)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BENCH_DIR="$REPO_ROOT/benchmarks"
BUILD_DIR="$REPO_ROOT/benchmarks/build"
CXX="${CXX:-g++}"
STD="c++20"
OPT="-O2"

mkdir -p "$BUILD_DIR"

build_and_run() {
    local src="$1"
    local basename
    basename="$(basename "$src" .cpp)"
    local binary="$BUILD_DIR/$basename"

    echo "=== Building: $basename ==="
    $CXX -std=$STD $OPT "$src" -o "$binary" -lpthread 2>/dev/null || {
        echo "  [WARN] Build failed, trying without -lpthread..."
        $CXX -std=$STD $OPT "$src" -o "$binary" 2>/dev/null || {
            echo "  [ERROR] Build failed for $basename"
            return 1
        }
    }

    echo "=== Running: $basename ==="
    "$binary"
    echo ""
}

if [[ "${1:-}" == "--all" ]]; then
    echo "Running all benchmarks..."
    for src in "$BENCH_DIR"/*.cpp; do
        [[ -f "$src" ]] || continue
        build_and_run "$src"
    done
elif [[ -n "${1:-}" ]]; then
    build_and_run "$BENCH_DIR/$1.cpp"
else
    echo "Usage: $0 <benchmark_name>"
    echo "       $0 --all"
    echo ""
    echo "Available benchmarks:"
    for src in "$BENCH_DIR"/*.cpp; do
        [[ -f "$src" ]] || continue
        basename "$( "$src" .cpp)"
    done 2>/dev/null || echo "  (none found in $BENCH_DIR/)"
    exit 1
fi
