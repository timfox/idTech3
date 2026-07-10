#!/usr/bin/env bash
# Wiring test: 2026 client domain folders + explicit ClientSources.cmake manifest.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT/tests/scripts/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"
fail() { echo "FAIL: $*" >&2; exit 1; }

CS="${ROOT}/cmake/client/ClientSources.cmake"
[ -f "$CS" ] || fail "missing ClientSources.cmake"

rg -q 'IDTECH3_CLIENT_CORE_SRCS' "$CS" || fail "core manifest missing"
rg -q 'core/cl_main.c' "$CS" || fail "cl_main not in core manifest"
rg -q '(src/client/world/|runtime/client/world/|IDTECH3_DIR_RUNTIME_CLIENT)' "${ROOT}/cmake/client/ClientExtensionSources.cmake" \
	|| fail "world paths in extension cmake"

for d in core world media platform; do
	[ -d "${IDTECH3_CLIENT}/$d" ] || fail "missing ${IDTECH3_CLIENT_REL}/$d"
done

[ -f "${IDTECH3_CLIENT}/README.md" ] || fail "missing ${IDTECH3_CLIENT_REL}/README.md"

# CMake must not use bare AUX on src/client
if rg -q 'AUX_SOURCE_DIRECTORY\(src/client' "${ROOT}/CMakeLists.txt"; then
	fail "CMakeLists still AUX_SOURCE_DIRECTORY src/client"
fi

echo "test_client_domains: passed"
