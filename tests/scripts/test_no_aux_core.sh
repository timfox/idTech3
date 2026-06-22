#!/usr/bin/env bash
# Wiring test: engine domains no longer use AUX_SOURCE_DIRECTORY (Phase 5b).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
fail() { echo "FAIL: $*" >&2; exit 1; }

CM="${ROOT}/CMakeLists.txt"

echo "[test_no_aux_core] checking CMakeLists.txt..."

for aux in \
	'src/qcommon' \
	'src/server' \
	'src/renderers/vulkan' \
	'src/botlib' \
	'src/audio/mp3'; do
	if rg -q "AUX_SOURCE_DIRECTORY\\(${aux}" "$CM"; then
		fail "CMakeLists still AUX_SOURCE_DIRECTORY ${aux}"
	fi
done

for f in \
	cmake/EngineQcommonSources.cmake \
	cmake/server/ServerSources.cmake \
	cmake/renderers/VulkanCoreSources.cmake \
	cmake/modules/BotlibSources.cmake \
	cmake/modules/AudioMp3Sources.cmake; do
	[ -f "${ROOT}/${f}" ] || fail "missing ${f}"
done

rg -q 'idtech3_init_qcommon_sources' "${ROOT}/cmake/EngineQcommonSources.cmake" || fail "qcommon init macro missing"
rg -q 'idtech3_init_server_sources' "${ROOT}/cmake/server/ServerSources.cmake" || fail "server init macro missing"
rg -q 'idtech3_init_vulkan_core_sources' "${ROOT}/cmake/renderers/VulkanCoreSources.cmake" || fail "vulkan init macro missing"
rg -q 'idtech3_init_botlib_sources' "${ROOT}/cmake/modules/BotlibSources.cmake" || fail "botlib init macro missing"
rg -q 'idtech3_append_mp3_client_sources' "${ROOT}/cmake/modules/AudioMp3Sources.cmake" || fail "mp3 append macro missing"
rg -q 'idtech3_glob_src_rel' "${ROOT}/cmake/IdTech3Layout.cmake" || fail "idtech3_glob_src_rel missing"

rg -q 'idtech3_init_qcommon_sources\(\)' "$CM" || fail "CMakeLists must call idtech3_init_qcommon_sources()"
rg -q 'idtech3_init_server_sources\(\)' "$CM" || fail "CMakeLists must call idtech3_init_server_sources()"
rg -q 'idtech3_init_vulkan_core_sources\(\)' "$CM" || fail "CMakeLists must call idtech3_init_vulkan_core_sources()"
rg -q 'idtech3_init_botlib_sources\(\)' "$CM" || fail "CMakeLists must call idtech3_init_botlib_sources()"
rg -q 'idtech3_append_mp3_client_sources\(\)' "$CM" || fail "CMakeLists must call idtech3_append_mp3_client_sources()"

# JPEG stays on AUX (external tree path variable)
if ! rg -q 'AUX_SOURCE_DIRECTORY\(\$\{EXTERNAL_JPEG_SRC_DIR\}' "$CM"; then
	fail "expected JPEG AUX on EXTERNAL_JPEG_SRC_DIR (vendored path)"
fi

echo "test_no_aux_core: passed"
