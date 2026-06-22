#!/usr/bin/env python3
"""Compare CMake MSVC manifest vs hand-maintained vcxproj ClCompile lists."""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

from msvc_paths import manifest_entry_to_vcxproj, normalize_canonical

MSVC_DIR_REL = Path("engine/platform/win32/msvc2017")

PROJECT_GROUPS: dict[str, list[str]] = {
    "quake3e": ["qcommon", "server", "client"],
    "quake3e-ded": ["qcommon", "server"],
    "botlib": ["botlib"],
    "vulkan": ["renderer_vulkan", "renderer_common"],
}

# Per-group minimum fraction of manifest entries present in vcxproj (overlap).
DEFAULT_MIN_COVERAGE: dict[str, dict[str, float]] = {
    "quake3e": {"qcommon": 0.95, "server": 0.90, "client": 0.99},
    "quake3e-ded": {"qcommon": 0.95, "server": 0.90},
    "botlib": {"botlib": 0.95},
    "vulkan": {"renderer_vulkan": 0.95, "renderer_common": 0.95},
}

IGNORE_MANIFEST_MISSING_PREFIXES = (
    "engine/core/net_dtls.c",
    "engine/core/net_sdr.c",
    "engine/core/vm_aarch64.c",
    "engine/core/vm_armv7l.c",
    "engine/core/vm_powerpc.c",
    "src/qcommon/net_dtls.c",
    "src/qcommon/net_sdr.c",
    "src/qcommon/vm_aarch64.c",
    "src/qcommon/vm_armv7l.c",
    "src/qcommon/vm_powerpc.c",
)

IGNORE_VCXPROJ_EXTRA_PREFIXES = (
    r"..\win_",
    r"..\sdl_",
    r"..\..\..\external\\",
    r"..\..\renderers\common\tr_image_jpg.c",
)


def normalize_vcxproj_path(path: str) -> str:
    p = path.replace("/", "\\")
    if p.startswith("$(ProjectDir)"):
        p = p[len("$(ProjectDir)") :]
    return p


def parse_clcompile(vcxproj: Path) -> set[str]:
    text = vcxproj.read_text(encoding="utf-8", errors="replace")
    out: set[str] = set()
    for m in re.finditer(r'<ClCompile Include="([^"]+)"', text):
        out.add(normalize_vcxproj_path(m.group(1)))
    return out


def should_ignore_manifest_miss(canon: str) -> bool:
    norm = canon.replace("\\", "/")
    return any(norm.endswith(suffix) for suffix in IGNORE_MANIFEST_MISSING_PREFIXES)


def should_ignore_vcxproj_extra(path: str) -> bool:
    p = path.replace("/", "\\")
    return any(p.startswith(prefix) for prefix in IGNORE_VCXPROJ_EXTRA_PREFIXES)


def skip_windows_only_manifest(canon: str, group: str, project: str) -> bool:
    norm = canon.replace("\\", "/")
    if project in ("quake3e", "quake3e-ded") and group == "client":
        if norm.startswith("engine/asm/") and norm.endswith((".s", ".S")):
            return True
        if norm.startswith("src/asm/") and norm.endswith((".s", ".S")):
            return True
    return False


def group_coverage(
    root: Path, manifest: dict, group: str, vcx_sources: set[str], *, depth3: bool, project: str
) -> tuple[float, int, int, list[str]]:
    expected: set[str] = set()
    unmapped: list[str] = []
    for rel in manifest.get(group, []):
        canon = normalize_canonical(root, rel)
        if should_ignore_manifest_miss(canon) or skip_windows_only_manifest(canon, group, project):
            continue
        mapped = manifest_entry_to_vcxproj(root, rel, depth3=depth3)
        if mapped is None:
            unmapped.append(canon)
            continue
        expected.add(mapped.replace("/", "\\"))
    if unmapped:
        return 0.0, 0, len(unmapped), unmapped
    if not expected:
        return 1.0, 0, 0, []
    hit = len(expected & vcx_sources)
    return hit / len(expected), hit, len(expected), []


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("root", type=Path, nargs="?", default=Path(__file__).resolve().parents[2])
    ap.add_argument("--manifest", type=Path, required=True)
    ap.add_argument("--project", choices=sorted(PROJECT_GROUPS), default="quake3e")
    ap.add_argument("--min-coverage", type=float, default=-1.0)
    args = ap.parse_args()

    root = args.root.resolve()
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    vcxproj = root / MSVC_DIR_REL / f"{args.project}.vcxproj"
    if not vcxproj.is_file():
        print(f"FAIL: missing {vcxproj}", file=sys.stderr)
        return 1

    depth3 = args.project == "vulkan"
    vcx_sources = parse_clcompile(vcxproj)
    thresholds = DEFAULT_MIN_COVERAGE.get(args.project, {})
    failed = False

    print(f"project={args.project} vcxproj_clcompile={len(vcx_sources)}")
    for group in PROJECT_GROUPS[args.project]:
        cov, hit, total, unmapped = group_coverage(
            root, manifest, group, vcx_sources, depth3=depth3, project=args.project
        )
        need = args.min_coverage if args.min_coverage >= 0 else thresholds.get(group, 0.0)
        status = "OK" if cov >= need and not unmapped else "FAIL"
        print(f"  {group}: coverage={cov:.1%} ({hit}/{total}) need>={need:.0%} [{status}]")
        if unmapped:
            print("    unmapped:", *unmapped[:8], sep="\n      ")
            failed = True
        if cov < need:
            failed = True

    if failed:
        print("FAIL: check_vcxproj_drift", file=sys.stderr)
        return 1
    print("PASS: check_vcxproj_drift")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
