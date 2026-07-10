#!/usr/bin/env bash
# Legacy invariants: canonical src/ tree + QVM/compat wiring must survive 2026 reorg.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
fail() { echo "FAIL: $*" >&2; exit 1; }

if command -v rg >/dev/null 2>&1; then
	search_q() {
		rg -q "$1" "$2"
	}
else
	search_q() {
		grep -Eq "$1" "$2"
	}
fi

echo "[test_legacy_intact] canonical src tree..."

for d in qcommon client server game platform renderers world botlib cgame ui asm; do
	[ -e "${ROOT}/src/${d}" ] || fail "missing src/${d} forwarding shim"
done

# Phase 5c: physical roots + src shims (both must resolve)
[ -d "${ROOT}/runtime/client" ] && [ ! -L "${ROOT}/runtime/client" ] || fail "runtime/client must be physical"
[ -d "${ROOT}/engine/core" ] && [ ! -L "${ROOT}/engine/core" ] || fail "engine/core must be physical"
[ -L "${ROOT}/src/client" ] || fail "src/client must forward to runtime/client"
[ -L "${ROOT}/src/qcommon" ] || fail "src/qcommon must forward to engine/core"

echo "[test_legacy_intact] QVM + compat scripts..."

[ -f "${ROOT}/src/qcommon/vm.c" ] || fail "missing vm.c"
search_q 'Q3_VM' "${ROOT}/docs/COMPATIBILITY.md" || fail "COMPATIBILITY.md QVM section"
search_q 'vm\.c' "${ROOT}/docs/COMPATIBILITY.md" || fail "COMPATIBILITY.md vm.c reference"
[ -x "${ROOT}/scripts/q3_openarena_compat_check.sh" ] || fail "missing q3_openarena_compat_check.sh"

echo "[test_legacy_intact] deprecated CMake aliases..."

search_q 'BUILD_EXAMPLE_DEMO_GAME' "${ROOT}/CMakeLists.txt" || fail "BUILD_EXAMPLE_DEMO_GAME shim removed"
search_q 'BUILD_SAMPLES_DEMO_GAME' "${ROOT}/CMakeLists.txt" || fail "BUILD_SAMPLES_DEMO_GAME missing"

echo "[test_legacy_intact] legacy path references in build..."

search_q 'IDTECH3_DIR_RUNTIME_CLIENT|runtime/client|src/client' "${ROOT}/cmake/client/ClientSources.cmake" \
	|| fail "client manifest must use IDTECH3_DIR_RUNTIME_CLIENT, runtime/client, or src/client"
search_q 'IDTECH3_DIR_ENGINE_CORE|engine/core|src/qcommon' "${ROOT}/CMakeLists.txt" \
	|| fail "CMake must reference engine/core or src/qcommon"
search_q 'engine/core|src/qcommon' "${ROOT}/cmake/EngineQcommonSources.cmake" \
	|| fail "qcommon manifest must use engine/core or src/qcommon"

[ -f "${ROOT}/docs/core/LEGACY_AND_MODERN.md" ] || fail "missing LEGACY_AND_MODERN.md"
[ -x "${ROOT}/scripts/archive_legacy_remote_branches.sh" ] || fail "missing archive_legacy_remote_branches.sh"

echo "test_legacy_intact: passed"
