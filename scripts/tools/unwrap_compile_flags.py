#!/usr/bin/env python3
"""Unwrap compile-time flags that are permanently enabled in the Vulkan renderer.

For each flag in ALWAYS_ON, removes preprocessor guards and keeps the enabled path:
  - #ifdef FLAG ... #endif  -> body only
  - #ifdef FLAG ... #else ... #endif -> true branch only
  - #ifndef FLAG ... #endif -> delete block
  - #ifndef FLAG ... #else ... #endif -> else branch only
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
VK_DIR = ROOT / "renderers" / "vulkan"
COMMON_DIR = ROOT / "renderers" / "common"

ALWAYS_ON = ("USE_VULKAN", "USE_VBO", "USE_FOG_COLLAPSE", "USE_VK_PBR")

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


def unwrap_flag(text: str, flag: str) -> tuple[str, int]:
    lines = text.splitlines(keepends=True)
    out: list[str] = []
    i = 0
    changes = 0

    ifdef_re = re.compile(rf"^\s*#\s*ifdef\s+{re.escape(flag)}\b")
    ifndef_re = re.compile(rf"^\s*#\s*ifndef\s+{re.escape(flag)}\b")
    else_re = re.compile(r"^\s*#\s*else\b")
    elif_re = re.compile(r"^\s*#\s*elif\b")
    endif_re = re.compile(r"^\s*#\s*endif\b")

    while i < len(lines):
        line = lines[i]
        if ifdef_re.match(line) or ifndef_re.match(line):
            is_ifndef = ifndef_re.match(line) is not None
            depth = 1
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
                            kept = false_lines if in_false else []
                        elif in_false:
                            kept = true_lines
                        else:
                            kept = true_lines
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
    flags = sys.argv[1:] if len(sys.argv) > 1 else list(ALWAYS_ON)
    search_dirs = [VK_DIR, COMMON_DIR]

    for flag in flags:
        total_changes = 0
        touched: list[str] = []

        for base in search_dirs:
            if not base.is_dir():
                continue
            for path in sorted(base.rglob("*")):
                if not path.is_file() or not should_process(path):
                    continue
                original = path.read_text(encoding="utf-8")
                updated, changes = unwrap_flag(original, flag)
                if changes and updated != original:
                    path.write_text(updated, encoding="utf-8")
                    total_changes += changes
                    touched.append(str(path.relative_to(ROOT)))

        print(f"{flag}: unwrapped {total_changes} blocks in {len(touched)} files")
        for name in touched:
            print(f"  {name}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
