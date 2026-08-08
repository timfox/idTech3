#!/usr/bin/env python3
"""Unwrap compile-time flags that are permanently enabled in the Vulkan renderer."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
VK_DIR = ROOT / "renderers" / "vulkan"

# Always defined in tr_local.h / tr_common.h for renderers/vulkan.
ALWAYS_ON_MACROS = frozenset(
    {
        "USE_VK_PBR",
        "USE_VBO",
        "USE_VULKAN",
        "USE_FOG_COLLAPSE",
    }
)

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


def unwrap_macro(text: str, macro: str) -> tuple[str, int]:
    lines = text.splitlines(keepends=True)
    out: list[str] = []
    i = 0
    changes = 0

    ifdef_re = re.compile(rf"^\s*#\s*ifdef\s+{macro}\b")
    ifndef_re = re.compile(rf"^\s*#\s*ifndef\s+{macro}\b")
    if_defined_re = re.compile(
        rf"^\s*#\s*if\s+defined\s*\(\s*{macro}\s*\)"
    )
    if_not_defined_re = re.compile(
        rf"^\s*#\s*if\s+!\s*defined\s*\(\s*{macro}\s*\)"
    )
    else_re = re.compile(r"^\s*#\s*else\b")
    elif_re = re.compile(r"^\s*#\s*elif\b")
    endif_re = re.compile(r"^\s*#\s*endif\b")

    def is_macro_if(line: str) -> bool:
        return (
            ifdef_re.match(line)
            or ifndef_re.match(line)
            or if_defined_re.match(line)
            or if_not_defined_re.match(line)
        )

    while i < len(lines):
        line = lines[i]
        if is_macro_if(line):
            is_ifndef = (
                ifndef_re.match(line) is not None
                or if_not_defined_re.match(line) is not None
            )
            depth = 1
            i += 1
            true_lines: list[str] = []
            false_lines: list[str] = []
            in_false = False
            had_else = False

            while i < len(lines) and depth > 0:
                cur = lines[i]
                if re.match(r"^\s*#\s*if(?:def|ndef)?\b", cur) or re.match(
                    r"^\s*#\s*if\s+", cur
                ):
                    depth += 1
                    (false_lines if in_false else true_lines).append(cur)
                elif elif_re.match(cur):
                    depth += 1
                    (false_lines if in_false else true_lines).append(cur)
                elif else_re.match(cur) and depth == 1:
                    in_false = True
                    had_else = True
                elif endif_re.match(cur):
                    depth -= 1
                    if depth > 0:
                        (false_lines if in_false else true_lines).append(cur)
                    else:
                        if is_ifndef:
                            kept = false_lines
                            if false_lines:
                                changes += 1
                        elif had_else:
                            kept = true_lines
                            changes += 1
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


def unwrap_all(text: str) -> tuple[str, int]:
    total = 0
    for _ in range(8):
        round_total = 0
        for macro in sorted(ALWAYS_ON_MACROS):
            text, n = unwrap_macro(text, macro)
            round_total += n
        total += round_total
        if round_total == 0:
            break
    return text, total


def main() -> int:
    total_changes = 0
    touched: list[str] = []

    for path in sorted(VK_DIR.rglob("*")):
        if not path.is_file() or not should_process(path):
            continue
        original = path.read_text(encoding="utf-8")
        updated, changes = unwrap_all(original)
        if changes and updated != original:
            path.write_text(updated, encoding="utf-8")
            total_changes += changes
            touched.append(str(path.relative_to(ROOT)))

    print(
        f"Unwrapped {total_changes} always-on flag blocks in {len(touched)} files:"
    )
    for name in touched:
        print(f"  {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
