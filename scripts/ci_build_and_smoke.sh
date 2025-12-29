#!/usr/bin/env bash
# CI helper: build Release and run smoke tests across renderers and mods.
# Env:
#   BUILD_TYPE     - Debug|Release (default: Release)
#   ENGINE_BIN     - optional override; if unset uses build/idtech3.x86_64 after build
#   MOD_LIST       - passed to ci_smoke.sh (default there: "mymod blacksun")
#   RENDERER_LIST  - passed to ci_smoke.sh (default there: "opengl vulkan")
#   SMOKE_TIMEOUT  - passed to ci_smoke.sh (default there: 10)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_TYPE="${BUILD_TYPE:-Release}"

echo "[ci] Building (${BUILD_TYPE})..."
pushd "${ROOT}" >/dev/null
./tools/compile_engine.sh "${BUILD_TYPE}"
popd >/dev/null

# ENGINE_BIN may point to release or build; if unset, default to build output
ENGINE_BIN="${ENGINE_BIN:-$ROOT/build/idtech3.x86_64}"

echo "[ci] Running smoke..."
ENGINE_BIN="${ENGINE_BIN}" "${ROOT}/tools/ci_smoke.sh"

echo "[ci] Done."

