#!/usr/bin/env bash
# Regenerate MSVC manifest from CMake and sync all hand-maintained vcxproj files.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${1:-${ROOT}/build-msvc-sync}"
MSVC_PY="${ROOT}/scripts/msvc"

MANIFEST="$("${ROOT}/scripts/generate_msvc_source_manifest.sh" "${BUILD}")"
export PYTHONPATH="${MSVC_PY}"

for project in quake3e quake3e-ded botlib vulkan; do
	echo "[msvc_sync] ${project}..."
	python3 "${MSVC_PY}/sync_vcxproj_sources.py" \
		--manifest "${MANIFEST}" \
		--project "${project}" \
		--write
done

"${ROOT}/tests/scripts/test_msvc_manifest_drift.sh"
"${ROOT}/tests/scripts/test_msvc_vcxproj_paths_resolve.sh"
echo "[msvc_sync] all projects OK"
