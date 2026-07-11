#!/usr/bin/env bash
# MSVC vcxproj paths resolve after layout_forwarding_symlinks.sh (Phase 5d bridge).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
MSVC_DIR="${ROOT}/engine/platform/win32/msvc2017"

fail() { echo "FAIL: $*" >&2; exit 1; }

"${ROOT}/scripts/layout_forwarding_symlinks.sh" >/dev/null

# Windows MSVC link needs a real junction for platform/external (see layout_forwarding_windows.ps1)
# and ExternalRoot must point at third_party so libpng16.lib is found after ProjectReference builds.
rg -q "engine\\\\platform\\\\external" "${ROOT}/scripts/layout_forwarding_windows.ps1" \
	|| fail "layout_forwarding_windows.ps1 must junction engine/platform/external"
rg -q 'ExternalRoot>\$\(IdTech3ThirdParty\)' \
	"${ROOT}/engine/platform/win32/msvc2017/IdTech3Layout2026.props" \
	|| fail "IdTech3Layout2026.props ExternalRoot must be IdTech3ThirdParty (MSVC libpng link)"

python3 - "$ROOT" "$MSVC_DIR" <<'PY'
import re
import sys
from pathlib import Path

root = Path(sys.argv[1])
msvc_dir = Path(sys.argv[2])

skip_vcxproj = {"renderer2.vcxproj"}  # legacy OpenGL tree removed
deprecated_prefixes = {
    "../../renderer2",
    "../../renderers/opengl",
}

prefixes = set()
for vcx in msvc_dir.glob("*.vcxproj"):
    if vcx.name in skip_vcxproj:
        continue
    text = vcx.read_text(encoding="utf-8", errors="replace")
    for m in re.finditer(r'Include="((?:\.\.\\)+(?:[^"\\]+\\)*)', text):
        p = m.group(1).rstrip("\\").replace("\\", "/")
        prefixes.add(p)

missing = []
for prefix in sorted(prefixes):
    if prefix in deprecated_prefixes:
        continue
    if prefix == "..":
        resolved = msvc_dir.parent  # win32/
    else:
        resolved = (msvc_dir / prefix).resolve()
    if not resolved.exists():
        missing.append(f"{prefix} -> {resolved}")

if missing:
    print("FAIL: MSVC vcxproj path prefixes do not resolve:", file=sys.stderr)
    for line in missing:
        print(f"  {line}", file=sys.stderr)
    sys.exit(1)

print(f"test_msvc_layout_bridge: {len(prefixes)} prefixes OK")
PY
