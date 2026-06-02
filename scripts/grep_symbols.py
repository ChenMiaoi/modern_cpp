#!/usr/bin/env python3
"""Search for C++ symbols across reference implementation source trees."""
import sys
import os
import subprocess
import argparse

# Map library names to source paths
LIB_PATHS = {
    "libstdcxx": "references/impl/gcc/libstdc++-v3",
    "libcxx": "references/impl/llvm-project/libcxx",
    "abseil": "references/impl/abseil-cpp",
    "folly": "references/impl/folly",
    "fmt": "references/impl/fmt",
    "spdlog": "references/impl/spdlog",
    "eastl": "references/impl/EASTL",
}

def find_repo_root():
    """Find the repository root."""
    path = os.path.dirname(os.path.abspath(__file__))
    while path != os.path.dirname(path):
        if os.path.exists(os.path.join(path, "knowledge-map.yml")):
            return path
        path = os.path.dirname(path)
    return os.path.dirname(os.path.abspath(__file__))

def grep_symbol(symbol, lib=None, paths=None, case_sensitive=True):
    """Search for a symbol in source trees."""
    root = find_repo_root()

    if paths:
        search_paths = [os.path.join(root, p) for p in paths]
    elif lib:
        if lib not in LIB_PATHS:
            print(f"Unknown library: {lib}")
            print(f"Available: {', '.join(LIB_PATHS.keys())}")
            return []
        search_paths = [os.path.join(root, LIB_PATHS[lib])]
    else:
        search_paths = [os.path.join(root, "references/impl")]

    # Build grep command
    cmd = ["grep", "-rn"]
    if not case_sensitive:
        cmd.append("-i")
    cmd.extend(["--include=*.h", "--include=*.hpp", "--include=*.tcc",
                "--include=*.cpp", "--include=*.cc"])
    cmd.append(symbol)
    cmd.extend(search_paths)

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        return result.stdout.strip().split("\n") if result.stdout.strip() else []
    except FileNotFoundError:
        print("Error: grep not found. On Windows, use Git Bash or WSL.")
        return []
    except subprocess.TimeoutExpired:
        print("Error: search timed out (30s)")
        return []

def main():
    parser = argparse.ArgumentParser(description="Search for C++ symbols in reference implementations")
    parser.add_argument("symbol", help="Symbol or pattern to search for")
    parser.add_argument("--lib", "-l", choices=list(LIB_PATHS.keys()),
                        help="Limit search to specific library")
    parser.add_argument("--path", "-p", action="append",
                        help="Specific source path(s) to search (relative to repo root)")
    parser.add_argument("-i", "--ignore-case", action="store_true",
                        help="Case-insensitive search")
    parser.add_argument("--limit", "-n", type=int, default=50,
                        help="Max results to show (default: 50)")

    args = parser.parse_args()

    results = grep_symbol(args.symbol, lib=args.lib, paths=args.path,
                          case_sensitive=not args.ignore_case)

    if not results:
        print(f"No results found for '{args.symbol}'")
        sys.exit(1)

    print(f"Found {len(results)} matches for '{args.symbol}':\n")
    for line in results[:args.limit]:
        # Shorten path for readability
        line = line.replace("\\", "/")
        if "references/impl/" in line:
            idx = line.index("references/impl/")
            # Skip the "references/impl/<repo>/" prefix
            rest = line[idx + len("references/impl/"):]
            parts = rest.split("/", 1)
            if len(parts) > 1:
                short = f"[{parts[0]}] {parts[1]}"
            else:
                short = rest
            print(f"  {short}")
        else:
            print(f"  {line}")

    if len(results) > args.limit:
        print(f"\n  ... and {len(results) - args.limit} more")

if __name__ == "__main__":
    main()
