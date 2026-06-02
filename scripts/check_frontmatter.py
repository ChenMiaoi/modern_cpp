#!/usr/bin/env python3
"""Check that all docs/*.md files have YAML frontmatter with required fields."""
import sys
import os
import re
import glob

REQUIRED_FIELDS = ["title", "topic", "feature", "standard", "status_checked_at"]
OPTIONAL_FIELDS = ["exercises", "solutions", "implementation", "verification",
                   "standard_refs", "proposals", "proposal", "syntax_status",
                   "compiler_support"]

def parse_frontmatter(content):
    """Extract YAML frontmatter from markdown content."""
    if not content.startswith("---"):
        return None
    end = content.find("---", 3)
    if end == -1:
        return None
    fm_text = content[3:end].strip()
    # Simple key extraction (not full YAML parser to avoid dependency)
    fields = {}
    for line in fm_text.split("\n"):
        line = line.strip()
        if ":" in line and not line.startswith("-") and not line.startswith("#"):
            key = line.split(":")[0].strip()
            if key:
                fields[key] = True
    return fields

def check_docs(docs_dir):
    """Check all .md files in docs/ for frontmatter."""
    errors = []
    checked = 0
    passed = 0

    pattern = os.path.join(docs_dir, "**", "*.md")
    for fpath in sorted(glob.glob(pattern, recursive=True)):
        # Skip index.md files at directory roots (they may have different structure)
        basename = os.path.basename(fpath)
        rel_path = os.path.relpath(fpath, docs_dir)

        with open(fpath, "r", encoding="utf-8") as f:
            content = f.read()

        checked += 1
        fm = parse_frontmatter(content)

        if fm is None:
            errors.append(f"MISSING frontmatter: {rel_path}")
            continue

        missing = [field for field in REQUIRED_FIELDS if field not in fm]
        if missing:
            errors.append(f"MISSING fields {missing}: {rel_path}")
            continue

        passed += 1

    return checked, passed, errors

def main():
    if len(sys.argv) > 1:
        docs_dir = sys.argv[1]
    else:
        docs_dir = "docs"

    if not os.path.isdir(docs_dir):
        print(f"Error: directory '{docs_dir}' not found")
        sys.exit(1)

    checked, passed, errors = check_docs(docs_dir)

    print(f"Checked: {checked} files")
    print(f"Passed:  {passed} files")
    print(f"Errors:  {len(errors)} files")

    if errors:
        print("\n--- Errors ---")
        for err in errors:
            print(f"  {err}")
        sys.exit(1)
    else:
        print("\nAll docs have valid frontmatter!")
        sys.exit(0)

if __name__ == "__main__":
    main()
