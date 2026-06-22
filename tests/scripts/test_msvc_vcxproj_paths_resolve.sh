#!/usr/bin/env bash
# MSVC vcxproj ClCompile paths must exist on disk (after layout bridge).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
MSVC_DIR="${ROOT}/engine/platform/win32/msvc2017"
PROJECTS=(quake3e quake3e-ded botlib vulkan)

"${ROOT}/scripts/layout_forwarding_symlinks.sh" >/dev/null

python3 - "$ROOT" "$MSVC_DIR" "${PROJECTS[@]}" <<'PY'
import re
import sys
from pathlib import Path

root = Path(sys.argv[1])
msvc_dir = Path(sys.argv[2])
projects = sys.argv[3:]

skip_prefixes = (r"..\win_", r"..\sdl_", r"..\qgl")

for project in projects:
    vcx = msvc_dir / f"{project}.vcxproj"
    if not vcx.is_file():
        print(f"FAIL: missing {vcx}", file=sys.stderr)
        sys.exit(1)
    text = vcx.read_text(encoding="utf-8", errors="replace")
    broken = []
    for m in re.finditer(r'<ClCompile Include="([^"]+)"', text):
        rel = m.group(1)
        if rel.startswith("$(ProjectDir)"):
            rel = rel[len("$(ProjectDir)") :]
        norm = rel.replace("\\", "/")
        if any(norm.startswith(p.replace("\\", "/")) for p in skip_prefixes):
            continue
        path = (msvc_dir / norm).resolve()
        if not path.is_file():
            broken.append(f"{rel} -> {path}")
    if broken:
        print(f"FAIL: {project} unresolved ClCompile paths:", file=sys.stderr)
        for line in broken[:20]:
            print(f"  {line}", file=sys.stderr)
        if len(broken) > 20:
            print(f"  ... and {len(broken) - 20} more", file=sys.stderr)
        sys.exit(1)
    count = len(list(re.finditer(r"<ClCompile Include", text)))
    print(f"  {project}: {count} paths OK")

print("test_msvc_vcxproj_paths_resolve: passed")
PY
