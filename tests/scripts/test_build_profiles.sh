#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

fail() { echo "FAIL: $*" >&2; exit 1; }

PROFILE_DIR="${ROOT}/cmake/profiles"
APPLY="${ROOT}/cmake/IdTech3Profile.cmake"

[ -f "$APPLY" ] || fail "missing IdTech3Profile.cmake"

grep -q 'idtech3_apply_build_profile' "$APPLY" || fail "profile apply function missing"
grep -q 'idtech3_gate_append' "$ROOT/cmake/IdTech3Extension.cmake" || fail "IdTech3Extension.cmake missing gate helper"

for p in core game full research; do
  [ -f "${PROFILE_DIR}/${p}.cmake" ] || fail "missing profile ${p}.cmake"
done

# core: research + open world off in profile cmake
grep -q 'USE_OPEN_WORLD OFF' "$APPLY" || fail "core profile must disable USE_OPEN_WORLD"
grep -q 'USE_RESEARCH_EXTENSIONS OFF' "$APPLY" || fail "game/core must disable research in profile function"

# full enables research + generative
grep -q 'USE_FLUX ON' "$APPLY" || fail "full profile must enable USE_FLUX"
grep -q 'USE_EXPERIMENTAL_RENDERERS ON' "$APPLY" || fail "full profile must enable experimental renderers"

# Qcommon extension paths only behind USE_RESEARCH_EXTENSIONS
QC="${ROOT}/cmake/IdTech3QcommonExtensions.cmake"
grep -q 'if(USE_RESEARCH_EXTENSIONS)' "$QC" || fail "research qcommon not gated"
grep -q 'radiusfps/radiusfps_cpu' "$QC" || fail "research paths not in QcommonExtensions (Phase 5b)"
grep -qE 'src/extensions/research|IDTECH3_DIR_EXTENSIONS}/research' "$QC" || fail "research paths not in QcommonExtensions"
grep -q 'neural/vk_niv.c' "${ROOT}/cmake/renderers/VulkanExtensionSources.cmake" || fail "vulkan neural extension paths missing"

# Client generative under extensions/
CE="${ROOT}/cmake/client/ClientExtensionSources.cmake"
grep -q 'generative/cl_flux.c' "$CE" || fail "client flux path missing (Phase 5b)"
grep -q 'idtech3_strip_client_extension_sources' "$CE" || fail "client strip macro missing"
grep -q 'idtech3_legacy_src' "${ROOT}/cmake/IdTech3Layout.cmake" || fail "IdTech3Layout missing idtech3_legacy_src"
grep -q 'IDTECH3_DIR_RUNTIME_CLIENT' "${ROOT}/cmake/client/ClientSources.cmake" || fail "ClientSources must use IDTECH3_DIR_RUNTIME_CLIENT (Phase 5b)"
grep -q 'idtech3_legacy_src' "${ROOT}/cmake/client/ClientExtensionSources.cmake" || fail "ClientExtensionSources must use idtech3_legacy_src (AUX-safe)"
grep -q 'IDTECH3_DIR_RENDERERS' "${ROOT}/cmake/renderers/VulkanExtensionSources.cmake" || fail "VulkanExtensionSources must use IDTECH3_DIR_RENDERERS for includes"
grep -q 'idtech3_require_layout' "${ROOT}/cmake/IdTech3Layout.cmake" || fail "IdTech3Layout missing idtech3_require_layout"
grep -q 'idtech3_strip_game_ai_middleware_sources' "${ROOT}/cmake/modules/ClientGameAiSources.cmake" || fail "game AI middleware cmake missing"

# compile_engine.sh accepts profile args
CE_SH="${ROOT}/scripts/compile_engine.sh"
grep -q 'IDTECH3_PROFILE' "$CE_SH" || fail "compile_engine.sh missing IDTECH3_PROFILE wiring"

# Default profile game in CMakeLists
grep -q 'IDTECH3_PROFILE "game"' "${ROOT}/CMakeLists.txt" || fail "CMake default profile not game"

# 2026 samples alias (symlink to examples/)
[ -e "${ROOT}/samples" ] || fail "missing samples/ alias (symlink to examples/)"
[ -d "${ROOT}/third_party" ] || fail "missing physical third_party/ tree (Phase 5c)"
[ -f "${ROOT}/cmake/IdTech3Layout.cmake" ] || fail "missing IdTech3Layout.cmake"
[ -d "${ROOT}/runtime/client" ] || fail "missing physical runtime/client (Phase 5c)"
[ -d "${ROOT}/engine/core" ] || fail "missing physical engine/core (Phase 5c)"
grep -q 'idtech3_init_qcommon_sources' "${ROOT}/cmake/EngineQcommonSources.cmake" || fail "EngineQcommonSources.cmake missing init macro"
grep -q 'idtech3_init_server_sources' "${ROOT}/cmake/server/ServerSources.cmake" || fail "ServerSources.cmake missing init macro"
grep -q 'idtech3_init_vulkan_core_sources' "${ROOT}/cmake/renderers/VulkanCoreSources.cmake" || fail "VulkanCoreSources.cmake missing init macro"
grep -q 'idtech3_init_botlib_sources' "${ROOT}/cmake/modules/BotlibSources.cmake" || fail "BotlibSources.cmake missing init macro"
grep -q 'idtech3_init_qcommon_sources()' "${ROOT}/CMakeLists.txt" || fail "CMakeLists must call idtech3_init_qcommon_sources()"

"${ROOT}/tests/scripts/test_no_aux_core.sh"
"${ROOT}/tests/scripts/test_repository_layout_2026.sh"
[ -x "${ROOT}/scripts/layout_forwarding_symlinks.sh" ] || fail "missing layout_forwarding_symlinks.sh (Phase 5c)"

echo "test_build_profiles: passed"
