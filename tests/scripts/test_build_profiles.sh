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
grep -q 'src/extensions/research/radiusfps' "$QC" || fail "research paths not in QcommonExtensions"

# Client generative under extensions/
CE="${ROOT}/cmake/client/ClientExtensionSources.cmake"
grep -q 'src/extensions/generative/cl_flux.c' "$CE" || fail "client flux path missing"
grep -q 'idtech3_strip_client_extension_sources' "$CE" || fail "client strip macro missing"

# compile_engine.sh accepts profile args
CE_SH="${ROOT}/scripts/compile_engine.sh"
grep -q 'IDTECH3_PROFILE' "$CE_SH" || fail "compile_engine.sh missing IDTECH3_PROFILE wiring"

# Default profile game in CMakeLists
grep -q 'IDTECH3_PROFILE "game"' "${ROOT}/CMakeLists.txt" || fail "CMake default profile not game"

# 2026 samples alias (symlink to examples/)
[ -e "${ROOT}/samples" ] || fail "missing samples/ alias (symlink to examples/)"
[ -e "${ROOT}/third_party" ] || fail "missing third_party/ alias (symlink to src/external/)"
grep -q 'BUILD_SAMPLES_DEMO_GAME' "${ROOT}/CMakeLists.txt" || fail "BUILD_SAMPLES_DEMO_GAME option missing"

echo "test_build_profiles: passed"
