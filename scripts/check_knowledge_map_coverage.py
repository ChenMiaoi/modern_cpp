#!/usr/bin/env python3
"""Check knowledge-map.yml coverage against docs/ and exercises/."""
import sys
import os
import glob

try:
    import yaml
except ImportError:
    yaml = None

def load_knowledge_map(path):
    """Load knowledge-map.yml."""
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()
    if yaml:
        return yaml.safe_load(content)
    docs = set()
    for line in content.split("\n"):
        line = line.strip()
        if line.startswith("- \"docs/") or line.startswith("- 'docs/"):
            p = line.strip("- \"'").strip("\"'")
            docs.add(p)
    return docs

def collect_doc_paths(data, prefix=""):
    """Recursively collect all doc paths from knowledge-map data."""
    paths = set()
    if isinstance(data, dict):
        for key, value in data.items():
            if key == "docs" and isinstance(value, list):
                for item in value:
                    if isinstance(item, str):
                        paths.add(item)
            elif key in ("exercises", "solutions") and isinstance(value, list):
                for item in value:
                    if isinstance(item, str):
                        paths.add(item)
            else:
                paths.update(collect_doc_paths(value, f"{prefix}.{key}"))
    elif isinstance(data, list):
        for item in data:
            paths.update(collect_doc_paths(item, prefix))
    return paths

def check_coverage(docs_dir, km_path):
    """Check coverage of docs against knowledge-map."""
    all_docs = set()
    for fpath in glob.glob(os.path.join(docs_dir, "**", "*.md"), recursive=True):
        rel = os.path.relpath(fpath, os.path.dirname(docs_dir)).replace("\\", "/")
        all_docs.add(rel)

    km_data = load_knowledge_map(km_path)
    km_docs = collect_doc_paths(km_data) if isinstance(km_data, dict) else km_data

    covered = all_docs & km_docs
    uncovered = all_docs - km_docs
    structural = {d for d in uncovered if d.endswith("/index.md") or d.endswith("index.md")}

    return len(all_docs), len(covered), len(uncovered), uncovered, structural

def main():
    docs_dir = "docs"
    km_path = "knowledge-map.yml"

    if not os.path.exists(km_path):
        print(f"Error: {km_path} not found")
        sys.exit(1)

    total, covered, uncovered_count, uncovered, structural = check_coverage(docs_dir, km_path)

    print(f"Total docs:     {total}")
    print(f"Covered:        {covered}")
    print(f"Uncovered:      {uncovered_count}")
    print(f"  (structural): {len(structural)}")

    non_structural_total = total - len(structural)
    non_structural_uncovered = uncovered - structural

    coverage_pct = (covered / non_structural_total * 100) if non_structural_total > 0 else 0
    print(f"\nCoverage (content docs): {coverage_pct:.1f}%")

    if non_structural_uncovered:
        print(f"\n--- Uncovered content docs ({len(non_structural_uncovered)}) ---")
        for d in sorted(non_structural_uncovered):
            print(f"  {d}")

    if coverage_pct < 95:
        print(f"\nFAIL: Coverage {coverage_pct:.1f}% < 95%")
        sys.exit(1)
    else:
        print(f"\nPASS: Coverage {coverage_pct:.1f}% >= 95%")
        sys.exit(0)

if __name__ == "__main__":
    main()
