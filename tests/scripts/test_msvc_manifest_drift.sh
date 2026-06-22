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

echo "test_msvc_manifest_drift: passed"
