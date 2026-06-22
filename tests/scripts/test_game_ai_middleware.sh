#!/usr/bin/env bash
# Wiring test: USE_GAME_AI_MIDDLEWARE CMake gate + core profile stubs.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
fail() { echo "FAIL: $*" >&2; exit 1; }

GM="${ROOT}/cmake/modules/ClientGameAiSources.cmake"
[ -f "$GM" ] || fail "missing ClientGameAiSources.cmake"

rg -q 'USE_GAME_AI_MIDDLEWARE OFF' "${ROOT}/cmake/IdTech3Profile.cmake" || fail "core profile must disable middleware"
rg -q 'idtech3_strip_game_ai_middleware_sources' "$GM" || fail "strip macro missing"
rg -q 'game_middleware_stubs.c' "$GM" || fail "stubs when middleware OFF"
rg -q 'g_engine_systems.c' "${ROOT}/CMakeLists.txt" || fail "engine systems must stay in client list"

# Middleware sources not listed unconditionally in CMakeLists (use macro)
if rg -q 'list\(APPEND CLIENT_SRCS.*g_director\.c' "${ROOT}/CMakeLists.txt"; then
	fail "g_director.c must not be unconditional in CMakeLists"
fi

rg -q 'USE_GAME_AI_MIDDLEWARE' "${ROOT}/src/client/core/cl_gameframe.c" || fail "cl_gameframe guard missing"
rg -q 'GameMiddleware_LogDisabled' "${ROOT}/src/client/core/cl_gameframe.c" || fail "middleware disabled log hook"

echo "test_game_ai_middleware: passed"
