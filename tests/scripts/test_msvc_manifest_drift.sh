#!/usr/bin/env bash
# Phase 5d: CMake manifest vs MSVC vcxproj overlap coverage.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="${ROOT}/build-msvc-drift"
MSVC_PY="${ROOT}/scripts/msvc"

"${ROOT}/scripts/layout_forwarding_symlinks.sh" >/dev/null

cmake -S "${ROOT}" -B "${BUILD}" \
	-G Ninja \
	-DIDTECH3_PROFILE=game \
	-DIDTECH3_EXPORT_MSVC_MANIFEST=ON \
	-DSKIP_IDPAK_CHECK=ON \
	>/dev/null

MANIFEST="${BUILD}/msvc_source_manifest.json"
[ -f "${MANIFEST}" ] || { echo "missing manifest" >&2; exit 1; }

export PYTHONPATH="${MSVC_PY}"
for project in quake3e quake3e-ded botlib vulkan; do
	python3 "${MSVC_PY}/check_vcxproj_drift.py" "${ROOT}" \
		--manifest "${MANIFEST}" \
		--project "${project}"
done

# Non-shrink guard: dry-run sync must not propose fewer ClCompile rows than on disk
# (realpath dedupe is off by default; this catches regressions if it is re-enabled).
for project in quake3e quake3e-ded botlib vulkan; do
	vcx="${ROOT}/engine/platform/win32/msvc2017/${project}.vcxproj"
	before="$(rg -c '<ClCompile Include="' "${vcx}" || true)"
	tmp="$(mktemp)"
	cp "${vcx}" "${tmp}"
	# Simulate write into a copy via PYTHONPATH helper: count after collect_missing only.
	after="$(python3 - "${ROOT}" "${MANIFEST}" "${project}" "${tmp}" <<'PY'
import json, re, sys
from pathlib import Path
sys.path.insert(0, str(Path(sys.argv[1]) / "scripts" / "msvc"))
from sync_vcxproj_sources import (
	collect_missing,
	count_clcompile,
	parse_clcompile_text,
	write_manifest_block,
)
root = Path(sys.argv[1])
manifest = json.loads(Path(sys.argv[2]).read_text(encoding="utf-8"))
project = sys.argv[3]
text = Path(sys.argv[4]).read_text(encoding="utf-8")
before = count_clcompile(text)
existing = parse_clcompile_text(text)
missing = collect_missing(root, manifest, project, existing)
text2 = write_manifest_block(text, missing)
after = count_clcompile(text2)
print(after)
if after < before:
	raise SystemExit(f"{project}: ClCompile would shrink {before} -> {after}")
PY
)"
	rm -f "${tmp}"
	echo "sync non-shrink ${project}: ClCompile ${before} -> ${after} OK"
done

echo "test_msvc_manifest_drift: passed"
