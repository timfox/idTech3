#!/usr/bin/env python3
"""Sync MSVC vcxproj ClCompile entries from CMake manifest (Phase 5d)."""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

from check_vcxproj_drift import (
    PROJECT_GROUPS,
    normalize_vcxproj_path,
    parse_clcompile,
    should_ignore_manifest_miss,
)
from msvc_paths import manifest_entry_to_vcxproj, normalize_canonical

MSVC_DIR_REL = Path("engine/platform/win32/msvc2017")
MARKER_BEGIN = "  <!-- IDTECH3_MSVC_MANIFEST_BEGIN -->"
MARKER_END = "  <!-- IDTECH3_MSVC_MANIFEST_END -->"

SEARCH_ROOTS = (
    "runtime/client",
    "runtime/server",
    "runtime/game",
    "runtime/cgame",
    "runtime/ui",
    "engine/core",
    "modules/audio",
    "modules/world",
    "modules/physics",
    "modules/navigation",
    "renderers",
    "extensions",
)

SIMPLE_CLCOMPILE_RE = re.compile(
    r'^[ \t]*<ClCompile Include="([^"]+)" */>[ \t]*\r?\n',
    re.M,
)
MULTILINE_CLCOMPILE_RE = re.compile(
    r'^[ \t]*<ClCompile Include="([^"]+)">.*?</ClCompile>[ \t]*\r?\n',
    re.M | re.S,
)


def vcxproj_dir(root: Path) -> Path:
    return root / MSVC_DIR_REL


def vcx_rel_to_path(vcx_rel: str) -> Path:
    return Path(normalize_vcxproj_path(vcx_rel).replace("\\", "/"))


def resolve_vcxproj_path(root: Path, vcx_rel: str) -> Path | None:
    path = (vcxproj_dir(root) / vcx_rel_to_path(vcx_rel)).resolve()
    return path if path.is_file() else None


def realpaths_for_vcxproj(root: Path, paths: set[str]) -> set[Path]:
    out: set[Path] = set()
    for rel in paths:
        p = resolve_vcxproj_path(root, rel)
        if p is not None:
            out.add(p)
    return out


def skip_for_windows_project(project: str, vcx_rel: str) -> bool:
    if project not in ("quake3e", "quake3e-ded"):
        return False
    return Path(vcx_rel).suffix.lower() in {".s", ".S"}


def relocate_broken_path(root: Path, vcx_rel: str) -> str | None:
    if resolve_vcxproj_path(root, vcx_rel) is not None:
        return vcx_rel
    name = Path(vcx_rel.replace("\\", "/")).name
    matches: list[Path] = []
    for rel_root in SEARCH_ROOTS:
        base = root / rel_root
        if base.is_dir():
            matches.extend(p for p in base.rglob(name) if p.is_file())
    if len(matches) != 1:
        return None
    canon = normalize_canonical(root, matches[0].relative_to(root.resolve()).as_posix())
    mapped = manifest_entry_to_vcxproj(root, canon)
    return mapped.replace("/", "\\") if mapped else None


def fix_all_clcompile_includes(text: str, root: Path) -> tuple[str, list[tuple[str, str]]]:
    fixes: list[tuple[str, str]] = []

    def repl(match: re.Match[str]) -> str:
        raw = match.group(2)
        old = normalize_vcxproj_path(raw)
        new = relocate_broken_path(root, old)
        if new and new != old:
            fixes.append((old, new))
            return f'{match.group(1)}{new}{match.group(3)}'
        return match.group(0)

    updated = re.sub(r'(<ClCompile Include=")([^"]+)(")', repl, text)
    return updated, fixes


def _dedupe_clcompile_matches(
    text: str, root: Path, pattern: re.Pattern[str], seen_real: set[Path]
) -> tuple[str, int]:
    removed = 0

    def repl(match: re.Match[str]) -> str:
        nonlocal removed
        inc = normalize_vcxproj_path(match.group(1))
        resolved = resolve_vcxproj_path(root, inc)
        if resolved is None:
            return match.group(0)
        if resolved in seen_real:
            removed += 1
            return ""
        seen_real.add(resolved)
        return match.group(0)

    return pattern.sub(repl, text), removed


def dedupe_clcompile(text: str, root: Path) -> tuple[str, int]:
    """Remove duplicate ClCompile entries that resolve to the same file.

    Unsafe with layout_forwarding_symlinks: bridge paths and hand-maintained
    entries often share a realpath, so this can delete legitimate ClCompile
    rows. Only run when --dedupe-realpath is explicitly requested.
    """
    seen: set[Path] = set()
    removed = 0
    text, n = _dedupe_clcompile_matches(text, root, MULTILINE_CLCOMPILE_RE, seen)
    removed += n
    text, n = _dedupe_clcompile_matches(text, root, SIMPLE_CLCOMPILE_RE, seen)
    removed += n
    return text, removed


def count_clcompile(text: str) -> int:
    return len(re.findall(r'<ClCompile Include="[^"]+"', text))


def collect_expected(root: Path, manifest: dict, project: str) -> list[str]:
    depth3 = project == "vulkan"
    out: list[str] = []
    for group in PROJECT_GROUPS[project]:
        for rel in manifest.get(group, []):
            canon = normalize_canonical(root, rel)
            if should_ignore_manifest_miss(canon):
                continue
            mapped = manifest_entry_to_vcxproj(root, rel, depth3=depth3)
            if mapped:
                out.append(mapped.replace("/", "\\"))
    return sorted(set(out))


def collect_missing(
    root: Path, manifest: dict, project: str, existing: set[str]
) -> list[str]:
    existing_real = realpaths_for_vcxproj(root, existing)
    missing: list[str] = []
    for rel in collect_expected(root, manifest, project):
        if rel in existing:
            continue
        if skip_for_windows_project(project, rel):
            continue
        resolved = resolve_vcxproj_path(root, rel)
        if resolved is None:
            continue
        if resolved in existing_real:
            continue
        missing.append(rel)
        existing_real.add(resolved)
    return missing


def write_manifest_block(text: str, missing: list[str]) -> str:
    if MARKER_BEGIN in text:
        pattern = re.compile(
            re.escape(MARKER_BEGIN) + r".*?" + re.escape(MARKER_END),
            re.S,
        )
        if missing:
            block = MARKER_BEGIN + "\n"
            for p in missing:
                block += f'    <ClCompile Include="{p}" />\n'
            block += MARKER_END
            return pattern.sub(lambda _m: block, text)
        return text

    if not missing:
        return text

    block = MARKER_BEGIN + "\n"
    for p in missing:
        block += f'    <ClCompile Include="{p}" />\n'
    block += MARKER_END
    insert = "\n  <ItemGroup>\n" + block + "\n  </ItemGroup>\n"
    anchor = '  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets" />'
    if anchor not in text:
        raise RuntimeError("vcxproj anchor not found")
    return text.replace(anchor, insert + anchor)


def parse_clcompile_text(text: str) -> set[str]:
    out: set[str] = set()
    for m in re.finditer(r'<ClCompile Include="([^"]+)"', text):
        out.add(normalize_vcxproj_path(m.group(1)))
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("root", type=Path, nargs="?", default=Path(__file__).resolve().parents[2])
    ap.add_argument("--manifest", type=Path, required=True)
    ap.add_argument("--project", choices=sorted(PROJECT_GROUPS), default="quake3e")
    ap.add_argument("--write", action="store_true")
    ap.add_argument(
        "--dedupe-realpath",
        action="store_true",
        help="Remove ClCompile rows that share a realpath (unsafe with layout bridges; off by default)",
    )
    args = ap.parse_args()

    root = args.root.resolve()
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    vcxproj = root / MSVC_DIR_REL / f"{args.project}.vcxproj"
    text = vcxproj.read_text(encoding="utf-8")
    before_count = count_clcompile(text)

    if args.write:
        # Relocate stale paths before optional dedupe / manifest merge.
        text, fixes = fix_all_clcompile_includes(text, root)
        if fixes:
            print(f"relocated {len(fixes)} stale ClCompile paths")
            for old, new in fixes[:20]:
                print(f"  {old} -> {new}")
            if len(fixes) > 20:
                print(f"  ... and {len(fixes) - 20} more")
        if args.dedupe_realpath:
            text, pruned = dedupe_clcompile(text, root)
            if pruned:
                print(f"deduped {pruned} duplicate ClCompile entries (--dedupe-realpath)")
        else:
            print("skipping realpath ClCompile dedupe (pass --dedupe-realpath to enable)")

    existing = parse_clcompile_text(text)
    missing = collect_missing(root, manifest, args.project, existing)

    print(f"{args.project}: {len(missing)} manifest sources to add")
    for p in missing[:40]:
        print(f"  + {p}")
    if len(missing) > 40:
        print(f"  ... and {len(missing) - 40} more")

    if not args.write:
        return 0

    text = write_manifest_block(text, missing)
    # Collapse empty manifest ItemGroup if present.
    text = re.sub(
        r"\n  <ItemGroup>\s*\r?\n  </ItemGroup>\r?\n",
        "\n",
        text,
    )
    after_count = count_clcompile(text)
    if after_count < before_count:
        print(
            f"FAIL: {args.project} ClCompile count would shrink {before_count} -> {after_count}",
            file=sys.stderr,
        )
        return 1
    vcxproj.write_text(text, encoding="utf-8", newline="\r\n")
    print(f"wrote {vcxproj} (ClCompile {before_count} -> {after_count})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
