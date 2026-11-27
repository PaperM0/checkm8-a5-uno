#!/usr/bin/env python3
"""Minimal unified-diff applier (exact context matching, no git required)."""
import re
import sys
from pathlib import Path

HUNK_RE = re.compile(r"^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@")


def parse(patch_text):
    files = []
    cur = None
    lines = patch_text.split("\n")
    i = 0
    while i < len(lines):
        ln = lines[i]
        if ln.startswith("diff --git"):
            cur = None
            i += 1
            continue
        if ln.startswith("--- ") and i + 1 < len(lines) and lines[i + 1].startswith("+++ "):
            old = ln[4:].split("\t")[0].strip()
            new = lines[i + 1][4:].split("\t")[0].strip()
            name = new[2:] if new.startswith("b/") else new
            cur = {"name": name, "hunks": []}
            files.append(cur)
            i += 2
            continue
        m = HUNK_RE.match(ln)
        if m and cur is not None:
            old_start = int(m.group(1))
            new_start = int(m.group(3))
            j = i + 1
            old_lines, new_lines = [], []
            while j < len(lines):
                l = lines[j]
                if HUNK_RE.match(l) or l.startswith("diff --git"):
                    break
                if l.startswith("\\"):
                    j += 1
                    continue
                if l.startswith(" "):
                    old_lines.append(l[1:])
                    new_lines.append(l[1:])
                elif l.startswith("-"):
                    old_lines.append(l[1:])
                elif l.startswith("+"):
                    new_lines.append(l[1:])
                elif l == "":
                    break
                else:
                    break
                j += 1
            cur["hunks"].append((old_start, old_lines, new_lines))
            i = j
            continue
        i += 1
    return files


def apply_file(target: Path, hunks):
    text = target.read_text(encoding="utf-8", errors="surrogateescape")
    lines = text.split("\n")
    applied = 0
    for old_start, old_lines, new_lines in hunks:
        if not old_lines:
            continue
        # search outward from the hinted position, then fall back to whole file
        idx = None
        for delta in range(0, len(lines) + 1):
            for cand in (old_start - 1 + delta, old_start - 1 - delta):
                if 0 <= cand <= len(lines) - len(old_lines) and lines[cand:cand + len(old_lines)] == old_lines:
                    idx = cand
                    break
            if idx is not None:
                break
        if idx is None:
            print(f"  !! context not found for hunk @-{old_start} in {target.name}")
            continue
        lines[idx:idx + len(old_lines)] = new_lines
        applied += 1
    target.write_text("\n".join(lines), encoding="utf-8", errors="surrogateescape")
    return applied, len(hunks)


def main():
    patch = Path(sys.argv[1])
    root = Path(sys.argv[2])
    files = parse(patch.read_text(encoding="utf-8", errors="surrogateescape"))
    total_ok = total = 0
    for f in files:
        target = root / f["name"]
        if not target.exists():
            print(f"MISSING {target}")
            continue
        ok, n = apply_file(target, f["hunks"])
        total_ok += ok
        total += n
        print(f"{f['name']}: {ok}/{n} hunks applied")
    print(f"TOTAL: {total_ok}/{total}")
    return 0 if total_ok == total and total else 1


if __name__ == "__main__":
    sys.exit(main())
