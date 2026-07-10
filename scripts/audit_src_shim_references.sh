#!/usr/bin/env bash
# Audit remaining src/* shim path references before Phase 5e shim removal.
# Reports counts; does not fail unless --strict and thresholds exceeded.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STRICT=0
if [[ "${1:-}" == "--strict" ]]; then
	STRICT=1
fi

count_matches() {
	local pattern="$1"
	local paths=("${@:2}")
	rg -l "${pattern}" "${paths[@]}" 2>/dev/null | wc -l || true
}

count_lines() {
	local pattern="$1"
	local paths=("${@:2}")
	rg -c "${pattern}" "${paths[@]}" 2>/dev/null | awk -F: '{s+=$2} END {print s+0}' || echo 0
}

CMAKE_FILES=("${ROOT}/CMakeLists.txt" "${ROOT}/cmake")
DOC_EXCLUDE=(--glob '!docs/**' --glob '!**/*.md' --glob '!**/msvc2017/**')

echo "[shim_audit] src/* path references (CMake + scripts + tests)..."

SHIM_QCOMMON=$(count_lines 'src/qcommon' "${CMAKE_FILES[@]}")
CANON_QCOMMON=$(count_lines 'engine/core' "${CMAKE_FILES[@]}")
SHIM_CLIENT=$(count_lines 'src/client' "${CMAKE_FILES[@]}")
CANON_CLIENT=$(count_lines 'runtime/client' "${CMAKE_FILES[@]}")
SHIM_SERVER=$(count_lines 'src/server' "${CMAKE_FILES[@]}")
CANON_SERVER=$(count_lines 'runtime/server' "${CMAKE_FILES[@]}")
SHIM_WORLD=$(count_lines 'src/world' "${CMAKE_FILES[@]}")
CANON_WORLD=$(count_lines 'modules/world' "${CMAKE_FILES[@]}")
SHIM_RENDERERS=$(count_lines 'src/renderers' "${CMAKE_FILES[@]}")
CANON_RENDERERS=$(count_lines 'renderers/' "${CMAKE_FILES[@]}")

printf '  qcommon:  src=%s  engine/core=%s\n' "$SHIM_QCOMMON" "$CANON_QCOMMON"
printf '  client:   src=%s  runtime/client=%s\n' "$SHIM_CLIENT" "$CANON_CLIENT"
printf '  server:   src=%s  runtime/server=%s\n' "$SHIM_SERVER" "$CANON_SERVER"
printf '  world:    src=%s  modules/world=%s\n' "$SHIM_WORLD" "$CANON_WORLD"
printf '  renderers src=%s  renderers/=%s\n' "$SHIM_RENDERERS" "$CANON_RENDERERS"

TOTAL_SHIM=$((SHIM_QCOMMON + SHIM_CLIENT + SHIM_SERVER + SHIM_WORLD + SHIM_RENDERERS))
TOTAL_CANON=$((CANON_QCOMMON + CANON_CLIENT + CANON_SERVER + CANON_WORLD + CANON_RENDERERS))
if [[ "$TOTAL_CANON" -gt 0 ]]; then
	# Integer percent: canonical share of (shim + canonical) CMake line refs.
	MIG_PCT=$((TOTAL_CANON * 100 / (TOTAL_SHIM + TOTAL_CANON)))
else
	MIG_PCT=0
fi
printf '  migration progress: cmake canonical=%s%% (shim_lines=%s canon_lines=%s)\n' \
	"$MIG_PCT" "$TOTAL_SHIM" "$TOTAL_CANON"

# Tests/scripts may legitimately use src/* until shims drop (paths resolve via symlink).
TEST_SHIM=$(count_lines 'src/(qcommon|client|server|world|renderers)' "${ROOT}/tests")
printf '  tests/scripts src/* refs: %s lines\n' "$TEST_SHIM"

echo "[shim_audit] physical layout..."
[ -d "${ROOT}/engine/core" ] && [ ! -L "${ROOT}/engine/core" ] || { echo "FAIL: engine/core not physical" >&2; exit 1; }
[ -L "${ROOT}/src/qcommon" ] || { echo "FAIL: src/qcommon shim missing" >&2; exit 1; }

if [[ "$STRICT" -eq 1 ]]; then
	# Post Phase 5e-prep: CMake manifests use canonical roots; residual src/* is rare.
	MAX_SHIM_CMAKE=50
	if [[ "$TOTAL_SHIM" -gt "$MAX_SHIM_CMAKE" ]]; then
		echo "FAIL: CMake src/* refs ($TOTAL_SHIM) exceed strict budget ($MAX_SHIM_CMAKE)" >&2
		echo "       Migrate cmake manifests to engine/runtime/modules paths before dropping shims." >&2
		exit 1
	fi
fi

echo "[shim_audit] OK (use --strict to enforce CMake migration budget)"
