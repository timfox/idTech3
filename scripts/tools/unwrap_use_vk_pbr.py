#!/usr/bin/env python3
"""Unwrap #ifdef USE_VK_PBR / #else / #endif in C/C++ sources (PBR path always on)."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
VK_DIR = ROOT / "renderers" / "vulkan"

SKIP_SUFFIXES = {".tmpl", ".glsl", ".frag", ".vert", ".comp"}
SKIP_NAMES = {"json.hpp", "json.h"}


def should_process(path: Path) -> bool:
    if path.suffix not in {".c", ".h", ".cpp", ".inc"}:
        return False
    if path.name in SKIP_NAMES:
        return False
    if path.suffix in SKIP_SUFFIXES:
        return False
    return True


def unwrap_use_vk_pbr(text: str) -> tuple[str, int]:
    lines = text.splitlines(keepends=True)
    out: list[str] = []
    i = 0
    changes = 0

    ifdef_re = re.compile(r"^\s*#\s*ifdef\s+USE_VK_PBR\b")
    ifndef_re = re.compile(r"^\s*#\s*ifndef\s+USE_VK_PBR\b")
    else_re = re.compile(r"^\s*#\s*else\b")
    elif_re = re.compile(r"^\s*#\s*elif\b")
    endif_re = re.compile(r"^\s*#\s*endif\b")

    while i < len(lines):
        line = lines[i]
        if ifdef_re.match(line) or ifndef_re.match(line):
            is_ifndef = ifndef_re.match(line) is not None
            depth = 1
            ifdef_line = i
            i += 1
            true_lines: list[str] = []
            false_lines: list[str] = []
            in_false = False

            while i < len(lines) and depth > 0:
                cur = lines[i]
                if ifdef_re.match(cur) or ifndef_re.match(cur) or elif_re.match(cur):
                    depth += 1
                    (false_lines if in_false else true_lines).append(cur)
                elif else_re.match(cur) and depth == 1:
                    in_false = True
                elif endif_re.match(cur):
                    depth -= 1
                    if depth > 0:
                        (false_lines if in_false else true_lines).append(cur)
                    else:
                        if is_ifndef:
                            kept = false_lines
                        elif in_false:
                            kept = true_lines
                        else:
                            kept = true_lines
                        if in_false or is_ifndef:
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
    touched: list[str] = []

    for path in sorted(VK_DIR.rglob("*")):
        if not path.is_file() or not should_process(path):
            continue
        original = path.read_text(encoding="utf-8")
        updated, changes = unwrap_use_vk_pbr(original)
        if changes and updated != original:
            path.write_text(updated, encoding="utf-8")
            total_changes += changes
            touched.append(str(path.relative_to(ROOT)))

    print(f"Unwrapped {total_changes} USE_VK_PBR blocks in {len(touched)} files:")
    for name in touched:
        print(f"  {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
