#!/usr/bin/env bash
# Legacy invariants: QVM/compat wiring must survive 2026 reorg (Phase 5e: no src/* shims).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "${BASH_SOURCE[0]}")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"

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

echo "[test_legacy_intact] canonical physical roots..."

[ -d "${ROOT}/runtime/client" ] && [ ! -L "${ROOT}/runtime/client" ] || fail "runtime/client must be physical"
[ -d "${ROOT}/engine/core" ] && [ ! -L "${ROOT}/engine/core" ] || fail "engine/core must be physical"
[ -d "${ROOT}/runtime/server" ] && [ ! -L "${ROOT}/runtime/server" ] || fail "runtime/server must be physical"
[ -d "${ROOT}/runtime/game" ] && [ ! -L "${ROOT}/runtime/game" ] || fail "runtime/game must be physical"
[ ! -e "${ROOT}/src/client" ] || fail "src/client shim must be gone (Phase 5e)"
[ ! -e "${ROOT}/src/qcommon" ] || fail "src/qcommon shim must be gone (Phase 5e)"

echo "[test_legacy_intact] QVM + compat scripts..."

VM="$(idtech3_file engine/core/vm.c src/qcommon/vm.c)"
[ -f "$VM" ] || fail "missing vm.c"
search_q 'Q3_VM' "${ROOT}/docs/COMPATIBILITY.md" || fail "COMPATIBILITY.md QVM section"
search_q 'vm\.c' "${ROOT}/docs/COMPATIBILITY.md" || fail "COMPATIBILITY.md vm.c reference"
[ -x "${ROOT}/scripts/q3_openarena_compat_check.sh" ] || fail "missing q3_openarena_compat_check.sh"

echo "[test_legacy_intact] deprecated CMake aliases..."

search_q 'BUILD_EXAMPLE_DEMO_GAME' "${ROOT}/CMakeLists.txt" || fail "BUILD_EXAMPLE_DEMO_GAME shim removed"
search_q 'BUILD_SAMPLES_DEMO_GAME' "${ROOT}/CMakeLists.txt" || fail "BUILD_SAMPLES_DEMO_GAME missing"

echo "[test_legacy_intact] legacy path references in build..."

search_q 'IDTECH3_DIR_RUNTIME_CLIENT|runtime/client' "${ROOT}/cmake/client/ClientSources.cmake" \
	|| fail "client manifest must use IDTECH3_DIR_RUNTIME_CLIENT or runtime/client"
search_q 'IDTECH3_DIR_ENGINE_CORE|engine/core' "${ROOT}/CMakeLists.txt" \
	|| fail "CMake must reference engine/core"
search_q 'engine/core' "${ROOT}/cmake/EngineQcommonSources.cmake" \
	|| fail "qcommon manifest must use engine/core"

[ -f "${ROOT}/docs/core/LEGACY_AND_MODERN.md" ] || fail "missing LEGACY_AND_MODERN.md"
[ -x "${ROOT}/scripts/archive_legacy_remote_branches.sh" ] || fail "missing archive_legacy_remote_branches.sh"

echo "test_legacy_intact: passed"
