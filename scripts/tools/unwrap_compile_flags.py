#!/usr/bin/env python3
"""Unwrap #ifdef MACRO / #else / #endif when MACRO is always defined (keep true branch)."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
VK_DIRS = [ROOT / "renderers" / "vulkan", ROOT / "renderers" / "common"]

SKIP_SUFFIXES = {".tmpl", ".glsl", ".frag", ".vert", ".comp"}
SKIP_NAMES = {"json.hpp", "json.h"}


ALWAYS_ON_MACROS = frozenset({
    "USE_VBO",
    "USE_FOG_COLLAPSE",
    "USE_VK_PBR",
    "USE_VULKAN",
    "VK_CUBEMAP",
    "VK_PBR_BRDFLUT",
})


def should_process(path: Path) -> bool:
    if path.suffix not in {".c", ".h", ".cpp", ".hpp", ".inc"}:
        return False
    if path.name in SKIP_NAMES:
        return False
    if path.suffix in SKIP_SUFFIXES:
        return False
    return True


def _defined_only_condition(line: str, macros: frozenset[str]) -> bool:
    """True when #if is only defined() checks on macros from the always-on set."""
    m = re.match(r"^\s*#\s*if\s+(.+)$", line)
    if not m:
        return False
    expr = m.group(1).strip()
    if "||" in expr or "!" in expr:
        return False
    parts = re.split(r"\s*&&\s*", expr)
    for part in parts:
        part = part.strip()
        dm = re.match(r"defined\s*\(\s*(\w+)\s*\)", part)
        if not dm:
            return False
        if dm.group(1) not in macros:
            return False
    return bool(parts)


def _matches_macro_directive(line: str, macro: str) -> bool:
    if re.match(rf"^\s*#\s*ifdef\s+{re.escape(macro)}\b", line):
        return True
    if re.match(rf"^\s*#\s*ifndef\s+{re.escape(macro)}\b", line):
        return True
    if _defined_only_condition(line, frozenset({macro})):
        return True
    return False


def unwrap_macro(text: str, macro: str) -> tuple[str, int]:
    lines = text.splitlines(keepends=True)
    out: list[str] = []
    i = 0
    changes = 0

    ifdef_re = re.compile(rf"^\s*#\s*ifdef\s+{re.escape(macro)}\b")
    ifndef_re = re.compile(rf"^\s*#\s*ifndef\s+{re.escape(macro)}\b")
    defined_re = re.compile(rf"^\s*#\s*if\s+defined\s*\(\s*{re.escape(macro)}\s*\)\s*$")
    else_re = re.compile(r"^\s*#\s*else\b")
    elif_re = re.compile(r"^\s*#\s*elif\b")
    endif_line_re = re.compile(r"^\s*#\s*endif\b[^\n]*\n", re.M)

    while i < len(lines):
        line = lines[i]
        is_compound_defined = _defined_only_condition(line, ALWAYS_ON_MACROS)
        if (
            ifdef_re.match(line)
            or ifndef_re.match(line)
            or defined_re.match(line)
            or is_compound_defined
        ):
            is_ifndef = ifndef_re.match(line) is not None
            depth = 1
            i += 1
            true_lines: list[str] = []
            false_lines: list[str] = []
            in_false = False
            had_else = False

            while i < len(lines) and depth > 0:
                cur = lines[i]
                if (
                    ifdef_re.match(cur)
                    or ifndef_re.match(cur)
                    or defined_re.match(cur)
                    or _matches_macro_directive(cur, macro)
                    or elif_re.match(cur)
                ):
                    depth += 1
                    (false_lines if in_false else true_lines).append(cur)
                elif else_re.match(cur) and depth == 1:
                    in_false = True
                    had_else = True
                elif endif_line_re.match(cur):
                    depth -= 1
                    if depth > 0:
                        (false_lines if in_false else true_lines).append(cur)
                    else:
                        if is_ifndef:
                            kept = false_lines if had_else else []
                        elif had_else:
                            kept = true_lines
                        else:
                            kept = true_lines
                        if is_ifndef or had_else or true_lines:
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
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "macros",
        nargs="*",
        default=sorted(ALWAYS_ON_MACROS),
        help="Compile-time macros to unwrap (default: all always-on renderer flags).",
    )
    args = parser.parse_args()

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
            for macro in args.macros:
                updated, n = unwrap_macro(updated, macro)
                file_changes += n
            if file_changes and updated != original:
                path.write_text(updated, encoding="utf-8")
                total_changes += file_changes
                touched[str(path.relative_to(ROOT))] = file_changes

    print(f"Unwrapped {total_changes} blocks across {len(touched)} files:")
    for name in sorted(touched):
        print(f"  {name} ({touched[name]})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
