#!/usr/bin/env python3
"""Remove #ifdef blocks for compile-time macros that are never defined."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
VK_DIRS = [ROOT / "renderers" / "vulkan", ROOT / "renderers" / "common"]

SKIP_SUFFIXES = {".tmpl", ".glsl", ".frag", ".vert", ".comp"}
SKIP_NAMES = {"json.hpp", "json.h"}

NEVER_DEFINED = frozenset({
    "USE_VBO_GRID",
    "USE_TESS_NEEDS_NORMAL",
    "USE_TESS_NEEDS_ST2",
    "USE_PMLIGHT",
})


def should_process(path: Path) -> bool:
    if path.suffix not in {".c", ".h", ".cpp", ".inc"}:
        return False
    if path.name in SKIP_NAMES:
        return False
    if path.suffix in SKIP_SUFFIXES:
        return False
    return True


def remove_macro(text: str, macro: str) -> tuple[str, int]:
    lines = text.splitlines(keepends=True)
    out: list[str] = []
    i = 0
    changes = 0

    ifdef_re = re.compile(rf"^\s*#\s*ifdef\s+{re.escape(macro)}\b")
    else_re = re.compile(r"^\s*#\s*else\b")
    elif_re = re.compile(r"^\s*#\s*elif\b")
    endif_re = re.compile(r"^\s*#\s*endif\b")
    if_re = re.compile(r"^\s*#\s*if\b")

    while i < len(lines):
        line = lines[i]
        if ifdef_re.match(line):
            depth = 1
            i += 1
            true_lines: list[str] = []
            false_lines: list[str] = []
            in_false = False
            had_else = False

            while i < len(lines) and depth > 0:
                cur = lines[i]
                if if_re.match(cur):
                    depth += 1
                    (false_lines if in_false else true_lines).append(cur)
                elif else_re.match(cur) and depth == 1:
                    in_false = True
                    had_else = True
                elif elif_re.match(cur) and depth == 1:
                    in_false = True
                    had_else = True
                    (false_lines if in_false else true_lines).append(cur)
                elif endif_re.match(cur):
                    depth -= 1
                    if depth > 0:
                        (false_lines if in_false else true_lines).append(cur)
                    else:
                        kept = false_lines if had_else else []
                        if true_lines or false_lines:
                            changes += 1
                        out.extend(kept)
                else:
                    (false_lines if in_false else true_lines).append(cur)
                i += 1
            continue

        out.append(line)
        i += 1

    return "".join(out), changes


def main() -> int:
    total_changes = 0
    touched: dict[str, int] = {}

    for vk_dir in VK_DIRS:
        if not vk_dir.is_dir():
            continue
        for path in sorted(vk_dir.rglob("*")):
            if not path.is_file() or not should_process(path):
                continue
            original = path.read_text(encoding="utf-8")
            updated = original
            file_changes = 0
            for macro in sorted(NEVER_DEFINED):
                updated, n = remove_macro(updated, macro)
                file_changes += n
            if file_changes and updated != original:
                path.write_text(updated, encoding="utf-8")
                total_changes += file_changes
                touched[str(path.relative_to(ROOT))] = file_changes

    print(f"Removed {total_changes} never-defined blocks across {len(touched)} files:")
    for name in sorted(touched):
        print(f"  {name} ({touched[name]})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
